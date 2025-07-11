/*****************************************************************************
  The MIT License

  Copyright © 2017 Pavel Karelin (hkarel), <hkarel@yandex.ru>

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*****************************************************************************/

#include "transport/udp.h"

#include "commands/pool.h"
#include "logger_operators.h"
#include "utils.h"

#include "shared/break_point.h"
#include "shared/spin_locker.h"
#include "shared/safe_singleton.h"
#include "shared/logger/logger.h"
#include "shared/qt/logger_operators.h"
#include "shared/qt/stream_init.h"
#include "shared/qt/version_number.h"

#include <stdexcept>

#define log_error_m   alog::logger().error   (alog_line_location, "TransportUDP")
#define log_warn_m    alog::logger().warn    (alog_line_location, "TransportUDP")
#define log_info_m    alog::logger().info    (alog_line_location, "TransportUDP")
#define log_verbose_m alog::logger().verbose (alog_line_location, "TransportUDP")
#define log_debug_m   alog::logger().debug   (alog_line_location, "TransportUDP")
#define log_debug2_m  alog::logger().debug2  (alog_line_location, "TransportUDP")

namespace {
#ifdef PPROTO_UDP_LONGSIG
const quint64 udpSignature = *((quint64*)PPROTO_UDP_SIGNATURE);
#else
const quint32 udpSignature = *((quint32*)PPROTO_UDP_SIGNATURE);
#endif
} // namespace

namespace pproto::transport::udp {

Socket::Socket()
{
    registrationQtMetatypes();
}

bool Socket::init(const HostPoint& bindPoint, QUdpSocket::BindMode bindMode)
{
    if (isRunning())
    {
        log_error_m << "Impossible execute a initialization because Socket thread is running";
        return false;
    }
    _bindPoint = bindPoint;
    _bindMode = bindMode;
    return true;
}

void Socket::waitBinding(int timeout)
{
    if ((timeout <= 0) || isBound())
        return;

    timeout *= 1000;
    QElapsedTimer timer;
    timer.start();
    while (!timer.hasExpired(timeout))
    {
        if (threadStop())
            break;
        msleep(20);
        if (isBound())
            break;
    }
}

bool Socket::isBound() const
{
    bool res = false;
    if (_socketLock.tryLock(10))
    {
        if (_socket)
            res = (_socket->state() == QAbstractSocket::BoundState);
        _socketLock.unlock();
    }
    return res;
}

SocketDescriptor Socket::socketDescriptor() const
{
    SocketDescriptor res = -1;
    if (_socketLock.tryLock(10))
    {
        if (_socket)
            res = _socket->socketDescriptor();
        _socketLock.unlock();
    }
    return res;
}

QList<QHostAddress> Socket::discardAddresses() const
{
    SpinLocker locker {_discardAddressesLock}; (void) locker;
    return _discardAddresses;
}

void Socket::setDiscardAddresses(const QList<QHostAddress>& val)
{
    SpinLocker locker {_discardAddressesLock}; (void) locker;
    _discardAddresses = val;
}

void Socket::run()
{
    { //Block for QMutexLocker
        QMutexLocker locker {&_socketLock}; (void) locker;

        _socket = simple_ptr<QUdpSocket>(new QUdpSocket(nullptr));
        if (!_socket->bind(_bindPoint.address(), _bindPoint.port(), _bindMode))
        {
            log_error_m << "Failed bind UDP socket"
                        << ". Error code: " << int(_socket->error())
                        << ". Detail: " << _socket->errorString();
            return;
        }
    }
    log_debug_m << "UDP socket is successfully bound to point " << _bindPoint;

    Message::List internalMessages;
    Message::List acceptMessages;

    QElapsedTimer timer;
    bool loopBreak = false;
    const int delay = 50;

    #define CHECK_SOCKET_ERROR \
        if (_socket->state() != QAbstractSocket::BoundState) \
        { \
            log_error_m << "UDP socket error code: " << int(_socket->error()) \
                        << ". Detail: " << _socket->errorString(); \
            loopBreak = true; \
            break; \
        }

    try
    {
        while (true)
        {
            if (threadStop())
                break;

            CHECK_SOCKET_ERROR
            if (loopBreak)
                break;

            quint64 sleepCount = 0;
            while (messagesCount() == 0
                   && acceptMessages.empty()
                   && !_socket->hasPendingDatagrams())
            {
                if (threadStop())
                {
                    loopBreak = true;
                    break;
                }

                ++sleepCount;
                int condDelay = 1;

                // Меньшее значение интервала ожидания дает лучшие  результаты
                // при синхронной  передаче большого количества маленьких сооб-
                // щений,  но при этом  может  существенно  возрасти  нагрузка
                // на процессор в режиме ожидания
                if      (sleepCount > 400) condDelay = 10; // После 1000 ms
                else if (sleepCount > 300) condDelay = 5;  // После 500 ms
                else if (sleepCount > 200) condDelay = 3;  // После 200 ms

                { //Block for QMutexLocker
                    QMutexLocker locker {&_messagesLock}; (void) locker;
                    _messagesCond.wait(&_messagesLock, condDelay);
                }
            }
            if (loopBreak)
                break;

            QList<QHostAddress> discardAddresses;
            { //Block for SpinLocker
                SpinLocker locker {_discardAddressesLock}; (void) locker;
                discardAddresses = _discardAddresses;
            }

            //--- Отправка сообщений ---
            timer.start();
            while (true)
            {
                Message::Ptr message;
                if (!internalMessages.empty())
                    message.attach(internalMessages.release(0));

                if (message.empty()
                    && messagesCount() != 0)
                {
                    QMutexLocker locker {&_messagesLock}; (void) locker;

                    if (!_messagesHigh.empty())
                        message.attach(_messagesHigh.release(0));

                    if (message.empty() && !_messagesNorm.empty())
                    {
                        if (_messagesNormCounter < 5)
                        {
                            ++_messagesNormCounter;
                            message.attach(_messagesNorm.release(0));
                        }
                        else
                        {
                            _messagesNormCounter = 0;
                            if (!_messagesLow.empty())
                                message.attach(_messagesLow.release(0));
                            else
                                message.attach(_messagesNorm.release(0));
                        }
                    }
                    if (message.empty() && !_messagesLow.empty())
                        message.attach(_messagesLow.release(0));
                }
                if (loopBreak || message.empty())
                    break;

                if (alog::logger().level() == alog::Level::Debug2)
                {
                    log_debug2_m << "Message before sending to the UDP socket"
                                 << ". Id: " << message->id()
                                 << ". Command: " << CommandNameLog(message->command());
                }
                if (message->size() > 500)
                    log_warn_m << "Too large message to send it through a UDP socket"
                               << ". The message may be lost"
                               << ". Command: " << CommandNameLog(message->command());

                QByteArray buff;
                buff.reserve(message->size() + sizeof(udpSignature));
                {
                    QDataStream stream {&buff, QIODevice::WriteOnly};
                    STREAM_INIT(stream);
                    stream << udpSignature;
                    message->toDataStream(stream);
                }

                if (!message->destinationPoints().isEmpty())
                {
                    for (const HostPoint& dp : message->destinationPoints())
                        _socket->writeDatagram(buff, dp.address(), dp.port());

                    CHECK_SOCKET_ERROR
                    if (alog::logger().level() == alog::Level::Debug2)
                    {
                        alog::Line logLine =
                            log_debug2_m << "Message was sent to the next addresses:";
                        for (const HostPoint& dp : message->destinationPoints())
                            logLine << " " << dp;
                        logLine << ". Id: " << message->id();
                        logLine << ". Command: " << CommandNameLog(message->command());
                    }
                }
                else if (!message->sourcePoint().isNull())
                {
                    _socket->writeDatagram(buff,
                                           message->sourcePoint().address(),
                                           message->sourcePoint().port());
                    CHECK_SOCKET_ERROR
                    if (alog::logger().level() == alog::Level::Debug2)
                    {
                        log_debug2_m << "Message was sent to the address"
                                     << ": " << message->sourcePoint()
                                     << ". Id: " << message->id()
                                     << ". Command: " << CommandNameLog(message->command());
                    }
                }
                else
                {
                    log_error_m << "Impossible send message: " << CommandNameLog(message->command())
                                << ". Id: " << message->id()
                                << ". Destination host point is undefined"
                                << ". Message discarded";
                }
                if (loopBreak
                    || timer.hasExpired(3 * delay))
                    break;
            }
            if (loopBreak)
                break;

            //--- Прием сообщений ---
            timer.start();
            while (_socket->hasPendingDatagrams())
            {
                if (loopBreak
                    || timer.hasExpired(3 * delay))
                    break;

                // Функция pendingDatagramSize() некорректно работает в windows,
                // поэтому размер  датаграммы  проверяем  после  чтения  данных
                // из сокета
                // if (_socket->pendingDatagramSize() < qint64(sizeof(udpSignature)))
                //    continue;

                qint64 datagramSize = _socket->pendingDatagramSize();
                if (datagramSize < 0)
                    datagramSize = 0;

                QHostAddress addr;
                quint16 port;
                QByteArray datagram;
                datagram.resize(datagramSize);
                qint64 res = _socket->readDatagram((char*)datagram.constData(),
                                                   datagramSize, &addr, &port);
                if (res == -1)
                {
                    log_error_m << "Failed read datagram"
                                << ". Source: " << addr << ":" << port
                                << ". Error code: " << int(_socket->error())
                                << ". Detail: " << _socket->errorString();
                    continue;
                }
                if (datagramSize < qint64(sizeof(udpSignature)))
                {
                    log_error_m << "Datagram size less sizeof(udpSignature)"
                                << ". Source: " << addr << ":" << port;
                    continue;
                }
                if (res != datagramSize)
                {
                    log_error_m << "Failed datagram size"
                                << ". Source: " << addr << ":" << port;
                    continue;
                }
                if (alog::logger().level() == alog::Level::Debug2)
                {
                    log_debug2_m << "Raw message received"
                                 << ". Source: " << addr << ":" << port;
                }
                if (discardAddresses.contains(addr) && (port == _bindPoint.port()))
                {
                    if (alog::logger().level() == alog::Level::Debug2)
                    {
                        log_debug2_m << "Raw message discarded"
                                     << ". Source: " << addr << ":" << port;
                    }
                    continue;
                }

                Message::Ptr message;
                { //Block for QDataStream
                    QDataStream stream {&datagram, QIODevice::ReadOnly | QIODevice::Unbuffered};
                    STREAM_INIT(stream);
#ifdef UDP_LONGSIG
                    quint64 udpSign;
#else
                    quint32 udpSign;
#endif
                    stream >> udpSign;
                    if (udpSign != udpSignature)
                    {
                        if (alog::logger().level() == alog::Level::Debug2)
                        {
                            log_debug2_m << "Raw message incompatible signature, discarded"
                                         << ". Source: " << addr << ":" << port;
                        }
                        continue;
                    }
                    message = Message::fromDataStream(stream, datagram);
                }
                if (alog::logger().level() == alog::Level::Debug2)
                {
                    log_debug2_m << "Message received"
                                 << ". Id: " << message->id()
                                 << ". Command: " << CommandNameLog(message->command())
                                 << ". Source: " << addr << ":" << port;
                }
                message->setSocketType(SocketType::Udp);
                message->setSocketDescriptor(SocketDescriptor(-1));
                message->setSourcePoint({addr, port});
                acceptMessages.add(message.detach());
                CHECK_SOCKET_ERROR
            }
            if (loopBreak)
                break;

            //--- Обработка принятых сообщений ---
            timer.start();
            while (!acceptMessages.empty())
            {
                Message::Ptr m;
                m.attach(acceptMessages.release(0));

                if (_checkUnknownCommands)
                {
                    // Обработка уведомления о неизвестной команде
                    if (m->command() == command::Unknown)
                    {
                        data::Unknown unknown;
                        readFromMessage(m, unknown);
                        if (unknown.dataIsValid)
                        {
                            log_error_m << "Command " << CommandNameLog(unknown.commandId)
                                        << " is unknown for the remote side"
                                        << ". Remote host:" << unknown.address << ":" << unknown.port
                                        << ". Socket descriptor: " << unknown.socketDescriptor;

                            SpinLocker locker {_unknownCommandsLock}; (void) locker;
                            _unknownCommands.insert(unknown.commandId);
                        }
                        else
                            log_error_m << "Incorrect data structure for command "
                                        << CommandNameLog(m->command());
                        continue;
                    }

                    // Если команда неизвестна - отправляем об этом уведомление
                    // и переходим к обработке следующей команды.
                    if (!command::pool().commandExists(m->command()))
                    {
                        break_point
                        // Отладить

                        data::Unknown unknown;
                        unknown.commandId = m->command();
                        unknown.address = _socket->localAddress();
                        unknown.port = _socket->localPort();
                        unknown.socketDescriptor = _socket->socketDescriptor();
                        Message::Ptr mUnknown = createMessage(unknown);
                        mUnknown->setPriority(Message::Priority::High);
                        internalMessages.add(mUnknown.detach());
                        log_error_m << "Unknown command: " << unknown.commandId
                                    << ". Host: " << unknown.address << ":" << unknown.port
                                    << ". Socket descriptor: " << unknown.socketDescriptor;
                        continue;
                    }
                }

                try
                {
                    if (alog::logger().level() == alog::Level::Debug2)
                    {
                        log_debug2_m << "Message emit"
                                     << ". Id: " << m->id()
                                     << ". Command: " << CommandNameLog(m->command());
                    }
                    emit message(m);
                }
                catch (std::exception& e)
                {
                    log_error_m << "Failed processing a message. Detail: " << e.what();
                }
                catch (...)
                {
                    log_error_m << "Failed processing a message. Unknown error";
                }
                if (timer.hasExpired(3 * delay))
                    break;
            }
        } // while (true)

        { //Block for QMutexLocker
            QMutexLocker locker {&_socketLock}; (void) locker;
            _socket->close();
            _socket.reset();
        }
    }
    catch (std::exception& e)
    {
        log_error_m << "Detail: " << e.what();
    }
    catch (...)
    {
        log_error_m << "Unknown error";
    }

    #undef CHECK_SOCKET_ERROR
}

Socket& socket()
{
    return safe::singleton<Socket>();
}

} // namespace pproto::transport::udp
