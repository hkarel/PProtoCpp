/*****************************************************************************
  The MIT License

  Copyright © 2020 Pavel Karelin (hkarel), <hkarel@yandex.ru>

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

  В модуле собраны функции общего назначения для работы с коммуникационными
  механизмами
*****************************************************************************/

#pragma once

#include "commands/base.h"
#include "serialize/result.h"
#include "error_sender.h"
#include "logger_operators.h"

#include "shared/type_name.h"
#include "shared/prog_abort.h"
#include "shared/logger/logger.h"
#include "shared/logger/format.h"
#include "shared/qt/logger_operators.h"

#include <typeinfo>
#include <stdexcept>
#include <type_traits>

#define log_error_m   alog::logger().error   (alog_line_location, "Serialize")
#define log_warn_m    alog::logger().warn    (alog_line_location, "Serialize")
#define log_info_m    alog::logger().info    (alog_line_location, "Serialize")
#define log_verbose_m alog::logger().verbose (alog_line_location, "Serialize")
#define log_debug_m   alog::logger().debug   (alog_line_location, "Serialize")
#define log_debug2_m  alog::logger().debug2  (alog_line_location, "Serialize")

namespace pproto {
namespace detail {

template<typename CommandDataT>
struct derived_from_data_t
{
    template<
        const QUuidEx* Command,
        Message::Type MessageType1,
        Message::Type MessageType2,
        Message::Type MessageType3
    >
    static constexpr std::true_type test(const data::Data<Command,
                                                          MessageType1,
                                                          MessageType2,
                                                          MessageType3>&);
    static constexpr std::false_type test(...);
    using type = decltype(test(std::declval<CommandDataT>()));
};

template<typename CommandDataT>
using is_derived_from_data_t = typename derived_from_data_t<CommandDataT>::type;

template<typename T> using is_error_data =
typename std::enable_if<std::is_base_of<data::MessageError, T>::value, int>::type;

template<typename T> using is_failed_data =
typename std::enable_if<std::is_base_of<data::MessageFailed, T>::value, int>::type;

template<typename T> using not_error_data =
typename std::enable_if<!std::is_base_of<data::MessageError, T>::value
                     && !std::is_base_of<data::MessageFailed, T>::value, int>::type;

#ifdef PPROTO_QBINARY_SERIALIZE
template<typename CommandDataT>
SResult messageWriteQBinary(const CommandDataT& data, Message::Ptr& message,
                            is_error_data<CommandDataT> = 0)
{
    if (std::is_same<data::MessageError, CommandDataT>::value)
        return message->writeContent(data);

    if (std::is_base_of<error::Trait, CommandDataT>::value)
        return message->writeContent(data);

    // Вариант расширенного сообщения об ошибке, когда структура CommandDataT
    // унаследована от data::MessageError
    return message->writeContent(static_cast<const data::MessageError&>(data), data);
}

template<typename CommandDataT>
SResult messageWriteQBinary(const CommandDataT& data, Message::Ptr& message,
                            is_failed_data<CommandDataT> = 0)
{
    if (std::is_same<data::MessageFailed, CommandDataT>::value)
        return message->writeContent(data);

    // Вариант расширенного сообщения о неудаче, когда структура CommandDataT
    // унаследована от data::MessageFailed
    return message->writeContent(static_cast<const data::MessageFailed&>(data), data);
}

template<typename CommandDataT>
auto messageWriteQBinary(const CommandDataT& data, Message::Ptr& message, int, int)
     -> decltype(data.toRaw(), SResult())
{
    return message->writeContent(data);
}

template<typename CommandDataT>
auto messageWriteQBinary(const CommandDataT&, Message::Ptr&, long, long)
     -> SResult
{
    QString err = "Method %1::toRaw not exists";
    err = err.arg(abi_type_name<CommandDataT>().c_str());
    log_error_m << err;
    return SResult(false, 0, err);
}

template<typename CommandDataT>
SResult messageWriteQBinary(const CommandDataT& data, Message::Ptr& message,
                            not_error_data<CommandDataT> = 0)
{
    return messageWriteQBinary(data, message, 0, 0);
}
#endif // PPROTO_QBINARY_SERIALIZE

#ifdef PPROTO_JSON_SERIALIZE
template<typename CommandDataT>
auto messageWriteJson(CommandDataT& data, Message::Ptr& message, int)
     -> decltype(data.toJson(), SResult())
{
    return message->writeJsonContent(data);
}

template<typename CommandDataT>
auto messageWriteJson(CommandDataT&, Message::Ptr&, long)
     -> SResult
{
    QString err = "Method %1::toJson not exists";
    err = err.arg(abi_type_name<CommandDataT>().c_str());
    log_error_m << err;
    return SResult(false, 0, err);
}

template<typename CommandDataT>
SResult messageWriteJson(const CommandDataT& data, Message::Ptr& message)
{
    return messageWriteJson(const_cast<CommandDataT&>(data), message, 0);
}
#endif // PPROTO_JSON_SERIALIZE

template<typename CommandDataT>
SResult messageWriteContent(const CommandDataT& data, Message::Ptr& message,
                            SerializeFormat contentFormat)
{
    SResult res {false};
    switch (contentFormat)
    {
#ifdef PPROTO_QBINARY_SERIALIZE
        case SerializeFormat::QBinary:
            res = messageWriteQBinary(data, message);
            break;
#endif
#ifdef PPROTO_JSON_SERIALIZE
        case SerializeFormat::Json:
            res = messageWriteJson(data, message);
            break;
#endif
        default:
        {
            log_error_m << "Unsupported message serialize format: "
                        << contentFormat;
            prog_abort();
        }
    }
    return res;
}
} // namespace detail

struct CreateMessageParams
{
    const Message::Type   type   = {Message::Type::Command};
    const SerializeFormat format = {SerializeFormat::QBinary};

    CreateMessageParams() = default;
    CreateMessageParams(Message::Type type,
                        SerializeFormat format = SerializeFormat::QBinary)
        : type{type}, format{format}
    {}
    CreateMessageParams(SerializeFormat format,
                        Message::Type type = Message::Type::Command)
        : type{type}, format{format}
    {}
};

inline Message::Ptr createMessage(const QUuidEx& command)
{
    return Message::create(command, SerializeFormat::QBinary);
}

/**
  Создает сообщение на основе структуры данных соответствующей определнной
  команде. Структуры данных описаны в модулях commands_base и commands
*/
template<typename CommandDataT>
Message::Ptr createMessage(const CommandDataT& data,
                           const CreateMessageParams& params = CreateMessageParams())
{
    static_assert(detail::is_derived_from_data_t<CommandDataT>::value,
                  "CommandDataT must be derived from pproto::data::Data");

    static_assert(CommandDataT::forCommandMessage()
                  || CommandDataT::forEventMessage(),
                  "In this function is allow 'Message::Type::Command'"
                  " or 'Message::Type::Event' type of struct only");

    Message::Ptr message = Message::create(data.command(), params.format);

    if (params.type == Message::Type::Command)
    {
        if (!CommandDataT::forCommandMessage())
        {
            log_error_m << log_format(
                "Cannot create message %? with type 'Command' and data %?"
                ". Mismatched types",
                CommandNameLog(message->command()), abi_type_name<CommandDataT>());
            prog_abort();
        }
        message->setType(Message::Type::Command);
    }
    else if (params.type == Message::Type::Event)
    {
        if (!CommandDataT::forEventMessage())
        {
            log_error_m << log_format(
                "Cannot create message %? with type 'Event' and data %?"
                ". Mismatched types",
                CommandNameLog(message->command()), abi_type_name<CommandDataT>());
            prog_abort();
        }
        message->setType(Message::Type::Event);
    }
    else
    {
        log_error_m << log_format("Cannot create message %? with type '%?'",
                                  CommandNameLog(message->command()), params.type);
        prog_abort();
    }

    message->setExecStatus(Message::ExecStatus::Unknown);
    detail::messageWriteContent(data, message, params.format);
    return message;
}

namespace detail {

template<typename CommandDataPtr>
Message::Ptr createMessagePtr(const CommandDataPtr& data,
                              const CreateMessageParams& params)
{
    if (data.empty())
    {
        log_error_m << "Impossible create message from empty data";
#ifndef NDEBUG
        prog_abort();
#endif
        return {};
    }
    return createMessage(*data, params);
}
} // namespace detail

template<typename CommandDataT>
Message::Ptr createMessage(const clife_ptr<CommandDataT>& data,
                           const CreateMessageParams& params = CreateMessageParams())
{
    return detail::createMessagePtr(data, params);
}

template<typename CommandDataT>
Message::Ptr createMessage(const container_ptr<CommandDataT>& data,
                           const CreateMessageParams& params = CreateMessageParams())
{
    return detail::createMessagePtr(data, params);
}

#ifdef PPROTO_JSON_SERIALIZE
inline Message::Ptr createJsonMessage(const QUuidEx& command)
{
    return Message::create(command, SerializeFormat::Json);
}

template<typename CommandDataT>
Message::Ptr createJsonMessage(const CommandDataT& data,
                               Message::Type type = Message::Type::Command)
{
    return createMessage(data, {type, SerializeFormat::Json});
}

template<typename CommandDataT>
Message::Ptr createJsonMessage(const clife_ptr<CommandDataT>& data,
                               Message::Type type = Message::Type::Command)
{
    return createMessage(data, {type, SerializeFormat::Json});
}

template<typename CommandDataT>
Message::Ptr createJsonMessage(const container_ptr<CommandDataT>& data,
                               Message::Type type = Message::Type::Command)
{
    return createMessage(data, {type, SerializeFormat::Json});
}
#endif

namespace detail {

#ifdef PPROTO_QBINARY_SERIALIZE
template<typename CommandDataT>
SResult messageReadQBinary(const Message::Ptr& message, CommandDataT& data,
                           is_error_data<CommandDataT> = 0)
{
    if (std::is_same<data::MessageError, CommandDataT>::value)
        return message->readContent(data);

    return message->readContent(static_cast<data::MessageError&>(data), data);
}

template<typename CommandDataT>
SResult messageReadQBinary(const Message::Ptr& message, CommandDataT& data,
                           is_failed_data<CommandDataT> = 0)
{
    if (std::is_same<data::MessageFailed, CommandDataT>::value)
        return message->readContent(data);

    return message->readContent(static_cast<data::MessageFailed&>(data), data);
}

template<typename CommandDataT>
auto messageReadQBinary(const Message::Ptr& message, CommandDataT& data, int, int)
     -> decltype(data.fromRaw(bserial::RawVector()), SResult())
{
    return message->readContent(data);
}

template<typename CommandDataT>
auto messageReadQBinary(const Message::Ptr&, CommandDataT&, long, long)
     -> SResult
{
    QString err = "Method %1::fromRaw not exists";
    err = err.arg(abi_type_name<CommandDataT>().c_str());
    log_error_m << err;
    return SResult(false, 0, err);
}

template<typename CommandDataT>
SResult messageReadQBinary(const Message::Ptr& message, CommandDataT& data,
                           not_error_data<CommandDataT> = 0)
{
    return messageReadQBinary(message, data, 0, 0);
}
#endif // PPROTO_QBINARY_SERIALIZE

#ifdef PPROTO_JSON_SERIALIZE
template<typename CommandDataT>
auto messageReadJson(const Message::Ptr& message, CommandDataT& data, int)
     -> decltype(data.fromJson(QByteArray()), SResult())
{
    return message->readJsonContent(data);
}

template<typename CommandDataT>
auto messageReadJson(const Message::Ptr&, CommandDataT&, long)
     -> SResult
{
    QString err = "Method %1::fromJson not exists";
    err = err.arg(abi_type_name<CommandDataT>().c_str());
    log_error_m << err;
    return SResult(false, 0, err);
}

template<typename CommandDataT>
SResult messageReadJson(const Message::Ptr& message, CommandDataT& data)
{
    return messageReadJson(message, data, 0);
}
#endif // PPROTO_JSON_SERIALIZE

template<typename CommandDataT>
SResult messageReadContent(const Message::Ptr& message, CommandDataT& data,
                           ErrorSenderFunc errorSender)
{
    SResult res {false};
    switch (message->contentFormat())
    {
#ifdef PPROTO_QBINARY_SERIALIZE
        case SerializeFormat::QBinary:
            res = messageReadQBinary(message, data);
            break;
#endif
#ifdef PPROTO_JSON_SERIALIZE
        case SerializeFormat::Json:
            res = messageReadJson(message, data);
            break;
#endif
        default:
            log_error_m << "Unsupported message serialize format: "
                        << message->contentFormat();
            prog_abort();
    }

    if (!res && errorSender)
    {
        data::Error error;
        error.commandId   = message->command();
        error.messageId   = message->id();
        error.code        = error::MessageContentParse;
        error.description = res.description();
        Message::Ptr err = createMessage(error, {message->contentFormat()});
        err->appendDestinationSocket(message->socketDescriptor());
        errorSender(err);
    }
    return res;
}
} // namespace detail

/**
  Преобразует содержимое Message-сообщения с структуру CommandDataT.
  Перед преобразованием выполняется ряд проверок, которые должны исключить
  некорректную десериализацию данных. В случае удачной десериализации поле
  CommandDataT::dataIsValid выставляется в TRUE
*/
template<typename CommandDataT>
SResult readFromMessage(const Message::Ptr& message, CommandDataT& data,
                        ErrorSenderFunc errorSender = ErrorSenderFunc())
{
    static_assert(detail::is_derived_from_data_t<CommandDataT>::value,
                  "CommandDataT must be derived from pproto::data::Data");

    data.dataIsValid = false;

    if (message->command() != data.command())
    {
        log_error_m << log_format(
            "Command of message %? is not equivalent command for data %?",
            CommandNameLog(message->command()), CommandNameLog(data.command()));
    }
    else if (message->type() == Message::Type::Command)
    {
        if (data.forCommandMessage())
        {
            SResult res = detail::messageReadContent(message, data, errorSender);
            data.dataIsValid = (bool)res;
            return res;
        }
        log_error_m << "Message " << CommandNameLog(message->command())
                    << " with type 'Command' cannot write data to struct "
                    << abi_type_name<CommandDataT>() << ". Mismatched types";
    }
    else if (message->type() == Message::Type::Event)
    {
        if (data.forEventMessage())
        {
            SResult res = detail::messageReadContent(message, data, errorSender);
            data.dataIsValid = (bool)res;
            return res;
        }
        log_error_m << "Message " << CommandNameLog(message->command())
                    << " with type 'Event' cannot write data to struct "
                    << abi_type_name<CommandDataT>() << ". Mismatched types";
    }
    else if (message->type() == Message::Type::Answer)
    {
        if (message->execStatus() == Message::ExecStatus::Success)
        {
            if (data.forAnswerMessage())
            {
                SResult res = detail::messageReadContent(message, data, errorSender);
                data.dataIsValid = (bool)res;
                return res;
            }
            log_error_m << "Message " << CommandNameLog(message->command())
                        << " with type 'Answer' cannot write data to struct "
                        << abi_type_name<CommandDataT>() << ". Mismatched types";
        }
        else if (message->execStatus() == Message::ExecStatus::Failed)
        {
            if (data.forAnswerMessage()
                && std::is_base_of<data::MessageFailed, CommandDataT>::value)
            {
                SResult res = detail::messageReadContent(message, data, errorSender);
                data.dataIsValid = (bool)res;
                return res;
            }
            log_error_m << "Message is failed. Type of data must be "
                        << "derived from pproto::data::MessageFailed"
                        << ". Command: " << CommandNameLog(message->command())
                        << ". Struct: "  << abi_type_name<CommandDataT>();
        }
        else if (message->execStatus() == Message::ExecStatus::Error)
        {
            if (data.forAnswerMessage()
                && std::is_base_of<data::MessageError, CommandDataT>::value)
            {
                SResult res = detail::messageReadContent(message, data, errorSender);
                data.dataIsValid = (bool)res;
                return res;
            }
            log_error_m << "Message is error. Type of data must be "
                        << "derived from pproto::data::MessageError"
                        << ". Command: " << CommandNameLog(message->command())
                        << ". Struct: "  << abi_type_name<CommandDataT>();
        }
        else
            log_error_m << "Message exec status is unknown: "
                        << static_cast<quint32>(message->execStatus())
                        << ". Command: " << CommandNameLog(message->command())
                        << ". Struct: "  << abi_type_name<CommandDataT>();
    }
#ifndef NDEBUG
    prog_abort();
#endif
    return SResult(false, 0, "Failed call readFromMessage()");
}

/**
  Специализированные функции для чтения сообщений MessageError, MessageFailed
*/
SResult readFromMessage(const Message::Ptr&, data::MessageError&,
                        ErrorSenderFunc errorSender = ErrorSenderFunc());

SResult readFromMessage(const Message::Ptr&, data::MessageFailed&,
                        ErrorSenderFunc errorSender = ErrorSenderFunc());

namespace detail {

template<typename CommandDataPtr>
SResult readFromMessagePtr(const Message::Ptr& message, CommandDataPtr& data,
                           ErrorSenderFunc errorSender)
{
    if (data.empty())
    {
        typedef typename CommandDataPtr::element_t element_t;
        data = CommandDataPtr(new element_t());
    }
    return readFromMessage(message, *data, errorSender);
}
} // namespace detail

template<typename CommandDataT>
SResult readFromMessage(const Message::Ptr& message, clife_ptr<CommandDataT>& data,
                        ErrorSenderFunc errorSender = ErrorSenderFunc())
{
    return detail::readFromMessagePtr(message, data, errorSender);
}

template<typename CommandDataT>
SResult readFromMessage(const Message::Ptr& message, container_ptr<CommandDataT>& data,
                        ErrorSenderFunc errorSender = ErrorSenderFunc())
{
    return detail::readFromMessagePtr(message, data, errorSender);
}

/**
  Преобразует структуру CommandDataT в Message-сообщение
*/
template<typename CommandDataT>
SResult writeToMessage(const CommandDataT& data, Message::Ptr& message,
                       SerializeFormat contentFormat = SerializeFormat::QBinary,
                       detail::not_error_data<CommandDataT> = 0)
{
    static_assert(detail::is_derived_from_data_t<CommandDataT>::value,
                  "CommandDataT must be derived from pproto::data::Data");

    if (data.command() != message->command())
    {
        log_error_m << "Command of message " << CommandNameLog(message->command())
                    << " is not equal command of data " << CommandNameLog(data.command());
    }
    else if (message->type() == Message::Type::Command)
    {
        if (data.forCommandMessage())
        {
            message->setExecStatus(Message::ExecStatus::Unknown);
            return detail::messageWriteContent(data, message, contentFormat);
        }
        log_error_m << "Structure of data " << abi_type_name<CommandDataT>()
                    << " cannot be used for 'Command'-message";
    }
    else if (message->type() == Message::Type::Event)
    {
        if (data.forEventMessage())
        {
            message->setExecStatus(Message::ExecStatus::Unknown);
            return detail::messageWriteContent(data, message, contentFormat);
        }
        log_error_m << "Structure of data " << abi_type_name<CommandDataT>()
                    << " cannot be used for 'Event'-message";
    }
    else if (message->type() == Message::Type::Answer)
    {
        if (data.forAnswerMessage())
        {
            message->setExecStatus(Message::ExecStatus::Success);
            return detail::messageWriteContent(data, message, contentFormat);
        }
        log_error_m << "Structure of data " << abi_type_name<CommandDataT>()
                    << " cannot be used for 'Answer'-message";
    }
#ifndef NDEBUG
    prog_abort();
#endif
    return SResult(false, 0, "Failed call writeToMessage()");
}

/**
  Специализированные функции для записи сообщений MessageError, MessageFailed.
  При записи данных тип сообщения меняется на Message::Type::Answer, а статус
  Message::ExecStatus на соответствующий структуре данных
*/
template<typename CommandDataT /*MessageError*/>
SResult writeToMessage(const CommandDataT& data, Message::Ptr& message,
                       SerializeFormat contentFormat = SerializeFormat::QBinary,
                       detail::is_error_data<CommandDataT> = 0)
{
    message->setType(Message::Type::Answer);
    message->setExecStatus(Message::ExecStatus::Error);
    return detail::messageWriteContent(data, message, contentFormat);
}

template<typename CommandDataT /*MessageFailed*/>
SResult writeToMessage(const CommandDataT& data, Message::Ptr& message,
                       SerializeFormat contentFormat = SerializeFormat::QBinary,
                       detail::is_failed_data<CommandDataT> = 0)
{
    message->setType(Message::Type::Answer);
    message->setExecStatus(Message::ExecStatus::Failed);
    return detail::messageWriteContent(data, message, contentFormat);
}

namespace detail {

template<typename CommandDataPtr>
SResult writeToMessagePtr(const CommandDataPtr& data, Message::Ptr& message,
                          SerializeFormat contentFormat)
{
    if (data.empty())
    {
        QString err = "Impossible write empty data to message";
        log_error_m << err;
#ifndef NDEBUG
        prog_abort();
#endif
        return SResult(false, 0, err);
    }
    return writeToMessage(*data, message, contentFormat);
}
} // namespace detail

template<typename CommandDataT>
SResult writeToMessage(const clife_ptr<CommandDataT>& data, Message::Ptr& message,
                       SerializeFormat contentFormat = SerializeFormat::QBinary)
{
    return detail::writeToMessagePtr(data, message, contentFormat);
}

template<typename CommandDataT>
SResult writeToMessage(const container_ptr<CommandDataT>& data, Message::Ptr& message,
                       SerializeFormat contentFormat = SerializeFormat::QBinary)
{
    return detail::writeToMessagePtr(data, message, contentFormat);
}

#ifdef PPROTO_JSON_SERIALIZE
template<typename CommandDataT>
SResult writeToJsonMessage(const CommandDataT& data, Message::Ptr& message)
{
    return writeToMessage(data, message, SerializeFormat::Json);
}

template<typename CommandDataT>
SResult writeToJsonMessage(const clife_ptr<CommandDataT>& data, Message::Ptr& message)
{
    return writeToMessage(data, message, SerializeFormat::Json);
}

template<typename CommandDataT>
SResult writeToJsonMessage(const container_ptr<CommandDataT>& data, Message::Ptr& message)
{
    return writeToMessage(data, message, SerializeFormat::Json);
}
#endif

/**
  Сервисная функция, возвращает описание ошибки из сообщений содержащих струк-
  туры MessageError, MessageFailed.  Если  сообщение  не  содержит  информации
  об ошибке - возвращается пустая строка
*/
QString errorDescription(const Message::Ptr&);

} // namespace pproto

#undef log_error_m
#undef log_warn_m
#undef log_info_m
#undef log_verbose_m
#undef log_debug_m
#undef log_debug2_m
