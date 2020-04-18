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

#include "transport/local.h"

#include "logger_operators.h"
#include "utils.h"

#include "shared/break_point.h"
#include "shared/logger/logger.h"
#include "shared/qt/logger_operators.h"

#ifdef PPROTO_JSON_SERIALIZE
#include "serialize/json.h"
#endif

#include <stdexcept>
#include <unistd.h>

#define log_error_m   alog::logger().error  (__FILE__, __func__, __LINE__, "TransportSoc")
#define log_warn_m    alog::logger().warn   (__FILE__, __func__, __LINE__, "TransportSoc")
#define log_info_m    alog::logger().info   (__FILE__, __func__, __LINE__, "TransportSoc")
#define log_verbose_m alog::logger().verbose(__FILE__, __func__, __LINE__, "TransportSoc")
#define log_debug_m   alog::logger().debug  (__FILE__, __func__, __LINE__, "TransportSoc")
#define log_debug2_m  alog::logger().debug2 (__FILE__, __func__, __LINE__, "TransportSoc")

namespace communication {
namespace transport {
namespace local {

//-------------------------------- Socket ------------------------------------

bool Socket::init(const QString& serverName)
{
    if (isRunning())
    {
        log_error_m << "Impossible execute a initialization because Sender thread is running";
        return false;
    }
    _serverName = serverName;
    return true;
}

void Socket::socketCreate()
{
    _socket = simple_ptr<QLocalSocket>(new QLocalSocket(0));
    chk_connect_d(_socket.get(), SIGNAL(disconnected()), this, SLOT(socketDisconnected()))
}

bool Socket::socketInit()
{
    if (initSocketDescriptor() == -1)
    {
        log_verbose_m << "Try connect to socket " << _serverName;

        _socket->connectToServer(_serverName, QIODevice::ReadWrite);
        if (!_socket->waitForConnected(3 * 1000))
        {
            log_error_m << "Failed connect to socket " << _serverName
                        << ". Error code: " << int(_socket->error())
                        << ". Detail: " << _socket->errorString();
            return false;
        }
    }
    else
    {
        if (!_socket->setSocketDescriptor(initSocketDescriptor()))
        {
            log_error_m << "Failed set socket descriptor"
                        << ". Error code: " << int(_socket->error())
                        << ". Detail: " << _socket->errorString();
            return false;
        }
    }
    _serverName = _socket->serverName();
    _printSocketDescriptor = _socket->socketDescriptor();

    alog::Line logLine = log_verbose_m
        << "Connect to socket"
        << ". Socket descriptor: " << _printSocketDescriptor;
    if (!_serverName.isEmpty())
        logLine << ". Socket name: " << _serverName;

    return true;
}

bool Socket::isLocalInternal() const
{
    return true;
}

SocketDescriptor Socket::socketDescriptorInternal() const
{
    return (_socket) ? _socket->socketDescriptor() : -1;
}

bool Socket::socketIsConnectedInternal() const
{
    return (_socket
            && _socket->isValid()
            && _socket->state() == QLocalSocket::ConnectedState);
}

void Socket::printSocketError(const char* file, const char* func, int line,
                              const char* module)
{
    if (_socket->error() == QLocalSocket::PeerClosedError)
    {
        alog::Line logLine =
            alog::logger().verbose(file, func, line, "TransportSoc")
                << _socket->errorString()
                << ". Socket descriptor: " << _printSocketDescriptor;
        if (!_serverName.isEmpty())
            logLine << ". Socket name: " << _serverName;
    }
    else
    {
        alog::logger().error(file, func, line, module)
            << "Socket error code: " << int(_socket->error())
            << ". Detail: " << _socket->errorString();
    }
}

qint64 Socket::socketBytesAvailable() const
{
    return _socket->bytesAvailable();
}

qint64 Socket::socketBytesToWrite() const
{
    return _socket->bytesToWrite();
}

qint64 Socket::socketRead(char* data, qint64 maxlen)
{
    return _socket->read(data, maxlen);
}

qint64 Socket::socketWrite(const char* data, qint64 len)
{
    return _socket->write(data, len);
}

bool Socket::socketWaitForReadyRead(int msecs)
{
    return _socket->waitForReadyRead(msecs);
}

bool Socket::socketWaitForBytesWritten(int msecs)
{
    return _socket->waitForBytesWritten(msecs);
}

void Socket::socketClose()
{
    try
    {
        if (_socket->isValid()
            && _socket->state() != QLocalSocket::UnconnectedState)
        {
            log_verbose_m << "Disconnected from socket " << _socket->serverName()
                          << ". Socket descriptor: " << _socket->socketDescriptor();

            _socket->disconnectFromServer();
            if (_socket->state() != QLocalSocket::UnconnectedState)
                _socket->waitForDisconnected(1000);
        }
        _socket->close();
    }
    catch (std::exception& e)
    {
        log_error_m << "Detail: " << e.what();
        alog::logger().flush();
        alog::logger().waitingFlush();
    }
    catch (...)
    {
        log_error_m << "Unknown error";
        alog::logger().flush();
        alog::logger().waitingFlush();
    }
    _socket.reset();
}

void Socket::messageInit(Message::Ptr& message)
{
    message->setSocketType(SocketType::Local);
    message->setSocketDescriptor(_socket->socketDescriptor());
    message->setSocketName(_socket->serverName());
}

void Socket::fillUnknownMessage(const Message::Ptr& message, data::Unknown& unknown)
{
    unknown.commandId = message->command();
    unknown.socketType = SocketType::Local;
    unknown.socketDescriptor = _socket->socketDescriptor();
    unknown.socketName = _socket->serverName();
    unknown.address = QHostAddress();
    unknown.port = 0;
}

//------------------------------- Listener -----------------------------------

Listener::Listener()
{
    registrationQtMetatypes();
    chk_connect_q(&_removeClosedSockets, SIGNAL(timeout()),
                  this, SLOT(removeClosedSockets()))
}

bool Listener::init(const QString& serverName)
{
    _serverName = serverName;
    int attempt = 0;
    while (!QLocalServer::listen(_serverName))
    {
        if (++attempt > 10)
            break;
        usleep(200*1000);
    }
    if (attempt > 10)
        log_error_m << "Start listener of connection to " << _serverName
                    << " is failed. Detail: " << errorString();
    else
        log_verbose_m << "Start listener of connection to " << _serverName;

    _removeClosedSockets.start(15*1000);
    return (attempt <= 10);
}

void Listener::close()
{
    closeSockets();
    QLocalServer::close();
    log_verbose_m << "Stop listener of connection to " << _serverName;
}

void Listener::removeClosedSockets()
{
    removeClosedSocketsInternal();
}

void Listener::incomingConnection(quintptr socketDescriptor)
{
    Socket::Ptr socket {new Socket};
    incomingConnectionInternal(socket, SocketDescriptor(socketDescriptor));
}

void Listener::connectSignals(base::Socket* socket)
{
    chk_connect_d(socket, SIGNAL(message(communication::Message::Ptr)),
                  this, SIGNAL(message(communication::Message::Ptr)))

    chk_connect_d(socket, SIGNAL(connected(communication::SocketDescriptor)),
                  this, SIGNAL(socketConnected(communication::SocketDescriptor)))

    chk_connect_d(socket, SIGNAL(disconnected(communication::SocketDescriptor)),
                  this, SIGNAL(socketDisconnected(communication::SocketDescriptor)))
}

void Listener::disconnectSignals(base::Socket* socket)
{
    QObject::disconnect(socket, 0, this, 0);
}

Listener& listener()
{
    return ::safe_singleton<Listener>();
}

} // namespace local
} // namespace transport
} // namespace communication

#undef log_error_m
#undef log_warn_m
#undef log_info_m
#undef log_verbose_m
#undef log_debug_m
#undef log_debug2_m
