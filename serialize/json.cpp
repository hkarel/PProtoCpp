/*****************************************************************************
  The MIT License

  Copyright © 2018 Pavel Karelin (hkarel), <hkarel@yandex.ru>

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

#include "serialize/json.h"
#include "rapidjson/error/en.h"
#include <atomic>

#define log_error_m   alog::logger().error   (alog_line_location, "JSerialize")
#define log_warn_m    alog::logger().warn    (alog_line_location, "JSerialize")
#define log_info_m    alog::logger().info    (alog_line_location, "JSerialize")
#define log_verbose_m alog::logger().verbose (alog_line_location, "JSerialize")
#define log_debug_m   alog::logger().debug   (alog_line_location, "JSerialize")
#define log_debug2_m  alog::logger().debug2  (alog_line_location, "JSerialize")

namespace pproto::serialize::json {

#include <cassert>
#include <cctype>

//---------------------------------- Reader ----------------------------------

static std::atomic<std::uint64_t> jsonIndexReader = {0};

Reader::Reader() : _jsonIndex(jsonIndexReader++)
{}

Reader::~Reader()
{
    if (_hasParseError)
    {
        log_error_m << "Failed parse json"
                    << ". JIndex: " << _jsonIndex
                    << ". Content: " << _jsonContent;
    }
}

SResult Reader::result() const
{
    if (_hasParseError)
    {
        QString msg = "Failed parse json. JIndex: %1";
        SResult res {false, 1, msg.arg(_jsonIndex)};
        return res;
    }
    return SResult(true);
}

bool Reader::parse(const QByteArray& json)
{
    _jsonContent = json;
    _document.Parse(_jsonContent.constData());
    if (_document.HasParseError())
    {
        setError(1);
        ParseErrorCode err = _document.GetParseError();
        int offset = int(_document.GetErrorOffset());

        log_error_m << log_format(
            "Failed parse json. JIndex: %?. Error: %? Detail: at offset %? near '%?...'",
            _jsonIndex, GetParseError_En(err), offset, _jsonContent.mid(offset, 30));
    }
    else
    {
        setError(0);
        _stack.push({&_document, StackItem::BeforeStart});
    }
    return !error();
}

Reader& Reader::member(const char* name, bool optional)
{
    if (error() < 1)
    {
        if (_stack.top().value->IsObject()
            && _stack.top().state == StackItem::Started)
        {
            Value::ConstMemberIterator memberItr = _stack.top().value->FindMember(name);
            if (memberItr != _stack.top().value->MemberEnd())
            {
                setError(0);
                _stack.push({&memberItr->value, StackItem::BeforeStart, name,
                            (optional ? 1 : 0)});
            }
            else
            {
                if (!optional)
                {
                    setError(1);
                    log_error_m << "Mandatory field '" << name << "' not found"
                                << ". Stack path: " << stackPath()
                                << ". JIndex: " << _jsonIndex;
                }
                else
                    setError(-1, optional);
            }
        }
        else
        {
            setError(1);
            log_error_m << "Stack top is not object"
                        << ". Field: " << name
                        << ". Stack path: " << stackPath()
                        << ". JIndex: " << _jsonIndex;
        }
    }
    return *this;
}

void Reader::setError(int val, bool optional)
{
    _error = val;
    if ((_error != 0) && !optional)
        _hasParseError = true;
}

QByteArray Reader::stackFieldName() const
{
    for (int i = _stack.count() - 1; i >= 0; --i)
        if (!_stack[i].name.isEmpty())
            return _stack[i].name;

    return QByteArray();
}

QByteArray Reader::stackPath() const
{
    QByteArray path;
    for (int i = 0; i < _stack.count(); ++i)
    {
        path += _stack[i].name;
        path += (_stack[i].value->IsArray()) ? "[]" : "/";
    }
    if ((path.size() > 1) && (path[path.size() - 1] == '/'))
        path.chop(1);

    return path;
}

Reader& Reader::startObject()
{
    if (!error())
    {
        if (_stack.top().value->IsObject()
            && _stack.top().state == StackItem::BeforeStart)
        {
            _stack.top().state = StackItem::Started;
        }
        else
        {
            setError(1);
            log_error_m << "Stack top is not object"
                        << ". Field: " << stackFieldName()
                        << ". Stack path: " << stackPath()
                        << ". JIndex: " << _jsonIndex;
        }
    }
    return *this;
}

Reader& Reader::endObject()
{
    if (error() < 1)
    {
        if (_stack.top().value->IsObject()
            && _stack.top().state == StackItem::Started)
        {
            next();
        }
        else
        {
            setError(1);
            log_error_m << "Stack top is not object"
                        << ". Field: " << stackFieldName()
                        << ". Stack path: " << stackPath()
                        << ". JIndex: " << _jsonIndex;
        }
    }
    return *this;
}

Reader& Reader::startArray(SizeType& size)
{
    size = 0;
    if (!error())
    {
        if (_stack.top().value->IsArray()
            && _stack.top().state == StackItem::BeforeStart)
        {
            _stack.top().state = StackItem::Started;
            size = _stack.top().value->Size();

            if (!_stack.top().value->Empty())
            {
                const Value& value = (*_stack.top().value)[_stack.top().index];
                _stack.push({&value, StackItem::BeforeStart});
            }
            else
                _stack.top().state = StackItem::Closed;
        }
        else
        {
            setError(1);
            log_error_m << "Stack top is not array"
                        << ". Field: " << stackFieldName()
                        << ". Stack path: " << stackPath()
                        << ". JIndex: " << _jsonIndex;
        }
    }
    return *this;
}

Reader& Reader::endArray()
{
    if (error() < 1)
    {
        if (_stack.top().value->IsArray()
            && _stack.top().state == StackItem::Closed)
        {
            next();
        }
        else
        {
            setError(1);
            log_error_m << "Stack top is not array"
                        << ". Field: " << stackFieldName()
                        << ". Stack path: " << stackPath()
                        << ". JIndex: " << _jsonIndex;
        }
    }
    return *this;
}

void Reader::next()
{
    if (error() > 0)
        return;

    assert(!_stack.empty());
    _stack.pop();

    if (!_stack.empty() && _stack.top().value->IsArray())
    {
        // Otherwise means reading array item pass end
        if (_stack.top().state == StackItem::Started)
        {
            if (_stack.top().index < (_stack.top().value->Size() - 1))
            {
                // Обнуляем ошибку отсутствия опционального поля в элементе списка
                if (error() == -1)
                    setError(0);

                const Value& value = (*_stack.top().value)[++_stack.top().index];
                _stack.push({&value, StackItem::BeforeStart});
            }
            else
                _stack.top().state = StackItem::Closed;
        }
        else
        {
            setError(1);
            log_error_m << "Stack top state is not StackItem::Started"
                        << ". Field: " << stackFieldName()
                        << ". Stack path: " << stackPath()
                        << ". JIndex: " << _jsonIndex;
        }
    }
}

Reader& Reader::setNull()
{
    // This function is for Writer only.
    setError(1);
    return *this;
}

Reader& Reader::operator& (bool& b)
{
    if (!error())
    {
        if (_stack.top().value->IsBool())
        {
            b = _stack.top().value->GetBool();
            next();
        }
        else if (_stack.top().value->IsNull())
        {
            b = false;
            next();
        }
        else
        {
            setError(1);
            log_error_m << "Stack top is not 'bool' type"
                        << ". Field: " << stackFieldName()
                        << ". Stack path: " << stackPath()
                        << ". JIndex: " << _jsonIndex;
        }
    }
    return *this;
}

Reader& Reader::operator& (qint8& i)
{
    qint32 val;
    this->operator& (val);
    if (!error())
        i = qint8(val);
    return *this;
}

Reader& Reader::operator& (quint8& u)
{
    quint32 val;
    this->operator& (val);
    if (!error())
        u = qint8(val);
    return *this;
}

Reader& Reader::operator& (qint16& i)
{
    qint32 val;
    this->operator& (val);
    if (!error())
        i = qint16(val);
    return *this;
}

Reader& Reader::operator& (quint16& u)
{
    quint32 val;
    this->operator& (val);
    if (!error())
        u = quint16(val);
    return *this;
}

Reader& Reader::operator& (qint32& i)
{
    if (!error())
    {
        if (_stack.top().value->IsInt())
        {
            i = _stack.top().value->GetInt();
            next();
        }
        else if (_stack.top().value->IsNull())
        {
            i = 0;
            next();
        }
        else
        {
            setError(1);
            log_error_m << "Stack top is not 'int' type"
                        << ". Field: " << stackFieldName()
                        << ". Stack path: " << stackPath()
                        << ". JIndex: " << _jsonIndex;
        }
    }
    return *this;
}

Reader& Reader::operator& (quint32& u)
{
    if (!error())
    {
        if (_stack.top().value->IsUint())
        {
            u = _stack.top().value->GetUint();
            next();
        }
        else if (_stack.top().value->IsNull())
        {
            u = 0;
            next();
        }
        else
        {
            setError(1);
            log_error_m << "Stack top is not 'uint' type"
                        << ". Field: " << stackFieldName()
                        << ". Stack path: " << stackPath()
                        << ". JIndex: " << _jsonIndex;
        }
    }
    return *this;
}

Reader& Reader::operator& (qint64& i)
{
    if (!error())
    {
        if (_stack.top().value->IsInt64())
        {
            i = _stack.top().value->GetInt64();
            next();
        }
        else if (_stack.top().value->IsNull())
        {
            i = 0;
            next();
        }
        else
        {
            setError(1);
            log_error_m << "Stack top is not int64 type"
                        << ". Field: " << stackFieldName()
                        << ". Stack path: " << stackPath()
                        << ". JIndex: " << _jsonIndex;
        }
    }
    return *this;
}

Reader& Reader::operator& (quint64& u)
{
    if (!error())
    {
        if (_stack.top().value->IsUint64())
        {
            u = _stack.top().value->GetUint64();
            next();
        }
        else if (_stack.top().value->IsNull())
        {
            u = 0;
            next();
        }
        else
        {
            setError(1);
            log_error_m << "Stack top is not uint64 type"
                        << ". Field: " << stackFieldName()
                        << ". Stack path: " << stackPath()
                        << ". JIndex: " << _jsonIndex;
        }
    }
    return *this;
}

Reader& Reader::operator& (double& d)
{
    if (!error())
    {
        if (_stack.top().value->IsNumber())
        {
            d = _stack.top().value->GetDouble();
            next();
        }
        else if (_stack.top().value->IsNull())
        {
            d = 0;
            next();
        }
        else
        {
            setError(1);
            log_error_m << "Stack top is not number"
                        << ". Field: " << stackFieldName()
                        << ". Stack path: " << stackPath()
                        << ". JIndex: " << _jsonIndex;
        }
    }
    return *this;
}

Reader& Reader::operator& (float& f)
{
    double val;
    this->operator& (val);
    if (!error())
        f = float(val);
    return *this;
}

Reader& Reader::operator& (QByteArray& ba)
{
    if (!error())
    {
        if (_stack.top().value->IsNull())
        {
            ba = QByteArray();
            next();
        }
        else
        {
            StringBuffer buff;
            rapidjson::Writer<StringBuffer> writer {buff};
            _stack.top().value->Accept(writer);
            ba = QByteArray(buff.GetString());
            next();
        }
    }
    return *this;
}

Reader& Reader::operator& (SByteArray& ba)
{
    return operator& (static_cast<QByteArray&>(ba));
}

Reader& Reader::operator& (QString& s)
{
    if (!error())
    {
        if (_stack.top().value->IsString())
        {
            s = QString::fromUtf8(_stack.top().value->GetString());
            next();
        }
        else if (_stack.top().value->IsNull())
        {
            s = QString();
            next();
        }
        else
        {
            setError(1);
            log_error_m << "Stack top is not 'string' type"
                        << ". Field: " << stackFieldName()
                        << ". Stack path: " << stackPath()
                        << ". JIndex: " << _jsonIndex;
        }
    }
    return *this;
}

Reader& Reader::operator& (QUuid& uuid)
{
    if (!error())
    {
        if (_stack.top().value->IsString())
        {
            const QByteArray& ba = QByteArray::fromRawData(
                                       _stack.top().value->GetString(),
                                       _stack.top().value->GetStringLength());
            uuid = QUuid(ba);
            next();
        }
        else if (_stack.top().value->IsNull())
        {
            uuid = QUuid();
            next();
        }
        else
        {
            setError(1);
            log_error_m << "Stack top is not 'string' type"
                        << ". Field: " << stackFieldName()
                        << ". Stack path: " << stackPath()
                        << ". JIndex: " << _jsonIndex;
        }
    }
    return *this;
}

Reader& Reader::operator& (QDate& date)
{
    if (!error())
    {
        if (_stack.top().value->IsString())
        {
            date = QDate::fromString(_stack.top().value->GetString(), "yyyy-MM-dd");
            next();
        }
        else if (_stack.top().value->IsNull())
        {
            date = QDate();
            next();
        }
        else
        {
            setError(1);
            log_error_m << "Stack top is not 'string' type"
                        << ". Field: " << stackFieldName()
                        << ". Stack path: " << stackPath()
                        << ". JIndex: " << _jsonIndex;
        }
    }
    return *this;
}

Reader& Reader::operator& (QTime& time)
{
    if (!error())
    {
        if (_stack.top().value->IsString())
        {
            time = QTime::fromString(_stack.top().value->GetString(), "hh:mm:ss.zzz");
            next();
        }
        else if (_stack.top().value->IsNull())
        {
            time = QTime();
            next();
        }
        else
        {
            setError(1);
            log_error_m << "Stack top is not 'string' type"
                        << ". Field: " << stackFieldName()
                        << ". Stack path: " << stackPath()
                        << ". JIndex: " << _jsonIndex;
        }
    }
    return *this;
}

Reader& Reader::operator& (QDateTime& dtime)
{
    if (!error())
    {
        if (_stack.top().value->IsInt64())
        {
            dtime = QDateTime::fromMSecsSinceEpoch(_stack.top().value->GetInt64());
            next();
        }
        else if (_stack.top().value->IsNull())
        {
            dtime = QDateTime();
            next();
        }
        else
        {
            setError(1);
            log_error_m << "Stack top is not int64 type"
                        << ". Field: " << stackFieldName()
                        << ". Stack path: " << stackPath()
                        << ". JIndex: " << _jsonIndex;
        }
    }
    return *this;
}

Reader& Reader::operator& (std::string& s)
{
    if (!error())
    {
        if (_stack.top().value->IsString())
        {
            s = _stack.top().value->GetString();
            next();
        }
        else if (_stack.top().value->IsNull())
        {
            s = std::string();
            next();
        }
        else
        {
            setError(1);
            log_error_m << "Stack top is not 'string' type"
                        << ". Field: " << stackFieldName()
                        << ". Stack path: " << stackPath()
                        << ". JIndex: " << _jsonIndex;
        }
    }
    return *this;
}

//---------------------------------- Writer ----------------------------------

Writer::Writer() : _writer(_stream)
{}

Writer::~Writer()
{}

const char* Writer::getString() const
{
    return _stream.GetString();
}

Writer& Writer::member(const char* name, bool)
{
    _writer.String(name, static_cast<SizeType>(strlen(name)));
    return *this;
}

Writer& Writer::startObject()
{
    _writer.StartObject();
    return *this;
}

Writer& Writer::endObject()
{
    _writer.EndObject();
    return *this;
}

Writer& Writer::startArray(SizeType*)
{
    _writer.StartArray();
    return *this;
}

Writer& Writer::endArray()
{
    _writer.EndArray();
    return *this;
}

Writer& Writer::setNull()
{
    _writer.Null();
    return *this;
}

Writer& Writer::operator& (const bool b)
{
    _writer.Bool(b);
    return *this;
}

Writer& Writer::operator& (const qint8 i)
{
    return this->operator& (qint32(i));
}

Writer& Writer::operator& (const quint8 u)
{
    return this->operator& (quint32(u));
}

Writer& Writer::operator& (const qint16 i)
{
    return this->operator& (qint32(i));
}

Writer& Writer::operator& (const quint16 u)
{
    return this->operator& (quint32(u));
}

Writer& Writer::operator& (const qint32 i)
{
    _writer.Int(i);
    return *this;
}

Writer& Writer::operator& (const quint32 u)
{
    _writer.Uint(u);
    return *this;
}

Writer& Writer::operator& (const qint64 i)
{
    _writer.Int64(i);
    return *this;
}

Writer& Writer::operator& (const quint64 u)
{
    _writer.Uint64(u);
    return *this;
}

Writer& Writer::operator& (const double d)
{
    _writer.Double(d);
    return *this;
}

Writer& Writer::operator& (const float f)
{
    _writer.Double(f);
    return *this;
}

Writer& Writer::operator& (const QByteArray& ba)
{
    if (ba.isNull())
    {
        setNull();
        return *this;
    }

    const char* begin = ba.constData();
    const char* end = begin + ba.length() - 1;

    // Удаляем пробелы в начале
    while (std::isspace(static_cast<unsigned char>(*begin)))
        ++begin;

    // Удаляем пробелы в конце
    while (std::isspace(static_cast<unsigned char>(*end)))
        --end;

    int length = end - begin + 1;

    if ((*begin == '{') && (*end == '}'))
    {
        _writer.RawValue(begin, size_t(length), kObjectType);
        return *this;
    }

    if ((*begin == '[') && (*end == ']'))
    {
        _writer.RawValue(begin, size_t(length), kArrayType);
        return *this;
    }

    QByteArray br = QByteArray::fromRawData(begin, length);
    if (br.length() == 4 /*sizeof 'TRUE'*/
        || br.length() == 5 /*sizeof 'FALSE'*/)
    {
        if (br == "true"
            || br == "True"
            || br == "TRUE")
        {
            _writer.Bool(true);
            return *this;
        }
        if (br == "false"
            || br == "False"
            || br == "FALSE")
        {
            _writer.Bool(false);
            return *this;
        }
    }
    if (br.length() <= 32 /*sizeof double in string representation*/)
    {
        bool ok;
        qlonglong i64 = br.toLongLong(&ok);
        if (ok)
        {
            _writer.Int64(i64);
            return *this;
        }

        qulonglong ui64 = br.toULongLong(&ok);
        if (ok)
        {
            _writer.Uint64(ui64);
            return *this;
        }

        double d = br.toDouble(&ok);
        if (ok)
        {
            _writer.Double(d);
            return *this;
        }
    }

    QByteArray ba2 {"\""};
    ba2.append(br);
    ba2.append("\"");
    _writer.RawValue(ba2.constData(), size_t(ba2.length()), kStringType);
    return *this;
}

Writer& Writer::operator& (const SByteArray& ba)
{
    return operator& (static_cast<const QByteArray&>(ba));
}

Writer& Writer::operator& (const QString& s)
{
#ifndef PPROTO_JSON_STRING_NOTNULL
    if (s.isNull())
    {
        setNull();
        return *this;
    }
#endif

    const QByteArray& ba = s.toUtf8();
    _writer.String(ba.constData(), SizeType(ba.length()));
    return *this;
}

Writer& Writer::operator& (const QUuid& uuid)
{
    if (uuid.isNull())
    {
        setNull();
        return *this;
    }

    const QByteArray& ba = uuid.toByteArray();
    if ((ba[0] == '{') && (ba[ba.length()-1] == '}'))
    {
        _writer.String(ba.constData() + 1, SizeType(ba.length() - 2));
    }
    else
    {
        break_point
        // Выяснить в каких случаях нет замыкающих фигурных скобок
        _writer.String(ba.constData(), SizeType(ba.length()));
    }
    return *this;
}

Writer& Writer::operator& (const QDate& date)
{
    if (date.isValid())
    {
        const QByteArray& ba = date.toString("yyyy-MM-dd").toUtf8();
        _writer.String(ba.constData(), SizeType(ba.length()));
    }
    else
        setNull();

    return *this;
}

Writer& Writer::operator& (const QTime& time)
{
    if (time.isValid())
    {
        const QByteArray& ba = time.toString("hh:mm:ss.zzz").toUtf8();
        _writer.String(ba.constData(), SizeType(ba.length()));
    }
    else
        setNull();

    return *this;
}

Writer& Writer::operator& (const QDateTime& dtime)
{
    if (dtime.isValid())
        _writer.Int64(dtime.toMSecsSinceEpoch());
    else
        setNull();

    return *this;
}

Writer& Writer::operator& (const std::string& s)
{
#ifndef PPROTO_JSON_STRING_NOTNULL
    if (s.empty())
    {
        setNull();
        return *this;
    }
#endif

    _writer.String(s.c_str(), SizeType(s.length()));
    return *this;
}

} // namespace pproto::serialize::json
