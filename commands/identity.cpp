#include "identity.h"

namespace pproto::data {

IdentityS::IdentityS(const Identity& identity)
{
    this->operator= (identity);
}

IdentityS& IdentityS::operator= (const Identity& identity)
{
    static_cast<Identity&>(*this) = identity;
    return *this;
}

void IdentityS::toRaw(bserial::DataStream& stream) const
{
    B_SERIALIZE_V1
    stream << id;
    stream << index;
    stream << name;
    stream << findField;
    B_SERIALIZE_RETURN
}

void IdentityS::fromRaw(const bserial::RawVector& vect)
{
    B_DESERIALIZE_V1(vect)
    stream >> id;
    stream >> index;
    stream >> name;
    stream >> findField;
    B_DESERIALIZE_END
}

} // namespace pproto::data
