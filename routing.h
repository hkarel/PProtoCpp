/*****************************************************************************
  The MIT License

  Copyright © 2022 Pavel Karelin (hkarel), <hkarel@yandex.ru>

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
#include <QtCore>

#include "shared/defmac.h"
#include "shared/simple_timer.h"
#include "shared/qt/quuidex.h"
#include "pproto/message.h"
#include "pproto/transport/tcp.h"

namespace pproto {

/**
  Простой механизм перенаправления команд между сокетами
*/
struct RouteCommands
{
    QSet<QUuidEx> commands;

    struct Point
    {
        QString name;
        transport::tcp::Socket::Ptr socket;

        // Идентификаторы переданных команд и время ожидания ответа
        QVector<QPair<QUuidEx, quint64/*Time*/>> transferredCommands;
    };
    Point point1;
    Point point2;

    bool forwarding(const Message::Ptr&);
};

} // namespace pproto
