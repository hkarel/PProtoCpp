#pragma once

#include "shared/qt/identity.h"
#include "commands/base.h"

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
    //     J_SERIALIZE_MAP_OPT( "id"         , id        )
    //     J_SERIALIZE_MAP_OPT( "index"      , index     )
    //     J_SERIALIZE_MAP_OPT( "name"       , name      )
    //     J_SERIALIZE_MAP_OPT( "find_field" , findField )
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

        if (!t->isNull())
            J_SERIALIZE_MAP_ITEM( "find_field", findField )
    }
    else if (p.isReader())
    {
        J_SERIALIZE_MAP_OPT( "id"         , id        )
        J_SERIALIZE_MAP_OPT( "index"      , index     )
        J_SERIALIZE_MAP_OPT( "name"       , name      )
        J_SERIALIZE_MAP_OPT( "find_field" , findField )
    }
    return p.endObject();
}

} // namespace pproto::data
