/*****************************************************************************
  The MIT License

  Copyright © 2012 Pavel Karelin (hkarel), <hkarel@yandex.ru>

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

#include "routing.h"

#include "shared/logger/logger.h"
#include "shared/qt/logger_operators.h"
#include "pproto/logger_operators.h"

#define log_error_m   alog::logger().error   (alog_line_location, "Routing")
#define log_warn_m    alog::logger().warn    (alog_line_location, "Routing")
#define log_info_m    alog::logger().info    (alog_line_location, "Routing")
#define log_verbose_m alog::logger().verbose (alog_line_location, "Routing")
#define log_debug_m   alog::logger().debug   (alog_line_location, "Routing")
#define log_debug2_m  alog::logger().debug2  (alog_line_location, "Routing")

namespace pproto {

bool RouteCommands::forwarding(const Message::Ptr& message)
{
    if (!commands.contains(message->command()))
        return false;

    quint64 curTime = std::time(nullptr);
    auto removeExpiredTime = [curTime](Point& point)
    {
        for (int i = 0; i < point.transferredCommands.count(); ++i)
            if (point.transferredCommands[i].second < curTime)
                point.transferredCommands.remove(i--);
    };
    removeExpiredTime(point1);
    removeExpiredTime(point2);

    auto func = [&message, curTime](Point& p1, Point& p2) -> int
    {
        if (p1.socket)
        {
            if (p1.socket->socketDescriptor() == message->socketDescriptor())
            {
                if (!p2.socket)
                {
                    log_error_m << "Unable forwarding command " << CommandNameLog(message->command())
                                << " from socket '" << p1.name << "'"
                                << " to socket '" << p2.name << "'"
                                << "; Socket '" << p2.name << "' is not available";

                    data::Error error;
                    error.commandId = message->command();
                    error.messageId = message->id();
                    error.description = "Unable forwarding message to socket '" + p2.name + "'"
                                        ". Socket is not available";

                    Message::Ptr m = createMessage(error, {message->contentFormat()});
                    p1.socket->send(m);
                    return 0;
                }

                if (message->type() == Message::Type::Command)
                {
                    quint64 time = (message->maxTimeLife() != 0)
                                   ? message->maxTimeLife() : (curTime + 10);
                    p1.transferredCommands.append(qMakePair(message->id(), time));
                    p2.socket->send(message);
                    return 1;
                }
                else if (message->type() == Message::Type::Answer)
                {
                    bool found = false;
                    for (int i = 0; i < p2.transferredCommands.count(); ++i)
                        if (p2.transferredCommands[i].first == message->id())
                        {
                            found = true;
                            p2.transferredCommands.remove(i);
                            break;
                        }

                    if (found)
                    {
                        p2.socket->send(message);
                        return 1;
                    }

                    log_error_m << "Unable forwarding command " << CommandNameLog(message->command())
                                << " from socket '" << p1.name << "'"
                                << " to socket '" << p2.name << "'"
                                << ". Timeout has expired. Message id: " << message->id();

                    data::Error error;
                    error.commandId = message->command();
                    error.messageId = message->id();
                    error.description = "Unable forwarding message to socket '" + p2.name + "'"
                                        ". Timeout for this message has expired";

                    Message::Ptr m = createMessage(error, {message->contentFormat()});
                    p1.socket->send(m);
                    return 0;
                }
                else // Message::Type::Event
                {
                    break_point
                    p2.socket->send(message);
                    return 1;
                }
            }
        }
        return -1;
    };

    int res = func(point1, point2);
    if (res != -1)
        return bool(res);

    res = func(point2, point1);
    if (res != -1)
        return bool(res);

    log_error_m << "Failed forwarding message " << CommandNameLog(message->command());
    return false;
}

} // namespace pproto
