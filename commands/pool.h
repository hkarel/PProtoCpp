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

#include "shared/defmac.h"
#include "shared/safe_singleton.h"
#include "shared/qt/quuidex.h"
#include <QtCore>

namespace pproto::command {

/**
  Пул идентификаторов команд. Все функции в пуле НЕ потокозащищенные, несмотря
  на это к ним можно обращаться из разных потоков. Такое поведение обусловлено
  тем, что список идентификаторов заполняется  единожды  при старте программы,
  и потом более не меняется
*/
class Pool
{
public:
    // Возвращает TRUE в случае если в пуле находятся только уникальные команды.
    // Если команды не уникальны в лог выводится сообщение об этом
    bool checkUnique() const;

    // Добавляет команду в пул
    void add(const QUuidEx* command, const char* commandName, bool multiproc);

    // Возвращает список идентификаторов команд
    QVector<QUuidEx> commands() const;

    // Возвращает строковое наименование команды по ее идентификатору
    const char* commandName(const QUuidEx& command) const;

    // Возвращает значение большее нуля если команда существует в пуле команд.
    // Если признак  multiproc  для  команды  равен  FALSE - будет возвращено
    // значение 1, если multiproc равен TRUE - будет возвращено значение 2
    quint32 commandExists(const QUuidEx& command) const;

    // Возвращает TRUE когда команда есть в пуле команд, и для нее установлен
    // признак singlproc
    bool commandIsSinglproc(const QUuidEx& command) const;

    // Возвращает TRUE когда команда есть в пуле команд, и для нее установлен
    // признак multiproc
    bool commandIsMultiproc(const QUuidEx& command) const;

    // Используется для регистрации команд в пуле. Параметр multiproc опреде-
    // ляет как команда будет восприниматься обработчиками команд. Если пара-
    // метр multiproc равен TRUE, то предполагается, что команда  может быть
    // обработана несколькими обработчиками. В этом случае обработчик не дол-
    // жен устанавливать для сообщения статус processed
    struct Registry : public QUuidEx
    {
        Registry(const char* uuidStr, const char* commandName, bool multiproc);
    };

    struct CommandTraits
    {
        const char* commandName = "";

        // Параметр определяет возможность для команды быть обработанной
        // несколькими обработчиками
        const bool multiproc = {false};

        CommandTraits(const char* commandName, bool multiproc);
        bool operator== (const CommandTraits&) const;
    };

private:
    Pool() = default;
    DISABLE_DEFAULT_COPY(Pool)

private:
    // Параметр используется для проверки уникальности идентификаторов команд.
    // Если идентификатор окажется не уникальным, то QSet<СommandTraits> будет
    // содержать более одного значения
    QMap<QUuidEx, QSet<CommandTraits>> _map;

    template<typename T, int> friend T& safe::singleton();
};

Pool& pool();

uint qHash(const Pool::CommandTraits&);
inline bool checkUnique() {return pool().checkUnique();}

//----------------------------------- Pool -----------------------------------

inline bool Pool::commandIsSinglproc(const QUuidEx& command) const
{
    return (commandExists(command) == 1);
}

inline bool Pool::commandIsMultiproc(const QUuidEx& command) const
{
    return (commandExists(command) == 2);
}

} // namespace pproto::command
