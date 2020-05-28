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

#include "logger_operators.h"
#include "commands_pool.h"
#include "shared/qt/logger_operators.h"

namespace alog {

Line& operator<< (Line&  line, const communication::HostPoint& hp)
{
    if (line.toLogger())
        line << hp.address() << ":" << hp.port();

    return line;
}

Line& operator<< (Line& line, const communication::CommandNameLog& cnl)
{
    if (line.toLogger())
    {
        QByteArray commandName = communication::command::pool().commandName(cnl.command);
        if (!commandName.isEmpty())
        {
            line << commandName;
            if (!cnl.onlyCommandName)
                line << "/";
        }
        if (commandName.isEmpty() || !cnl.onlyCommandName)
            line << cnl.command;
    }
    return line;
}

Line& operator<< (Line& line, communication::Message::Type type)
{
    if (line.toLogger())
        switch (type)
        {
            case communication::Message::Type::Command:
                line << "Command";
                break;

            case communication::Message::Type::Answer:
                line << "Answer";
                break;

            case communication::Message::Type::Event:
                line << "Event";
                break;

            default:
                line << "Unknown";
        }

    return line;
}

Line& operator<< (Line& line, communication::Message::ExecStatus execStatus)
{
    if (line.toLogger())
        switch (execStatus)
        {
            case communication::Message::ExecStatus::Success:
                line << "Success";
                break;

            case communication::Message::ExecStatus::Failed:
                line << "Failed";
                break;

            case communication::Message::ExecStatus::Error:
                line << "Error";
                break;

            default:
                line << "Unknown";
        }

    return line;
}

} // namespace alog
