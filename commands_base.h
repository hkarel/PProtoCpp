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

  В модуле представлен базовый список идентификаторов команд для коммуникации
  между клиентской и серверной частями приложения.  Так же  модуль  содержит
  структуры данных соответствующие базовым командам.
  Фактически структуры описанные в модуле являются неким  подобием  публичного
  интерфейса.  И хотя для бинарного протокола обмена данными публичный  интер-
  фейс не является обязательным, его наличие облегчит ориентацию  в передавае-
  мых структурах данных.

  Требование надежности коммуникаций: однажды назначенный идентификатор
  команды не должен более меняться
*****************************************************************************/

#pragma once

#include "message.h"

#include "shared/logger/logger.h"
#include "shared/qt/quuidex.h"
#include "shared/qt/version_number.h"

#ifdef PPROTO_QBINARY_SERIALIZE
#include "bserialize_space.h"
#endif

#ifdef PPROTO_JSON_SERIALIZE
#include "serialize/json.h"
#endif

#include <QtCore>
#include <QHostAddress>

namespace communication {

//------------------------- Список базовых команд ----------------------------
namespace command {

/**
  Идентификатор неизвестной команды
*/
extern const QUuidEx Unknown;

/**
  Идентификатор сообщения об ошибке
*/
extern const QUuidEx Error;

/**
  Запрос информации о совместимости. При подключении клиент и сервер отправляют
  друг другу информацию о совместимости
*/
extern const QUuidEx ProtocolCompatible;

/**
  Требование закрыть TCP-соединение. Эта команда работает следующим образом:
  сторона, которая хочет закрыть соединение отправляет это сообщение с инфор-
  мацией о причине необходимости закрыть соединение.  Принимающая  сторона
  записывает эту информацию в свой лог (или использует иным образом), затем
  отправляет обратное пустое сообщение.  После того,  как ответное сообщение
  получено - TCP-соединение может быть разорвано. Такое поведение реализовано
  для того чтобы сторона с которой разрывают соединение  имела  информацию о
  причине разрыва
*/
extern const QUuidEx CloseConnection;

} // namespace command

//------------------------ Список базовых структур ---------------------------
namespace data {

/**
  Структура Data используется для ассоциации целевой структуры  данных с опре-
  деленной командой. Она позволяют связать идентификатор команды со структурой
  данных, а так же задать направления передачи данных.
  В дальнейшем эти параметры будут использоваться для проверки возможности
  преобразования Message-сообщения в конкретную структуру
*/
template<
    const QUuidEx* Command,
    Message::Type MessageType1,
    Message::Type MessageType2 = Message::Type::Unknown,
    Message::Type MessageType3 = Message::Type::Unknown
>
struct Data
{
    // Идентификатор команды
    static constexpr const QUuidEx& command() {return *Command;}

    // Статус состояния данных. Выставляется в TRUE когда данные были корректно
    // прочитаны из сообщения, во всех остальных случаях флаг должен быть FALSE
    bool dataIsValid = {false};

    // Признак, что данные могут быть использованы для Message-сообщений
    // с типом Command
    static constexpr bool forCommandMessage() {
        return (MessageType1 == Message::Type::Command
                || MessageType2 == Message::Type::Command
                || MessageType3 == Message::Type::Command);
    }

    // Признак, что данные могут быть использованы для Message-сообщений
    // с типом Answer
    static constexpr bool forAnswerMessage() {
        return (MessageType1 == Message::Type::Answer
                || MessageType2 == Message::Type::Answer
                || MessageType3 == Message::Type::Answer);
    }

    // Признак, что данные могут быть использованы для Message-сообщений
    // с типом Event
    static constexpr bool forEventMessage() {
        return (MessageType1 == Message::Type::Event
                || MessageType2 == Message::Type::Event
                || MessageType3 == Message::Type::Event);
    }

#ifdef PPROTO_QBINARY_SERIALIZE
    // Фиктивные функции, необходимы для вывода в лог сообщений об отсутствии
    // в целевой структуре функции сериализации toRaw(), fromRaw()
    bserial::RawVector toRawNone() const;
    void fromRawNone(const bserial::RawVector&);
#endif

#ifdef PPROTO_JSON_SERIALIZE
    // Фиктивные функции, необходимы для вывода в лог сообщений об отсутствии
    // в целевой структуре функции сериализации toJson(), fromJson()
    QByteArray toJsonNone();
    SResult fromJsonNone(const QByteArray&);
#endif
};

/**
  Структура содержит информацию об ошибке произошедшей в процессе обработки
  сообщения. Данная структура  отправляется  вызывающей стороне как Answer-
  сообщение, при этом статус обработки команды Message::ExecStatus
  равен Error.
  См. так же описание enum Message::ExecStatus
*/
struct MessageError
{
    qint32  group = {0};  // Используется для группировки сообщений по группам
    QUuidEx code;         // Глобальный код ошибки
    QString description;  // Описание ошибки (сериализуется в utf8)

    MessageError() = default;
    MessageError(const MessageError&) = default;

    MessageError(qint32 group, const QUuidEx& code, const QString& description);
    MessageError(qint32 group, const QUuidEx& code, const char* description);

    void assign(const MessageError& msg) {*this = msg;}

#ifdef PPROTO_QBINARY_SERIALIZE
    DECLARE_B_SERIALIZE_FUNC
#endif

#ifdef PPROTO_JSON_SERIALIZE
    J_SERIALIZE_BASE_BEGIN
        J_SERIALIZE_ITEM( group )
        J_SERIALIZE_ITEM( code  )
        J_SERIALIZE_ITEM( description )
    J_SERIALIZE_BASE_END
    J_SERIALIZE_BASE_ONE
#endif
};

/**
  Сообщение о неудачной обработке сообщения, которое не является ошибкой. Такая
  ситуация может возникнуть когда запрашиваемое действие не может быть выполне-
  но в силу разных причин, например, когда  недостаточно  прав на запрашиваемое
  действие. Данная структура отправляется вызывающей стороне как Answer-сообще-
  ние, при этом статус обработки команды Message::ExecStatus равен Failed.
  См. так же описание enum Message::ExecStatus
*/
struct MessageFailed
{
    qint32  group = {0};  // Используется для группировки сообщений по группам
    QUuidEx code;         // Глобальный код неудачи
    QString description;  // Описание неудачи (сериализуется в utf8)

    MessageFailed() = default;
    MessageFailed(const MessageFailed&) = default;

    MessageFailed(qint32 group, const QUuidEx& code, const QString& description);
    MessageFailed(qint32 group, const QUuidEx& code, const char* description);

    void assign(const MessageFailed& msg) {*this = msg;}

#ifdef PPROTO_QBINARY_SERIALIZE
    DECLARE_B_SERIALIZE_FUNC
#endif

#ifdef PPROTO_JSON_SERIALIZE
    J_SERIALIZE_BASE_BEGIN
        J_SERIALIZE_ITEM( group )
        J_SERIALIZE_ITEM( code  )
        J_SERIALIZE_ITEM( description )
    J_SERIALIZE_BASE_END
    J_SERIALIZE_BASE_ONE
#endif
};

/**
  Информационное сообщение о неизвестной команде
*/
struct Unknown : Data<&command::Unknown,
                       Message::Type::Command>
{
    // Идентификатор неизвестной команды
    QUuidEx commandId;

    // Тип сокета для которого было создано сообщение
    SocketType socketType = {SocketType::Unknown};

    // Идентификатор сокета
    quint64 socketDescriptor = {quint64(-1)};

    // Наименование локального сокета (сериализуется в utf8)
    QString socketName;

    // Адрес и порт хоста для которого команда неизвестна
    QHostAddress address;
    quint16 port = {0};

#ifdef PPROTO_QBINARY_SERIALIZE
    DECLARE_B_SERIALIZE_FUNC
#endif

#ifdef PPROTO_JSON_SERIALIZE
    DECLARE_J_SERIALIZE_FUNC
#endif
};

/**
  Сообщение об ошибке возникшей  на стороне сервера или клиента.  Если по какой
  либо причине невозможно передать сообщение при помощи MessageError, то исполь-
  зуется эта структура, причем как самостоятельное сообщение
*/
struct Error : Data<&command::Error,
                     Message::Type::Command>
{
    QUuidEx commandId;   // Идентификатор команды
    QUuidEx messageId;   // Идентификатор сообщения
    qint32  group = {0}; // Используется для группировки сообщений по группам
    QUuidEx code;        // Глобальный код ошибки
    QString description; // Описание ошибки (сериализуется в utf8)

#ifdef PPROTO_QBINARY_SERIALIZE
    DECLARE_B_SERIALIZE_FUNC
#endif

#ifdef PPROTO_JSON_SERIALIZE
    J_SERIALIZE_BEGIN
        J_SERIALIZE_ITEM( commandId   )
        J_SERIALIZE_ITEM( messageId   )
        J_SERIALIZE_ITEM( group       )
        J_SERIALIZE_ITEM( code        )
        J_SERIALIZE_ITEM( description )
    J_SERIALIZE_END
#endif
};

/**
  Структура содержит информацию о причинах закрытия TCP-соединения
*/
struct CloseConnection : Data<&command::CloseConnection,
                               Message::Type::Command>
{
    qint32  code = {0};   // Код причины. Нулевой код соответствует
                          // несовместимости версий протоколов.
    QString description;  // Описание причины закрытия соединения,
                          // (сериализуется в utf8)

#ifdef PPROTO_QBINARY_SERIALIZE
    DECLARE_B_SERIALIZE_FUNC
#endif

#ifdef PPROTO_JSON_SERIALIZE
    J_SERIALIZE_BEGIN
        J_SERIALIZE_ITEM( code )
        J_SERIALIZE_ITEM( description )
    J_SERIALIZE_END
#endif
};

//------------------------ Функции json-сериализации -------------------------

#ifdef PPROTO_JSON_SERIALIZE
template <typename This, typename Packer>
Packer& Unknown::jserialize(const This* ct, Packer& p)
{
    This* t = const_cast<This*>(ct);

    p.startObject();
    p.member("commandId")        & t->commandId;
    p.member("socketType")       & t->socketType;
    p.member("socketDescriptor") & t->socketDescriptor;
    p.member("socketName")       & t->socketName;

    QString addressProtocol = "ip4";
    QString addressString;
    QString addressScopeId;

    if (p.isWriter())
    {
        addressString = t->address.toString();
        if (t->address.protocol() == QAbstractSocket::IPv6Protocol)
        {
            addressProtocol = "ip6";
            addressScopeId = t->address.scopeId();
        }
    }
    p.member("addressProtocol") & addressProtocol;
    p.member("address")         & addressString;
    p.member("addressScopeId")  & addressScopeId;

    if (p.isReader())
    {
        t->address = QHostAddress(addressString);
        if (addressProtocol == "ip6")
            t->address.setScopeId(addressScopeId);
    }
    p.member("port") & t->port;
    return p.endObject();
}
#endif // PPROTO_JSON_SERIALIZE

} // namespace data

//----------------------- Механизм для описания ошибок -----------------------
namespace error {

/**
  Пул кодов ошибок, используется для проверки уникальности кодов ошибок
*/
QHash<QUuidEx, int>& pool();

/**
  Проверяет уникальность кодов ошибок
*/
bool checkUnique();

/**
  Trait используется в качестве маркера структуры содержащей описание ошибки
*/
struct Trait {};

#define DECL_ERROR_CODE(VAR, GROUP, CODE, DESCR) \
    struct VAR##__struct : data::MessageError, Trait { \
        VAR##__struct () : data::MessageError(GROUP, QUuidEx(CODE), DESCR) { \
            static bool b {false}; if (!b) {b = true; pool()[code]++;} \
        } \
        data::MessageFailed asFailed() const { \
            return data::MessageFailed(group, code, description); \
        } \
    } static const VAR;

//--------------- Список глобальных ошибок для сообщения Error ---------------

/**
  Ошибка парсинга контента сообщения
*/
extern const QUuidEx MessageContentParse;


} // namespace error
} // namespace communication


