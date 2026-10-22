/*****************************************************************************
  The MIT License

  Copyright © 2026 Pavel Karelin (hkarel), <hkarel@yandex.ru>

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
#include "shared/qt/identity.h"

#include <tuple>

namespace pproto::data {

/**
  Структура используется для идентификации различных примитивов. Идентификация
  может осуществляться по одному или паре полей:  id,  index,  name.  За выбор
  полей для идентификации примитива отвечает пользователь API.  Смотри базовый
  класс Identity
*/
struct IdentityS /*Identity Serialize*/ : Identity
{
    IdentityS() = default;
    IdentityS(const IdentityS&) = default;

    IdentityS& operator= (IdentityS&&) = default;
    IdentityS& operator= (const IdentityS&) = default;

    IdentityS(const Identity&);
    IdentityS& operator= (const Identity&);

    DECLARE_B_SERIALIZE_FUNC
    DECLARE_J_SERIALIZE_FUNC

    // J_SERIALIZE_BEGIN
    //     J_SERIALIZE_MAP_OPT( "id"    , id    )
    //     J_SERIALIZE_MAP_OPT( "index" , index )
    //     J_SERIALIZE_MAP_OPT( "name"  , name  )
    // J_SERIALIZE_END
};

template<typename This, typename Packer>
Packer& IdentityS::jserialize(const This* ct, Packer& p)
{
    This* t = const_cast<This*>(ct);

    p.startObject();

    if (p.isWriter())
    {
        if (!t->id.isNull())
            J_SERIALIZE_MAP_ITEM( "id", id )

        if (t->index >= 0)
            J_SERIALIZE_MAP_ITEM( "index", index )

        if (!t->name.isEmpty())
            J_SERIALIZE_MAP_ITEM( "name", name )
    }
    else if (p.isReader())
    {
        J_SERIALIZE_MAP_OPT( "id"    , id    )
        J_SERIALIZE_MAP_OPT( "index" , index )
        J_SERIALIZE_MAP_OPT( "name"  , name  )
    }
    return p.endObject();
}

} // namespace pproto::data
