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
*****************************************************************************/

#pragma once

#include <QByteArray>
#include <QDataStream>

namespace pproto {
namespace serialize {

/**
  Структура ByteArray (SByteArray) используется для переопределения потокового
  оператора '>>' для QByteArray в механизме бинарной сериализации.
  Переопределенный  оператор  '>>'  менее универсальный,  но дает преимущество
  по производительности.
  Рекомендуется использовать ByteArray (SByteArray) вместо QByteArray
  при создании сериализуемых структур данных.
*/
struct ByteArray : QByteArray
{
    ByteArray() noexcept : QByteArray() {}
    ByteArray(const char* c, int size = -1) noexcept : QByteArray(c, size) {}
    ByteArray(const QByteArray& ba) noexcept : QByteArray(ba) {}
    ByteArray(const ByteArray&  ba) noexcept : QByteArray(ba) {}

    ByteArray& operator= (const ByteArray& ba) noexcept
    {
        QByteArray::operator= (ba);
        return *this;
    }
    ByteArray& operator= (const QByteArray& ba) noexcept
    {
        QByteArray::operator= (ba);
        return *this;
    }
    ByteArray& operator= (const char* c) noexcept
    {
        QByteArray::operator= (c);
        return *this;
    }
};

// Выполняет чтение QByteArray из потока QDataStream. С точки зрения скорости
// работы эта функция более оптимальна, чем потоковый оператор для QByteArray.
QByteArray readByteArray(QDataStream&);

inline void readByteArray(QDataStream& s, QByteArray& ba)
    {ba = readByteArray(s);}

inline QDataStream& operator>> (QDataStream& s, ByteArray& ba)
    {readByteArray(s, ba); return s;}

inline QDataStream& operator<< (QDataStream& s, const ByteArray& ba)
    {return operator<< (s, static_cast<const QByteArray&>(ba));}

} // namespace serialize

using SByteArray = serialize::ByteArray;

} // namespace pproto
