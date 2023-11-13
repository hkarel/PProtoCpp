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
  ---

  В модуле представлены функции и макросы механизма json сериализации данных

*****************************************************************************/

#pragma once

#include "serialize/result.h"
#include "serialize/byte_array.h"

#include "shared/list.h"
#include "shared/defmac.h"
#include "shared/clife_base.h"
#include "shared/clife_ptr.h"
#include "shared/container_ptr.h"
#include "shared/break_point.h"
#include "shared/logger/logger.h"
#include "shared/qt/quuidex.h"
#include "shared/qt/logger_operators.h"

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include <QtGlobal>
#include <QDateTime>
#include <QByteArray>
#include <QString>
#include <QList>
#include <QVector>
#include <QStack>

#include <list>
#include <vector>
#include <type_traits>

namespace pproto::serialize::json {

using namespace rapidjson;
class Reader;

namespace detail {

template<typename T> using not_enum_type =
typename std::enable_if<!std::is_enum<T>::value, int>::type;

template<typename T> using is_enum_type =
typename std::enable_if<std::is_enum<T>::value, int>::type;

template<typename T> using derived_from_clife_base =
typename std::enable_if<std::is_base_of<clife_base, T>::value, int>::type;

template<typename T> using not_derived_from_clife_base =
typename std::enable_if<!std::is_base_of<clife_base, T>::value, int>::type;

template<typename T>
Reader& operatorAmp(Reader&, T&, not_enum_type<T> = 0);

template<typename T, typename Compare, typename Allocator>
Reader& readArray(Reader&, lst::List<T, Compare, Allocator>&,
                  derived_from_clife_base<T> = 0);

template<typename T, typename Compare, typename Allocator>
Reader& readArray(Reader&, lst::List<T, Compare, Allocator>&,
                  not_derived_from_clife_base<T> = 0);

template<typename T> Reader& readArray(Reader&, T&);
template<typename T> Reader& readPtr  (Reader&, T&);

} // namespace detail

class Reader
{
public:
    Reader();
    ~Reader();

    SResult result() const;

    // Parse json
    bool parse(const QByteArray& json);
    bool hasParseError() const {return _hasParseError;}

    Reader& member(const char* name, bool optional = false);
    quint64 jsonIndex() const {return _jsonIndex;}

    bool stackTopIsNull()     const {return _stack.top().value->IsNull();}
    bool stackTopIsObject()   const {return _stack.top().value->IsObject();}
    bool stackTopIsOptional() const {return (_stack.top().optional == 1);}

    Reader& startObject();
    Reader& endObject();

    Reader& startArray(SizeType& size);
    Reader& endArray();

    Reader& setNull();

    Reader& operator& (bool&);
    Reader& operator& (qint8&);
    Reader& operator& (quint8&);
    Reader& operator& (qint16&);
    Reader& operator& (quint16&);
    Reader& operator& (qint32&);
    Reader& operator& (quint32&);
    Reader& operator& (qint64&);
    Reader& operator& (quint64&);
    Reader& operator& (double&);
    Reader& operator& (float&);
    Reader& operator& (QByteArray&);
    Reader& operator& (SByteArray&);
    Reader& operator& (QString&);
    Reader& operator& (QUuid&);
    Reader& operator& (QDate&);
    Reader& operator& (QTime&);
    Reader& operator& (QDateTime&);
    Reader& operator& (std::string&);

    template<typename T> Reader& operator& (T& t);
    template<typename T> Reader& operator& (QList<T>&);
#if QT_VERSION < 0x060000
    template<typename T> Reader& operator& (QVector<T>&);
#endif
    template<typename T> Reader& operator& (clife_ptr<T>&);
    template<typename T> Reader& operator& (container_ptr<T>&);
    template<int N>      Reader& operator& (QUuidT<N>&);

    template<typename T> Reader& operator& (std::list<T>&);
    template<typename T> Reader& operator& (std::vector<T>&);

    template<typename T, typename Compare, typename Allocator>
    Reader& operator& (lst::List<T, Compare, Allocator>&);

    bool isReader() const {return true;}
    bool isWriter() const {return false;}

    // Вспомогательная функция, используется при парсинге элементов json.
    // Функция может возвращать три кода ошибки: 1, 0, -1
    //   1 - В процессе разбора json-выражения произошли ошибки, дальнейший
    //       парсинг невозможен;
    //   0 - Пасинг элемента json был успешным;
    //  -1 - Запрашиваемый json-элемент не был найден,  но  при этом разбор
    //       json-выражения может быть продолжен.
    int error() const {return _error;}

private:
    struct StackItem
    {
        enum State
        {
            BeforeStart, // An object/array is in the stack but it is not yet called
                         // by startObject()/startArray().
            Started,     // An object/array is called by  startObject()/startArray().
            Closed       // An array is closed after read all  element,  but  before
                         // call endArray().
        };

        StackItem() = default;
        StackItem(const Value* value, State state, const char* name = 0,
                  int optional = -1)
            : name(name), value(value), state(state), optional(optional)
        {}

        QByteArray name;
        const Value* value;
        State state;
        SizeType index = {0}; // For array iteration

        // Вспомогательное поле, признак опционального параметра.
        // Поле может принимать три значения: 1, 0, -1
        //   1 - Поле опциональное;
        //   0 - Поле обязательное (не опциональное);
        //  -1 - Нет данных о статусе поля.
        int optional = {-1};
    };
    typedef QStack<StackItem> Stack;

    void setError(int val, bool optional = false);
    QByteArray stackFieldName() const;
    QByteArray stackPath() const;
    void next();

private:
    DISABLE_DEFAULT_COPY(Reader)

    Document _document;
    Stack _stack;

    int _error = {0};
    bool _hasParseError = {false};

    quint64 _jsonIndex = {0};
    QByteArray _jsonContent;

    template<typename T>
    friend Reader& detail::operatorAmp(Reader&, T&, detail::not_enum_type<T>);

    template<typename T, typename Compare, typename Allocator>
    friend Reader& detail::readArray(Reader&, lst::List<T, Compare, Allocator>&,
                                     detail::derived_from_clife_base<T>);

    template<typename T, typename Compare, typename Allocator>
    friend Reader& detail::readArray(Reader&, lst::List<T, Compare, Allocator>&,
                                     detail::not_derived_from_clife_base<T>);

    template<typename T> friend Reader& detail::readArray(Reader&, T&);
    template<typename T> friend Reader& detail::readPtr  (Reader&, T&);
};

class Writer
{
public:
    Writer();
    ~Writer();

    // Obtains the serialized JSON string.
    const char* getString() const;

    Writer& member(const char* name, bool /*optional*/ = false);

    Writer& startObject();
    Writer& endObject();

    Writer& startArray(SizeType* size = 0);
    Writer& endArray();

    Writer& setNull();

    Writer& operator& (const bool);
    Writer& operator& (const qint8);
    Writer& operator& (const quint8);
    Writer& operator& (const qint16);
    Writer& operator& (const quint16);
    Writer& operator& (const qint32);
    Writer& operator& (const quint32);
    Writer& operator& (const qint64);
    Writer& operator& (const quint64);
    Writer& operator& (const double);
    Writer& operator& (const float);
    Writer& operator& (const QByteArray&);
    Writer& operator& (const SByteArray&);
    Writer& operator& (const QString&);
    Writer& operator& (const QUuid&);
    Writer& operator& (const QDate&);
    Writer& operator& (const QTime&);
    Writer& operator& (const QDateTime&);
    Writer& operator& (const std::string&);

    template<typename T> Writer& operator& (const T& t);
    template<typename T> Writer& operator& (const QList<T>&);
#if QT_VERSION < 0x060000
    template<typename T> Writer& operator& (const QVector<T>&);
#endif
    template<typename T> Writer& operator& (const clife_ptr<T>&);
    template<typename T> Writer& operator& (const container_ptr<T>&);
    template<int N>      Writer& operator& (const QUuidT<N>&);

    template<typename T> Writer& operator& (const std::list<T>&);
    template<typename T> Writer& operator& (const std::vector<T>&);

    template<typename T, typename Compare, typename Allocator>
    Writer& operator& (const lst::List<T, Compare, Allocator>&);

    bool isReader() const {return false;}
    bool isWriter() const {return true;}

private:
    DISABLE_DEFAULT_COPY(Writer)

    StringBuffer _stream;
    rapidjson::Writer<StringBuffer> _writer;
};


//---------------------------- Reader, Writer --------------------------------

namespace detail {

template<typename T>
Reader& operatorAmp(Reader& r, T& t, not_enum_type<T>)
{
    if (r.stackTopIsOptional() && r.stackTopIsNull())
    {
        t = T{};
        r.next();
    }
    else
        T::jserialize(&t, r);

    return r;
}

template<typename T>
Writer& operatorAmp(Writer& w, T& t, not_enum_type<T> = 0)
{
    T::jserialize(&t, w);
    return w;
}

template<typename T>
Reader& operatorAmp(Reader& r, T& t, is_enum_type<T> = 0)
{
    typedef typename std::underlying_type<T>::type underlying_enum_type;
    static_assert(std::is_same<underlying_enum_type, qint32>::value
               || std::is_same<underlying_enum_type, quint32>::value,
                  "Base type of enum must be 'int' or 'unsigned int'");

    underlying_enum_type val;
    r & val;
    t = static_cast<T>(val);
    return r;
}

template<typename T>
Writer& operatorAmp(Writer& w, const T t, is_enum_type<T> = 0)
{
    typedef typename std::underlying_type<T>::type underlying_enum_type;
    static_assert(std::is_same<underlying_enum_type, qint32>::value
               || std::is_same<underlying_enum_type, quint32>::value,
                  "Base type of enum must be 'int' or 'unsigned int'");

    underlying_enum_type val = static_cast<underlying_enum_type>(t);
    w & val;
    return w;
}

template<typename T, typename Compare, typename Allocator>
Reader& readArray(Reader& r, lst::List<T, Compare, Allocator>& list,
                  derived_from_clife_base<T>)
{
    /* Эта функция используется когда T унаследовано от clife_base */

    if (r.error())
        return r;

    list.clear();
    if (r.stackTopIsNull())
    {
        r.next();
        return r;
    }

    SizeType count;
    r.startArray(count);
    for (SizeType i = 0; i < count; ++i)
    {
        typedef lst::List<T, Compare, Allocator> ListType;
        typename ListType::ValueType* value = list.allocator().create();
        if (value->clife_count() == 0)
            value->add_ref();
        r & (*value);
        list.add(value);
    }
    return r.endArray();
}

template<typename T, typename Compare, typename Allocator>
Reader& readArray(Reader& r, lst::List<T, Compare, Allocator>& list,
                  not_derived_from_clife_base<T>)
{
    /* Эта функция используется когда T НЕ унаследовано от clife_base */

    if (r.error())
        return r;

    list.clear();
    if (r.stackTopIsNull())
    {
        r.next();
        return r;
    }

    SizeType count;
    r.startArray(count);
    for (SizeType i = 0; i < count; ++i)
    {
        typedef lst::List<T, Compare, Allocator> ListType;
        typename ListType::ValueType* value = list.allocator().create();
        r & (*value);
        list.add(value);
    }
    return r.endArray();
}

template<typename T>
Reader& readArray(Reader& r, T& arr)
{
    if (r.error())
        return r;

    arr.clear();
    if (r.stackTopIsNull())
    {
        r.next();
        return r;
    }

    SizeType count;
    r.startArray(count);
    for (SizeType i = 0; i < count; ++i)
    {
        typename T::value_type t;
        r & t;
        arr.push_back(t);
    }
    return r.endArray();
}

template<typename T>
Writer& writeArray(Writer& w, const T& arr)
{
    w.startArray();
    for (decltype(arr.size()) i = 0; i < arr.size(); ++i)
        w & arr[i];

    return w.endArray();
}

template<typename T>
Reader& readPtr(Reader& r, T& ptr)
{
    if (r.error())
        return r;

    if (r.stackTopIsNull())
    {
        ptr.reset();
        r.next();
    }
    else if (r.stackTopIsObject())
    {
        typedef T Ptr;
        typedef typename Ptr::element_t element_t;
        if (ptr.empty())
            ptr = Ptr(new element_t());
        r & (*ptr);
    }
    else
    {
        r.setError(1);
        alog::logger().error(alog_line_location, "JSerialize")
            << "Stack top is not object"
            << ". Field: " << r.stackFieldName()
            << ". Stack path: " << r.stackPath()
            << ". JIndex: " << r.jsonIndex();
    }
    return r;
}

template<typename T>
Writer& writePtr(Writer& w, const T& ptr)
{
    if (ptr)
        w & (*ptr);
    else
        w.setNull();

    return w;
}

} // namespace detail

template<typename T>
Reader& Reader::operator& (T& t)
{
    if (error())
        return *this;

    Reader& r = const_cast<Reader&>(*this);
    return detail::operatorAmp(r, t);
}

template<typename T>
Writer& Writer::operator& (const T& ct)
{
    Writer& w = const_cast<Writer&>(*this);
    T& t = const_cast<T&>(ct);
    return detail::operatorAmp(w, t);
}

template<typename T>
Reader& Reader::operator& (QList<T>& l)
{
    Reader& r = const_cast<Reader&>(*this);
    return detail::readArray(r, l);
}

template<typename T>
Writer& Writer::operator& (const QList<T>& l)
{
    Writer& w = const_cast<Writer&>(*this);
    return detail::writeArray(w, l);
}

#if QT_VERSION < 0x060000
template<typename T>
Reader& Reader::operator& (QVector<T>& v)
{
    Reader& r = const_cast<Reader&>(*this);
    return detail::readArray(r, v);
}

template<typename T>
Writer& Writer::operator& (const QVector<T>& v)
{
    Writer& w = const_cast<Writer&>(*this);
    return detail::writeArray(w, v);
}
#endif

template<typename T>
Reader& Reader::operator& (clife_ptr<T>& ptr)
{
    static_assert(std::is_base_of<clife_base, T>::value,
                  "Class T must be derived from clife_base");

    Reader& r = const_cast<Reader&>(*this);
    return detail::readPtr(r, ptr);
}

template<typename T>
Writer& Writer::operator& (const clife_ptr<T>& ptr)
{
    Writer& w = const_cast<Writer&>(*this);
    return detail::writePtr(w, ptr);
}

template<typename T>
Reader& Reader::operator& (container_ptr<T>& ptr)
{
    Reader& r = const_cast<Reader&>(*this);
    return detail::readPtr(r, ptr);
}

template<typename T>
Writer& Writer::operator& (const container_ptr<T>& ptr)
{
    Writer& w = const_cast<Writer&>(*this);
    return detail::writePtr(w, ptr);
}

template<int N>
Reader& Reader::operator& (QUuidT<N>& uuid)
{
    return this->operator& (static_cast<QUuid&>(uuid));
}

template<int N>
Writer& Writer::operator& (const QUuidT<N>& uuid)
{
    return this->operator& (static_cast<const QUuid&>(uuid));
}

template<typename T>
Reader& Reader::operator& (std::list<T>& l)
{
    Reader& r = const_cast<Reader&>(*this);
    return detail::readArray(r, l);
}

template<typename T>
Writer& Writer::operator& (const std::list<T>& l)
{
    Writer& w = const_cast<Writer&>(*this);
    return detail::writeArray(w, l);
}

template<typename T>
Reader& Reader::operator& (std::vector<T>& v)
{
    Reader& r = const_cast<Reader&>(*this);
    return detail::readArray(r, v);
}

template<typename T>
Writer& Writer::operator& (const std::vector<T>& v)
{
    Writer& w = const_cast<Writer&>(*this);
    return detail::writeArray(w, v);
}

template<typename T, typename Compare, typename Allocator>
Reader& Reader::operator& (lst::List<T, Compare, Allocator>& l)
{
    Reader& r = const_cast<Reader&>(*this);
    return detail::readArray(r, l);
}

template<typename T, typename Compare, typename Allocator>
Writer& Writer::operator& (const lst::List<T, Compare, Allocator>& l)
{
    Writer& w = const_cast<Writer&>(*this);
    return detail::writeArray(w, l);
}

//-------------------------------- Functions ---------------------------------

template<typename GenericValueT>
bool stringEqual(const typename GenericValueT::Ch* a, const GenericValueT& b)
{
    RAPIDJSON_ASSERT(b.IsString());

    const SizeType l1 = strlen(a);
    const SizeType l2 = b.GetStringLength();
    if (l1 != l2)
        return false;

    const typename GenericValueT::Ch* const b_ = b.GetString();
    if (a == b_)
        return true;

    return (std::memcmp(a, b_, sizeof(typename GenericValueT::Ch) * l1) == 0);
}

} // namespace pproto::serialize::json

/**
  Макросы для работы с функциями сериализации toJson(), fromJson()
*/
#define J_SERIALIZE_FUNC \
    QByteArray toJson() const { \
        pproto::serialize::json::Writer writer; \
        jserialize(this, writer); \
        return QByteArray(writer.getString()); \
    } \
    pproto::SResult fromJson(const QByteArray& json) { \
        pproto::serialize::json::Reader reader; \
        if (reader.parse(json)) \
            jserialize(this, reader); \
        return reader.result(); \
    }

#define DECLARE_J_SERIALIZE_FUNC \
    J_SERIALIZE_FUNC \
    template<typename This, typename Packer> \
    static Packer& jserialize(const This*, Packer&);

#define J_SERIALIZE_BEGIN \
    J_SERIALIZE_FUNC \
    template<typename This, typename Packer> \
    static Packer& jserialize(const This* ct, Packer& p) { \
        This* t = const_cast<This*>(ct); \
        p.startObject();

#define J_SERIALIZE_EXTERN_BEGIN(CLASS) \
    template<typename This, typename Packer> \
    Packer& CLASS::jserialize(const This* ct, Packer& p) { \
        This* t = const_cast<This*>(ct); \
        p.startObject();

#define J_SERIALIZE_ITEM(FIELD) \
        p.member(#FIELD) & t->FIELD;

#define J_SERIALIZE_OPT(FIELD) \
        p.member(#FIELD, true) & t->FIELD;

#define J_SERIALIZE_MAP_ITEM(FIELD_NAME, FIELD) \
        p.member(FIELD_NAME) & t->FIELD;

#define J_SERIALIZE_MAP_OPT(FIELD_NAME, FIELD) \
        p.member(FIELD_NAME, true) & t->FIELD;

#define J_SERIALIZE_END \
        return p.endObject(); \
    }

#define J_SERIALIZE_BASE_BEGIN \
    template<typename Packer> \
    void jserializeBase(Packer& p) { \
        decltype(this) t = this;
        //std::remove_const_t<decltype(this)> t = const_cast<std::remove_const_t<decltype(this)>>(this);

#define J_SERIALIZE_BASE_END \
    }

#define J_SERIALIZE_BASE(CLASS) \
    static_cast<CLASS*>(t)->jserializeBase(p);

#define J_SERIALIZE_ONE(FIELD) \
    J_SERIALIZE_BEGIN \
    J_SERIALIZE_ITEM(FIELD) \
    J_SERIALIZE_END

#define J_SERIALIZE_MAP_ONE(FIELD_NAME, FIELD) \
    J_SERIALIZE_BEGIN \
    J_SERIALIZE_MAP_ITEM(FIELD_NAME, FIELD) \
    J_SERIALIZE_END

#define J_SERIALIZE_BASE_ONE \
    J_SERIALIZE_BEGIN \
    t->jserializeBase(p); \
    J_SERIALIZE_END
