/*****************************************************************************
  The MIT License

  Copyright © 2025 Pavel Karelin (hkarel), <hkarel@yandex.ru>

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

#include "commands/base.h"
#include <time.h>

namespace pproto::data {

/**
  Структура общего назначения, используется для представления timespec
*/
struct TimeSpec
{
    TimeSpec() = default;
    TimeSpec(const TimeSpec&) = default;
    TimeSpec(const timespec& ts) : tv_sec(ts.tv_sec), tv_nsec(ts.tv_nsec) {}

    TimeSpec& operator= (TimeSpec&&) = default;
    TimeSpec& operator= (const TimeSpec&) = default;

    TimeSpec& operator= (const timespec& ts) {
        tv_sec = ts.tv_sec;
        tv_nsec = ts.tv_nsec;
        return *this;
    }

    operator timespec() const {return {tv_sec, tv_nsec};}

    qint64 tv_sec  = {0};
    qint64 tv_nsec = {0};

#ifdef PPROTO_QBINARY_SERIALIZE
    DECLARE_B_SERIALIZE_FUNC
#endif

#ifdef PPROTO_JSON_SERIALIZE
    J_SERIALIZE_BEGIN
        J_SERIALIZE_ITEM( tv_sec  )
        J_SERIALIZE_ITEM( tv_nsec )
    J_SERIALIZE_END
#endif
};

} // namespace pproto::data
