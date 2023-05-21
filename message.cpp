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
*****************************************************************************/

#include "message.h"
#include "serialize/byte_array.h"

#ifdef PPROTO_JSON_SERIALIZE
#include "serialize/json.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#endif

#include "shared/break_point.h"
#include "shared/prog_abort.h"
#include "shared/logger/logger.h"
#include "shared/logger/format.h"
#include "shared/qt/logger_operators.h"

#ifdef LZMA_COMPRESSION
#include "compression/qt/qlzma.h"
#endif
#ifdef PPMD_COMPRESSION
#include "compression/qt/qppmd.h"
#endif

#define log_error_m   alog::logger().error   (alog_line_location, "Message")
#define log_warn_m    alog::logger().warn    (alog_line_location, "Message")
#define log_info_m    alog::logger().info    (alog_line_location, "Message")
#define log_verbose_m alog::logger().verbose (alog_line_location, "Message")
#define log_debug_m   alog::logger().debug   (alog_line_location, "Message")
#define log_debug2_m  alog::logger().debug2  (alog_line_location, "Message")

namespace pproto {

Message::Message() : _flags(0), _flags2(0)
{
    // Флаги  _flags,  _flags2  должны  быть  инициализированы  обязательно
    // в конструкторе, так как невозможно корректно выполнить инициализацию
    // неименованных union-параметров при их объявлении в классе
}

Message::Ptr Message::create(const QUuidEx& command, SerializeFormat contentFormat)
{
    Ptr message {new Message};

    message->_id = QUuid::createUuid();
    message->_command = command;
    message->_flag.type = static_cast<quint32>(Type::Command);
    message->_flag.execStatus = static_cast<quint32>(ExecStatus::Unknown);
    message->_flag.priority = static_cast<quint32>(Priority::Normal);
    message->_flag.compression = static_cast<quint32>(Compression::None);
    message->_flag.contentFormat = static_cast<quint32>(contentFormat);
    message->_proxyId = pproto::proxyId();

    return message;
}

Message::Ptr Message::cloneForAnswer() const
{
    Ptr message {new Message};

    // Клонируемые параметры
    message->_id = _id;
    message->_command = _command;
    message->_protocolVersionLow = _protocolVersionLow;
    message->_protocolVersionHigh = _protocolVersionHigh;
    message->_flags = _flags;
    message->_flags2 = _flags2;
    message->_tags = _tags;
    message->_maxTimeLife = _maxTimeLife;
    message->_proxyId = _proxyId;
    message->_accessId = _accessId;
    message->_socketType = _socketType;
    message->_sourcePoint = _sourcePoint;
    message->_socketDescriptor = _socketDescriptor;
    message->_socketName = _socketName;
    message->_auxiliary = _auxiliary;

    // Инициализируемые параметры
    message->_flag.type = static_cast<quint32>(Type::Answer);
    message->_flag.execStatus = static_cast<quint32>(ExecStatus::Success);
    message->_flag.compression = static_cast<quint32>(Compression::None);

    return message;
}

HostPoint::Set& Message::destinationPoints()
{
    return _destinationPoints;
}

void Message::appendDestinationPoint(const HostPoint& hostPoint)
{
    _destinationPoints.insert(hostPoint);
}

SocketDescriptorSet& Message::destinationSockets()
{
    return _destinationSockets;
}

void Message::appendDestinationSocket(SocketDescriptor descriptor)
{
    _destinationSockets.insert(descriptor);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"

void Message::compress(int level, Compression compression)
{
    if (this->compression() != Compression::None)
        return;

    if (compression == Compression::Disable)
    {
        _flag.compression = static_cast<quint32>(Compression::Disable);
        return;
    }
    level = qBound(-1, level, 9);
    int sz = size()
#ifdef UDP_LONGSIG
           + sizeof(quint64); // UDP long signature
#else
           + sizeof(quint32); // UDP signature
#endif

    // Здесь 508 это минимальный размер UDP пакета передаваемого по сети без
    // фрагментации. Уже при значении 508 сжатие становится мало эффективным,
    // но мы все равно пытаемся сжать пакет, чтобы уложиться в границы 508-ми
    // байт.
    if (level != 0 && sz > 508)
    {
        switch (compression)
        {
            case Compression::Zip:
                _content = qCompress(_content, level);
                _flag.compression = static_cast<quint32>(Compression::Zip);
                break;
#ifdef LZMA_COMPRESSION
            case Compression::Lzma:
            {
                QByteArray content;
                if (qlzma::compress(_content, content, level) == 0)
                {
                    _content = content;
                    _flag.compression = static_cast<quint32>(Compression::Lzma);
                }
                break;
            }
#endif
#ifdef PPMD_COMPRESSION
            case Compression::Ppmd:
            {
                QByteArray content;
                if (qppmd::compress(_content, content, level) == 0)
                {
                    _content = content;
                    _flag.compression = static_cast<quint32>(Compression::Ppmd);
                }
                break;
            }
#endif
            default:
                log_error_m << "Unsupported compression algorithm";
                prog_abort();
        }
    }
}

void Message::decompress(QByteArray& content) const
{
    switch (compression())
    {
        case Compression::None:
        case Compression::Disable:
            content = _content;
            break;

        case Compression::Zip:
            content = qUncompress(_content);
            break;
#ifdef LZMA_COMPRESSION
        case Compression::Lzma:
            if (qlzma::decompress(_content, content) != 0)
                content.clear();
            break;
#endif
#ifdef PPMD_COMPRESSION
        case Compression::Ppmd:
            if (qppmd::decompress(_content, content) != 0)
                content.clear();
            break;
#endif
        default:
            log_error_m << "Unsupported decompression algorithm";
            prog_abort();
    }
}

#pragma GCC diagnostic pop

void Message::decompress()
{
    if (compression() != Compression::None)
    {
        QByteArray content;
        decompress(content);
        _content = content;
        _flag.compression = static_cast<quint32>(Compression::None);
    }
}

QByteArray Message::content() const
{
    QByteArray content;
    decompress(content);
    return content;
}

int Message::size() const
{
    initNotEmptyTraits();

    int sz = sizeof(_id)
             + sizeof(_command)
             + sizeof(_protocolVersionLow)
             + sizeof(_protocolVersionHigh)
             + sizeof(_flags);

    if (_flag.flags2NotEmpty)
        sz += sizeof(_flags2);

    if (_flag.tagsNotEmpty)
        sz += sizeof(quint8) + _tags.count() * sizeof(quint64);

    if (_flag.maxTimeLfNotEmpty)
        sz += sizeof(_maxTimeLife);

    if (_flag.proxyIdNotEmpty)
        sz += sizeof(_proxyId);

    if (_flag.accessIdNotEmpty)
        sz += sizeof(quint32) + _accessId.size();

    if (_flag.contentNotEmpty)
        sz += sizeof(quint32) + _content.size();

    return sz;
}

void Message::initNotEmptyTraits() const
{
    _flag.flags2NotEmpty    = (_flags2 != 0);
    _flag.tagsNotEmpty      = !_tags.isEmpty();
    _flag.maxTimeLfNotEmpty = (_maxTimeLife != quint64(-1));
    _flag.contentNotEmpty   = !_content.isEmpty();
    _flag.proxyIdNotEmpty   = (_proxyId != 0);
    _flag.accessIdNotEmpty  = !_accessId.isEmpty();
}

#ifdef PPROTO_QBINARY_SERIALIZE
QByteArray Message::toQBinary() const
{
    QByteArray ba;
    ba.reserve(size());
    {
        QDataStream stream {&ba, QIODevice::WriteOnly};
        STREAM_INIT(stream);
        toDataStream(stream);
    }
    return ba;
}

Message::Ptr Message::fromQBinary(const QByteArray& ba)
{
    QDataStream stream {(QByteArray*)&ba, QIODevice::ReadOnly | QIODevice::Unbuffered};
    STREAM_INIT(stream);
    return fromDataStream(stream);
}

void Message::toDataStream(QDataStream& stream) const
{
    initNotEmptyTraits();

    stream << _id;
    stream << _command;
    stream << _protocolVersionLow;
    stream << _protocolVersionHigh;
    stream << _flags;

    if (_flag.flags2NotEmpty)
        stream << _flags2;

    if (_flag.tagsNotEmpty)
    {
        stream << quint8(_tags.size());
        for (quint64 t : _tags)
            stream << t;
    }
    if (_flag.maxTimeLfNotEmpty)
        stream << _maxTimeLife;

    if (_flag.proxyIdNotEmpty)
        stream << _proxyId;

    if (_flag.accessIdNotEmpty)
        stream << _accessId;

    if (_flag.contentNotEmpty)
        stream << _content;
}

Message::Ptr Message::fromDataStream(QDataStream& stream)
{
    Ptr message {new Message};

    stream >> message->_id;
    stream >> message->_command;
    stream >> message->_protocolVersionLow;
    stream >> message->_protocolVersionHigh;
    stream >> message->_flags;

    if (message->_flag.flags2NotEmpty)
        stream >> message->_flags2;

    if (message->_flag.tagsNotEmpty)
    {
        quint8 size;
        stream >> size;
        message->_tags.resize(size);
        for (quint8 i = 0; i < size; ++i)
        {
            quint64 t;
            stream >> t;
            message->_tags[i] = t;
        }
    }
    if (message->_flag.maxTimeLfNotEmpty)
        stream >> message->_maxTimeLife;

    if (message->_flag.proxyIdNotEmpty)
        stream >> message->_proxyId;

    if (message->_flag.accessIdNotEmpty)
    {
        //stream >> message->_accessId;
        serialize::readByteArray(stream, message->_accessId);
    }

    if (message->_flag.contentNotEmpty)
    {
        //stream >> message->_content;
        serialize::readByteArray(stream, message->_content);
    }

    return message;
}
#endif // PPROTO_QBINARY_SERIALIZE

#ifdef PPROTO_JSON_SERIALIZE
QByteArray Message::toJson(bool webFlags) const
{
    initNotEmptyTraits();

    using namespace rapidjson;
    StringBuffer buff;
    rapidjson::Writer<StringBuffer> writer(buff);

    writer.StartObject();

    // stream << _id;
    writer.Key("id");
    const QByteArray& id = _id.toByteArray();
    writer.String(id.constData() + 1, SizeType(id.length() - 2));

    // stream << _command;
    writer.Key("command");
    const QByteArray& command = _command.toByteArray();
    writer.String(command.constData() + 1, SizeType(command.length() - 2));

    if (_protocolVersionLow != 0)
    {
        // stream << _protocolVersionLow;
        writer.Key("protocolVersionLow");
        writer.Uint(_protocolVersionLow);
    }
    if (_protocolVersionHigh != 0)
    {
        // stream << _protocolVersionHigh;
        writer.Key("protocolVersionHigh");
        writer.Uint(_protocolVersionHigh);
    }

    // stream << _flags;
    writer.Key("flags");
    writer.Uint(_flags);

    if (_flag.flags2NotEmpty)
    {
        // stream << _flags2;
        writer.Key("flags2");
        writer.Uint(_flags2);
    }
    if (_flag.tagsNotEmpty)
    {
        writer.Key("tags");
        writer.StartArray();
        for (int i = 0; i < _tags.count(); ++i)
            writer.Uint64(_tags.at(i));
        writer.EndArray();
    }
    if (_flag.maxTimeLfNotEmpty)
    {
        // stream << _maxTimeLife;
        writer.Key("maxTimeLife");
        writer.Uint64(_maxTimeLife);
    }
    if (_flag.proxyIdNotEmpty)
    {
        // stream << _proxyId;
        writer.Key("proxyId");
        writer.Uint64(_proxyId);
    }
    if (_flag.accessIdNotEmpty)
    {
        // stream << _accessId;
        writer.Key("accessId");
        QByteArray ba {"\""};
        ba.append(_accessId);
        ba.append("\"");
        writer.RawValue(ba.constData(), size_t(ba.length()), kStringType);
    }
    if (_flag.contentNotEmpty)
    {
        // stream << _content;
        writer.Key("content");
        writer.RawValue(_content.constData(), size_t(_content.length()), kObjectType);
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"

    if (webFlags)
    {
        writer.Key("webFlags");
        writer.StartObject();

        writer.Key("type");
        switch (type())
        {
            case Message::Type::Command: writer.String("command"); break;
            case Message::Type::Answer:  writer.String("answer" ); break;
            case Message::Type::Event:   writer.String("event"  ); break;
            default:                     writer.String("unknown"); break;
        }

        writer.Key("execStatus");
        switch (execStatus())
        {
            case Message::ExecStatus::Success: writer.String("success"); break;
            case Message::ExecStatus::Failed:  writer.String("failed" );break;
            case Message::ExecStatus::Error:   writer.String("error"  ); break;
            default:                           writer.String("unknown"); break;
        }

        writer.Key("priority");
        switch (priority())
        {
            case Message::Priority::High: writer.String("high"  ); break;
            case Message::Priority::Low:  writer.String("low"   ); break;
            default:                      writer.String("normal"); break;
        }

        // compression() не используется при json-сериализации

        writer.Key("contentFormat");
        writer.String("json");

        writer.EndObject(); // Key("webFlags")
    }

#pragma GCC diagnostic pop

    writer.EndObject();
    return QByteArray(buff.GetString());
}

Message::Ptr Message::fromJson(const QByteArray& ba)
{
    Ptr message {new Message};

    using namespace rapidjson;
    using namespace serialize::json;
    Document doc;
    doc.Parse(ba.constData(), size_t(ba.length()));

    if (doc.HasParseError())
    {
        ParseErrorCode e = doc.GetParseError();
        int o = int(doc.GetErrorOffset());
        log_error_m << log_format(
            "Failed parce json. Error: %? Detail: at offset %? near '%?...'",
            GetParseError_En(e), o, ba.mid(o, 20));
        return message;
    }
    if (!doc.IsObject())
    {
        log_error_m << "Failed json format";
        return message;
    }

    bool tagsNotEmpty      = false;
    bool maxTimeLfNotEmpty = false;
    bool contentNotEmpty   = false;
    bool proxyIdNotEmpty   = false;
    bool accessIdNotEmpty  = false;
    bool flags2NotEmpty    = false;

    quint32 flags = 0, flags2 = 0;
    bool webFlagsExists = false;

    for (auto member = doc.MemberBegin(); member != doc.MemberEnd(); ++member)
    {
        if (stringEqual("id", member->name) && member->value.IsString())
        {
            const QByteArray& ba = QByteArray::fromRawData(member->value.GetString(),
                                                           member->value.GetStringLength());
            message->_id = QUuidEx(ba);
        }
        else if (stringEqual("command", member->name) && member->value.IsString())
        {
            const QByteArray& ba = QByteArray::fromRawData(member->value.GetString(),
                                                           member->value.GetStringLength());
            message->_command = QUuidEx(ba);
        }
        else if (stringEqual("protocolVersionLow", member->name) && member->value.IsUint())
        {
            message->_protocolVersionLow = quint16(member->value.GetUint());
        }
        else if (stringEqual("protocolVersionHigh", member->name) && member->value.IsUint())
        {
            message->_protocolVersionHigh = quint16(member->value.GetUint());
        }
        else if (stringEqual("flags", member->name) && member->value.IsUint())
        {
            flags = quint32(member->value.GetUint());
        }
        else if (stringEqual("flags2", member->name) && member->value.IsUint())
        {
            flags2NotEmpty = true;
            flags2 = quint32(member->value.GetUint());
        }
        else if (stringEqual("tags", member->name) && member->value.IsArray())
        {
            tagsNotEmpty = true;
            int size = int(member->value.Size());
            message->_tags.resize(size);
            for (int i = 0; i < size; ++i)
                message->_tags[i] = member->value[SizeType(i)].GetUint64();
        }
        else if (stringEqual("maxTimeLife", member->name) && member->value.IsUint64())
        {
            maxTimeLfNotEmpty = true;
            message->_maxTimeLife = quint64(member->value.GetUint64());
        }
        else if (stringEqual("proxyId", member->name) && member->value.IsUint64())
        {
            proxyIdNotEmpty = true;
            message->_proxyId = quint64(member->value.GetUint64());
        }
        else if (stringEqual("accessId", member->name) && member->value.IsString())
        {
            accessIdNotEmpty = true;
            StringBuffer buff;
            rapidjson::Writer<StringBuffer> writer {buff};
            member->value.Accept(writer);
            message->_accessId = QByteArray(buff.GetString() + 1);
            message->_accessId.chop(1);
        }
        else if (stringEqual("content", member->name) && member->value.IsObject())
        {
            contentNotEmpty = true;
            StringBuffer buff;
            rapidjson::Writer<StringBuffer> writer {buff};
            member->value.Accept(writer);
            message->_content = QByteArray(buff.GetString());
        }
        else if (stringEqual("webFlags", member->name) && member->value.IsObject())
        {
            webFlagsExists = true;
            for (auto wflag = member->value.MemberBegin(); wflag != member->value.MemberEnd(); ++wflag)
            {
                auto equal = [](const char* s1, const char* s2) -> bool {
                    return (std::strcmp(s1, s2) == 0);
                };

                if (stringEqual("type", wflag->name) && wflag->value.IsString())
                {
                    const char* s = wflag->value.GetString();
                    if      (equal(s, "command")) message->setType(Message::Type::Command);
                    else if (equal(s, "answer" )) message->setType(Message::Type::Answer);
                    else if (equal(s, "event"  )) message->setType(Message::Type::Event);
                    else                          message->setType(Message::Type::Unknown);
                }
                else if (stringEqual("execStatus", wflag->name) && wflag->value.IsString())
                {
                    const char* s = wflag->value.GetString();
                    if      (equal(s, "success")) message->setExecStatus(Message::ExecStatus::Success);
                    else if (equal(s, "failed" )) message->setExecStatus(Message::ExecStatus::Failed);
                    else if (equal(s, "error"  )) message->setExecStatus(Message::ExecStatus::Error);
                    else                          message->setExecStatus(Message::ExecStatus::Unknown);
                }
                else if (stringEqual("priority", wflag->name) && wflag->value.IsString())
                {
                    const char* s = wflag->value.GetString();
                    if      (equal(s, "high")) message->setPriority(Message::Priority::High);
                    else if (equal(s, "low" )) message->setPriority(Message::Priority::Low);
                    else                       message->setPriority(Message::Priority::Normal);
                }

                // compression() не используется при json-сериализации

                else if (stringEqual("contentFormat", wflag->name) && wflag->value.IsString())
                {
                    message->setContentFormat(SerializeFormat::Json);
                }
            }
        }
    }

    message->_flag.tagsNotEmpty      = tagsNotEmpty;
    message->_flag.maxTimeLfNotEmpty = maxTimeLfNotEmpty;
    message->_flag.contentNotEmpty   = contentNotEmpty;
    message->_flag.proxyIdNotEmpty   = proxyIdNotEmpty;
    message->_flag.accessIdNotEmpty  = accessIdNotEmpty;
    message->_flag.flags2NotEmpty    = flags2NotEmpty;

    if (flags)
    {
        if (webFlagsExists && (message->_flags != flags))
            log_error_m << "Binary-flags and web-flags do not match"
                        << ". Will be used binary-flags";
        message->_flags = flags;
    }
    if (flags2)
        message->_flags2 = flags2;

    return message;
}
#endif // PPROTO_JSON_SERIALIZE

Message::Type Message::type() const
{
    return static_cast<Type>(_flag.type);
}

void Message::setType(Type val)
{
    _flag.type = static_cast<quint32>(val);
}

Message::ExecStatus Message::execStatus() const
{
    return static_cast<ExecStatus>(_flag.execStatus);
}

void Message::setExecStatus(ExecStatus val)
{
    _flag.execStatus = static_cast<quint32>(val);
}

Message::Priority Message::priority() const
{
    return static_cast<Priority>(_flag.priority);
}

void Message::setPriority(Priority val)
{
    _flag.priority = static_cast<quint32>(val);
}

quint64 Message::tag(int index) const
{
    if (!lst::inRange(index, 0, 255))
    {
        log_error_m << "Index value not in range [0..254]";
        return 0;
    }
    if (index >= _tags.count())
        return 0;

    return _tags[index];
}

void Message::setTag(quint64 val, int index)
{
    if (!lst::inRange(index, 0, 255))
    {
        log_error_m << "Index value not in range [0..254]";
        return;
    }
    if (index >= _tags.count())
        _tags.resize(index + 1);

    _tags[index] = val;
}

void Message::setTags(const QVector<quint64>& val)
{
    _tags = val;
    if (_tags.count() > 255)
    {
        log_error_m << "Size of tags array great then 255"
                       ". Array will be truncated to 255";
        _tags.resize(255);
    }
}

Message::Compression Message::compression() const
{
    return static_cast<Compression>(_flag.compression);
}

SerializeFormat Message::contentFormat() const
{
    return static_cast<SerializeFormat>(_flag.contentFormat);
}

void Message::setContentFormat(SerializeFormat val)
{
    _flag.contentFormat = static_cast<quint32>(val);
}

quint64 internalProxyId(const quint64* val = nullptr)
{
    static quint64 id {0};
    if (val) id = *val;
    return id;
}

quint64 proxyId()
{
    return internalProxyId();
}

void setProxyId(quint64 id)
{
    (void) internalProxyId(&id);
}

} // namespace pproto
