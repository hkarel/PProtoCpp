/*****************************************************************************
  The MIT License

  Copyright © 2015 Pavel Karelin (hkarel), <hkarel@yandex.ru>

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
  ---

  В модуле представлена структура сообщения, которая используется для обмена
  сообщениями и данными между распределенными компонентами системы
*****************************************************************************/

#pragma once

#include "host_point.h"
#include "serialize/result.h"

#ifdef PPROTO_QBINARY_SERIALIZE
#include "serialize/qbinary.h"
#endif

#include "shared/list.h"
#include "shared/defmac.h"
#include "shared/clife_base.h"
#include "shared/clife_ptr.h"
#include "shared/qt/quuidex.h"

#include <QtCore>
#include <atomic>
#include <utility>

namespace pproto {

namespace transport {
namespace local {class Socket;}
namespace tcp   {class Socket;}
namespace udp   {class Socket;}
} // namespace transport

enum class SocketType : quint32
{
    Unknown = 0,
    Local   = 1,
    Tcp     = 2,
    Udp     = 3,
};

#if QT_VERSION >= 0x050000
typedef qintptr SocketDescriptor;
#else
typedef int SocketDescriptor;
#endif
typedef QSet<SocketDescriptor> SocketDescriptorSet;

enum class SerializeFormat
{
    QBinary = 0, // Qt-бинарный формат
    Json    = 1  // Json формат
  //LastFormat = 7  Предполагается, что будет не больше 8 форматов
};

class Message : public clife_base
{
    struct Allocator {void destroy(Message* x) {if (x) x->release();}};

public:
    typedef clife_ptr<Message> Ptr;
    typedef lst::List<Message, lst::CompareItemDummy, Allocator> List;

    enum class Type : quint32
    {
        Unknown = 0,
        Command = 1, // Сообщение-команда.  Это может быть сообщение с командой
                     // на выполнение  действия,  либо  запросом  на  получение
                     // данных. Предполагается, что в ответ на данное сообщение
                     // придет сообщение с типом Answer.
        Answer  = 2, // Ответ на сообщением с типом Command.
        Event   = 3  // Этот тип похож на Command, но не предполагает получения
                     // ответа (Answer), он используется для рассылки широкове-
                     // щательных сообщений о событиях
    };

    // Статус выполнения команды (обработки сообщения).  Используется в сообще-
    // ниях с типом Answer для того чтобы уведомить другую сторону о результате
    // выполнения команды с типом Command
    enum class ExecStatus : quint32
    {
        Unknown = 0,
        Success = 1, // Сообщение было обработано успешно и содержит корректные
                     // ответные данные.
        Failed  = 2, // Сообщение  не было  обработано  успешно,  но  результат
                     // не является ошибкой. Ответное сообщение (Message) будет
                     // содержать данные в формате  pproto::data::MessageFailed
                     // с описанием причины неудачи.
        Error   = 3  // При  обработке  сообщения  произошла  ошибка.  Ответное
                     // сообщение  (Message)  будет  содержать данные в формате
                     // pproto::data::MessageError с описанием  причины ошибки
    };

    enum class Priority : quint32
    {
        High   = 0,
        Normal = 1,
        Low    = 2
        // Reserved = 3
    };

    enum class Compression : quint32
    {
        None = 0,
        Zip  = 1,
        Lzma = 2,
        Ppmd = 3,
        Disable = 7 // Используется в тех случаях, когда нужно явно запретить
                    // сжатие сообщения при его отправке в TCP сокет.
                    // Это может оказаться полезным, когда контент сообщения
                    // уже сжат (JPG, PNG и пр. форматы)
    };

    // Идентификатор сообщения
    QUuidEx id() const {return _id;}

    // Идентификатор команды
    QUuidEx command() const {return _command;}

    // Функции возвращают нижнюю и верхнюю границы версий бинарного протокола.
    // Причины по которым версии протокола передаются вместе с сообщением:
    // 1) Изначально бинарный протокол и система сообщений была  спроектирована
    // для распределенной сети. Это означает, что сообщение могло быть трансли-
    // ровано через узлы имеющие различные версии протокола.  В этом случае ко-
    // ридор версий протокола должен сверяться на каждом узле. Если на каком-то
    // из узлов сети коридор версий окажется  не совместим с версией сообщения,
    // то в этом случае сообщение не должно обрабатываться этим узлом, а только
    // транслироваться к следующим узлам сети;
    // 2) Теоретически сообщение может быть передано из TCP/Local сокета в UDP
    // сокет для дальнейшего транслирования.  Для UDP сокета версия  протокола
    // должна всегда передаваться вместе с сообщением, это единственный способ
    // проверки совместимости протоколов
    quint16 protocolVersionLow()  const {return _protocolVersionLow;}
    quint16 protocolVersionHigh() const {return _protocolVersionHigh;}

    // Тип пересылаемого сообщения
    Type type() const;
    void setType(Type);

    // Статус выполнения/обработки сообщения
    ExecStatus execStatus() const;
    void setExecStatus(ExecStatus);

    // Приоритет сообщения
    Priority priority() const;
    void setPriority(Priority);

    // Используется для передачи списка пользовательских данных размером 8 байт
    // без сохранения их в поле  content.  Это позволяет  сократить  количество
    // ресурсоемких операций по сериализации/десериализации данных, выполняемых
    // для поля content. Максимальная длина списка составляет 255 элементов
    QVector<quint64> tags() const {return _tags;}
    void setTags(const QVector<quint64>& val);

    // Доступ к элементам списка tags
    quint64 tag(int index = 0) const;
    void setTag(quint64 val, int index = 0);

    // Максимальное время жизни сообщения.  Задается  в секундах  в формате UTC
    // от начала  эпохи.  Параметр  представляет  абсолютное  значение  времени,
    // по достижении которого сообщение перестает быть актуальным.
    // Тайм-аут в 2 мин. можно задать так: setMaxTimeLife(std::time(0) + 2*60)
    quint64 maxTimeLife() const {return _maxTimeLife;}
    void setMaxTimeLife(quint64 val) {_maxTimeLife = val;}

    // Тип сокета из которого было получено сообщение
    SocketType socketType() const {return _socketType;}

    // Адрес и порт хоста с которого было получено сообщение. Поле имеет
    // валидное значение только если тип сокета соответствует значениям
    // SocketType::Tcp или SocketType::Udp
    HostPoint sourcePoint() const {return _sourcePoint;}

    // Адреса и порты хостов  назначения.  Параметр  используется  для отправки
    // сообщения  через UDP сокет.  В случае  если  параметр  destinationPoints
    // не содержит ни одного элемента HostPoint и при этом параметр sourcePoint
    // не пуст (!isNull), то в этом случае  сообщение будет  отправлено  на UDP
    // сокет с точкой назначения sourcePoint
    HostPoint::Set& destinationPoints();

    // Добавляет HostPoint в коллекцию destinationPoints
    void appendDestinationPoint(const HostPoint&);

    // Вспомогательный параметр, используется на стороне TCP (или Local) сервера
    // для идентификации TCP (или Local) сокета принявшего сообщение.
    // Поле имеет валидное значение только если тип сокета соответствует значе-
    // ниям SocketType::Tcp или SocketType::Local
    SocketDescriptor socketDescriptor() const {return _socketDescriptor;}

    // Параметр содержит идентификаторы  сокетов  на  которые  нужно  отправить
    // сообщение.  Eсли  параметр  destinationSockets  не  содержит  ни  одного
    // идентификатора  и  при  этом  параметр  socketDescriptor  не  равен  -1,
    // то в этом случае сообщение будет отправлено  на сокет  с идентификатором
    // socketDescriptor
    SocketDescriptorSet& destinationSockets();

    // Добавляет идентификатор сокета в коллекцию destinationSockets
    void appendDestinationSocket(SocketDescriptor);

    // Наименование сокета  с которого  было  получено  сообщение.  Поле  имеет
    // валидное значение только если тип сокета соответствует SocketType::Local
    QString socketName() const {return _socketName;}

    // Вспомогательный параметр, используется для хранения произвольной инфор-
    // мации. Данный параметр не сериализуется, поэтому он не является частью
    // передаваемого сообщения.
    // Параметр  может  использоваться  в качестве  идентификатора  сообщения,
    // в ситуациях когда сообщение получено не из сокета, и socketDescriptor()
    // не может быть использован для этой цели
    qint64 auxiliary() const {return _auxiliary;}
    void setAuxiliary(qint64 val) {_auxiliary = val;}

    // Вспомогательный параметр, используется для того чтобы сообщить функциям-
    // обработчикам сообщений о том, что сообщение уже было  обработано  ранее.
    // Таким образом следующие обработчики могут проигнорировать это сообщение
    bool processed() const {return _processed;}
    void markAsProcessed() const {_processed = true;}

    // Возвращает информацию о том, в каком состоянии (сжатом или несжатом)
    // находится контент сообщения
    Compression compression() const;

    // Функция выполняет сжатие контента сообщения. Параметр level определяет
    // уровень сжатия контента. Допускаются значения  в диапазоне  от 0 до 9,
    // что соответствует уровням сжатия для zip-алгоритма.
    // Если  значение  level  равно -1,  то  будет  применен  уровень  сжатия
    // 'по умолчанию' для используемого алгоритма
    void compress(int level = -1, Compression compression = Compression::Zip);

    // Запрещает сжатие сообщения на уровне сетевого сокета
    void disableCompress() {compress(-1, Compression::Disable);}

    // Выполняет декомпрессию контента сообщения
    void decompress();

    // Возвращает контент в сыром виде. Если контент сжат, то предварительно
    // будет выполнена декомпрессия
    QByteArray content() const;

    // Удаляет контент сообщения
    void clearContent() {_content.clear();}

    // Возвращает TRUE если сообщение не содержит контент
    bool contentIsEmpty() const {return _content.isEmpty();}

    // Формат сериализации контента
    SerializeFormat contentFormat() const;

    // Создает сообщение. Формат сериализации контента необходимо устанавливать
    // при создании сообщения. Если этого не делать, то в случае  передачи пус-
    // того сообщения  (без контента)  значение параметра сериализации контента
    // может быть неверно  установлено,  что  приведет  неправильной  обработке
    // ответного сообщения
    static Ptr create(const QUuidEx& command, SerializeFormat contentFormat);

    // Создает отдельную  копию  сообщения  для  использования  ее  в  качестве
    // сообщения-ответа. Основная причина введения данной  функции  заключается
    // в том, что в многопоточном приложении нельзя менять параметры  исходного
    // сообщения.
    // Ряд параметров не клонируются и инициализируются значениями по умолчанию.
    //   - контент сообщения
    //   - destinationPoints
    //   - destinationSockets
    // Ряд параметров инициализируются предопределенными значениями:
    //   - type = Answer
    //   - execStatus = Success
    //   - compression = None
    Ptr cloneForAnswer() const;

    // !!! Экспериментальная функция !!!
    // Возвращает/устанавливает proxy-идентификатор необходимый для передачи
    // сообщения через промежуточный proxy-узел
    quint64 proxyId() const {return _proxyId;}
    void setProxyId(quint64 val) {_proxyId = val;}

    // !!! Экспериментальная функция !!!
    // Используется для интеграции протокола с системами авторизации, например
    // Keycloak. При сериализации сообщения в Json-формат поле  должно  содер-
    // жать данные в строковом представлении utf8. При сериализации  сообщения
    // в QBinary-формат поле может содержать  данные  в произвольном  бинарном
    // представлении
    QByteArray accessId() const {return _accessId;}
    void setAccessId(const QByteArray& val) {_accessId = val;}

#ifdef PPROTO_QBINARY_SERIALIZE
    // Функция записи данных
    template<typename... Args>
    SResult writeContent(const Args&... args);

    // Функция чтения данных
    template<typename... Args>
    SResult readContent(Args&... args) const;

    // Вспомогательные функции, используются для формирования сырого потока
    // данных для отправки в сетевой сокет
    QByteArray toQBinary() const;
    static Ptr fromQBinary(const QByteArray&);

    void toDataStream(QDataStream&) const;
    static Ptr fromDataStream(QDataStream&);
#endif

#ifdef PPROTO_JSON_SERIALIZE
    // Функция записи данных для json формата
    template<typename T>
    SResult writeJsonContent(const T&);

    // Функция чтения данных для json формата
    template<typename T>
    SResult readJsonContent(T&) const;

    QByteArray toJson(bool webFlags = false) const;
    static Ptr fromJson(const QByteArray&);
#endif

    // Возвращает максимально возможную длину сообщения в сериализованном виде.
    // Метод используется для оценки возможности передачи сообщения посредством
    // UDP датаграммы
    int size() const;

private:
    Message();
    DISABLE_DEFAULT_COPY(Message)

    void initNotEmptyTraits() const;
    void decompress(QByteArray&) const;

#ifdef PPROTO_QBINARY_SERIALIZE
    template<typename T, typename... Args>
    void writeInternal(QDataStream& s, const T& t, const Args&... args);
    void writeInternal(QDataStream&) {}

    template<typename T, typename... Args>
    void readInternal(QDataStream& s, T& t, Args&... args) const;
    void readInternal(QDataStream&) const {}
#endif

    void setSocketType(SocketType val) {_socketType = val;}
    void setSourcePoint(const HostPoint& val) {_sourcePoint = val;}
    void setSocketDescriptor(SocketDescriptor val) {_socketDescriptor = val;}
    void setSocketName(const QString& val) {_socketName = val;}

    void setContentFormat(SerializeFormat);

private:
    QUuidEx _id;
    QUuidEx _command;

    quint16 _protocolVersionLow  = {PPROTO_VERSION_LOW};
    quint16 _protocolVersionHigh = {PPROTO_VERSION_HIGH};

    // Битовые флаги
    // TODO: Проверить значения битовых флагов при пересылке сообщения
    //       из little-endian системы в big-endian систему.
    union {
        quint32 _flags; // Поле содержит значения всех флагов, используется
                        // при сериализации
        struct {
            //--- Байт 1 ---
            // Тип пересылаемого сообщения, соответствует enum Type
            quint32 type: 3;

            // Статус выполнения команды, соответствует enum ExecStatus
            quint32 execStatus: 3;

            // Приоритет сообщения, соответствует enum Priority
            quint32 priority: 2;

            //--- Байт 2 ---
            // Параметр определяет два признака:
            // 1) Контент сжат/не сжат;
            // 2) Алгоритм сжатия.
            // Параметр соответствует enum Compression
            quint32 compression: 3;

            // Признаки не пустых полей, используются для оптимизации размера
            // сообщения при его сериализации
            mutable quint32 tagsNotEmpty: 1;
            mutable quint32 maxTimeLfNotEmpty: 1;
            mutable quint32 contentNotEmpty: 1;
            mutable quint32 proxyIdNotEmpty: 1;
            mutable quint32 accessIdNotEmpty: 1;

            //--- Байт 3 ---
            quint32 reserved3: 8;

            //--- Байт 4 ---
            // Формат сериализации контента, соответствует enum SerializeFormat
            quint32 contentFormat: 3;
            quint32 reserved4: 4;

            // Признак не пустого флага _flags2, используется для оптимизации
            // размера сообщения при его сериализации. Признак идет последним
            // битом в поле _flags
            mutable quint32 flags2NotEmpty: 1;
        } _flag;
    };

    // Зарезервировано для будущего использования
    quint32 _flags2;

    QVector<quint64> _tags;
    quint64 _maxTimeLife = {quint64(-1)};
    quint64 _proxyId = {0};
    QByteArray _accessId;
    QByteArray _content;
    SocketType _socketType = {SocketType::Unknown};
    HostPoint _sourcePoint;
    HostPoint::Set _destinationPoints;
    SocketDescriptor _socketDescriptor = {-1};
    SocketDescriptorSet _destinationSockets;
    QString _socketName;
    qint64 _auxiliary = {0};
    mutable std::atomic_bool _processed = {false};

    friend class transport::local::Socket;
    friend class transport::tcp::Socket;
    friend class transport::udp::Socket;
};


//------------------------- Implementation Message ---------------------------

#ifdef PPROTO_QBINARY_SERIALIZE
template<typename... Args>
SResult Message::writeContent(const Args&... args)
{
    _content.clear();
    setContentFormat(SerializeFormat::QBinary);
    QDataStream stream {&_content, QIODevice::WriteOnly};
    STREAM_INIT(stream);
    writeInternal(stream, args...);
    return SResult(stream.status() == QDataStream::Ok);
}

template<typename... Args>
SResult Message::readContent(Args&... args) const
{
    if (contentIsEmpty())
        return SResult(false, 1, "Message content is empty");

    QByteArray content;
    decompress(content);
    QDataStream stream {content};
    STREAM_INIT(stream);
    readInternal(stream, args...);
    return SResult(stream.status() == QDataStream::Ok);
}

template<typename T, typename... Args>
void Message::writeInternal(QDataStream& s, const T& t, const Args&... args)
{
    if (s.status() != QDataStream::Ok)
        return;
    s << t;
    writeInternal(s, args...);
}

template<typename T, typename... Args>
void Message::readInternal(QDataStream& s, T& t, Args&... args) const
{
    // Не делаем здесь проверку условия s.atEnd() == TRUE, эта проверка
    // выполняется внутри потокового оператора для типа Т
    if (s.status() != QDataStream::Ok)
        return;
    s >> t;
    readInternal(s, args...);
}
#endif

#ifdef PPROTO_JSON_SERIALIZE
template<typename T>
SResult Message::writeJsonContent(const T& t)
{
    setContentFormat(SerializeFormat::Json);
    _content = const_cast<T&>(t).toJson();
    return SResult(true);
}

template<typename T>
SResult Message::readJsonContent(T& t) const
{
    return t.fromJson(_content);
}
#endif

/**
  Глобальная сервисная функция возвращает/устанавливает proxy-идентификатор
  для сетевого узла
*/
quint64 proxyId();
void setProxyId(quint64);

} // namespace pproto
