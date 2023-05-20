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

#pragma once

#include "message.h"
#include "shared/logger/logger.h"
#include "shared/qt/quuidex.h"

namespace pproto {

/**
  Вспомогательная структура, используется для отправки в лог идентификатора
  команды вместе с именем.
*/
struct CommandNameLog
{
    const QUuidEx& command;
    bool onlyCommandName;
    CommandNameLog(const QUuidEx& command, bool onlyCommandName = true)
        : command(command),
          onlyCommandName(onlyCommandName)
    {}
};
} // namespace pproto

namespace alog {

Line& operator<< (Line&, const pproto::HostPoint&);
Line& operator<< (Line&, const pproto::CommandNameLog&);
Line& operator<< (Line&, pproto::SerializeFormat);
Line& operator<< (Line&, pproto::Message::Type);
Line& operator<< (Line&, pproto::Message::ExecStatus);

} // namespace alog
