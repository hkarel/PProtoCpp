/*****************************************************************************
  The MIT License

  Copyright © 2017 Pavel Karelin (hkarel), <hkarel@yandex.ru>

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
#include "logger_operators.h"

#include "shared/list.h"
#include "shared/defmac.h"
#include "shared/logger/logger.h"
#include <exception>

#define log_error_m   alog::logger().error   (alog_line_location, "FuncInvok")
#define log_warn_m    alog::logger().warn    (alog_line_location, "FuncInvok")
#define log_info_m    alog::logger().info    (alog_line_location, "FuncInvok")
#define log_verbose_m alog::logger().verbose (alog_line_location, "FuncInvok")
#define log_debug_m   alog::logger().debug   (alog_line_location, "FuncInvok")
#define log_debug2_m  alog::logger().debug2  (alog_line_location, "FuncInvok")

namespace pproto {

/**
  В классе реализован механизм для связывания команды с функцией-обработчиком
  сообщения
*/
class FunctionInvoker
{
    struct BaseItem
    {
        QUuidEx command;
        virtual ~BaseItem() = default;
        virtual void call(const Message::Ptr&) const = 0;

        struct Compare
        {
            int operator() (const BaseItem* item1, const BaseItem* item2) const
            {
                return QUuidEx::compare(item1->command, item2->command);
            }
            int operator() (const QUuidEx* command, const BaseItem* item2) const
            {
                return QUuidEx::compare(*command, item2->command);
            }
        };
        typedef lst::List<BaseItem, Compare> List;
    };

    template<typename T>
    struct Item : BaseItem
    {
        typedef void (T::*Func)(const Message::Ptr&);
        T* instance;
        Func func;
        void call(const Message::Ptr& message) const
        {
            try
            {
                (instance->*func)(message);
            }
            catch (std::exception& e)
            {
                log_error_m
                    << "Handler of command " << CommandNameLog(message->command())
                    << " throw a exception. Detail: " << e.what();
            }
            catch (...)
            {
                log_error_m
                    << "Handler of command " << CommandNameLog(message->command())
                    << " throw a exception. Unknown error";
            }
        }
    };

    template<typename T>
    struct ItemConst : BaseItem
    {
        typedef void (T::*Func)(const Message::Ptr&) const;
        T* instance;
        Func func;
        void call(const Message::Ptr& message) const
        {
            try
            {
                (instance->*func)(message);
            }
            catch (std::exception& e)
            {
                log_error_m
                    << "Handler of command " << CommandNameLog(message->command())
                    << " throw a exception. Detail: " << e.what();
            }
            catch (...)
            {
                log_error_m
                    << "Handler of command " << CommandNameLog(message->command())
                    << " throw a exception. Unknown error";
            }
        }
    };

    void addItem(BaseItem* item)
    {
        if (_functions.empty())
        {
            _functions.add(item);
            _functions.sort();
        }
        else
        {
            lst::FindResult fr = _functions.find(item);
            if (fr.success())
            {
                log_warn_m << "Redefined handler for command "
                           << CommandNameLog(item->command);
                _functions.replace(fr.index(), item, true);
            }
            else
                _functions.addInSort(item, fr);
        }
    }

public:
    FunctionInvoker() = default;

    template<typename T>
    void registration(const QUuidEx& command,
                      void (T::*func)(const Message::Ptr&), T* instance)
    {
        Item<T>* item = new Item<T>;
        item->command = command;
        item->instance = instance;
        item->func = func;
        addItem(item);
    }

    template<typename T>
    void registration(const QUuidEx& command,
                      void (T::*func)(const Message::Ptr&) const, T* instance)
    {
        ItemConst<T>* item = new ItemConst<T>;
        item->command = command;
        item->instance = instance;
        item->func = func;
        addItem(item);
    }

    bool containsCommand(const QUuidEx& command)
    {
        return _functions.findRef(command).success();
    }

    lst::FindResult findCommand(const QUuidEx& command)
    {
        return _functions.findRef(command);
    }

    void call(const Message::Ptr& message)
    {
        if (lst::FindResult fr = _functions.findRef(message->command()))
            _functions.item(fr.index())->call(message);
    }

    void call(const Message::Ptr& message, const lst::FindResult& fr)
    {
        if (fr.success())
            _functions.item(fr.index())->call(message);
    }

private:
    DISABLE_DEFAULT_COPY(FunctionInvoker)
    BaseItem::List _functions;
};

} // namespace pproto

#undef log_error_m
#undef log_warn_m
#undef log_info_m
#undef log_verbose_m
#undef log_debug_m
#undef log_debug2_m
