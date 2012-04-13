#include <ironbeepp/connection_data.hpp>
#include <ironbeepp/connection.hpp>
#include <ironbeepp/engine.hpp>
#include <ironbeepp/memory_pool.hpp>

#include <ironbee/engine.h>

using namespace std;

namespace IronBee {

// ConstConnectionData

ConstConnectionData::ConstConnectionData() :
    m_ib(NULL)
{
    // nop
}

ConstConnectionData::ConstConnectionData(ib_type ib_connection_data) :
    m_ib(ib_connection_data)
{
    // nop
}

Engine ConstConnectionData::engine() const
{
    return Engine(ib()->ib);
}

Connection ConstConnectionData::connection() const
{
    return Connection(ib()->conn);
}

size_t ConstConnectionData::allocated() const
{
    return ib()->dalloc;
}

size_t ConstConnectionData::length() const
{
    return ib()->dlen;
}

char* ConstConnectionData::data() const
{
    return reinterpret_cast<char*>(ib()->data);
}

// ConnectionData

ConnectionData ConnectionData::remove_const(
    ConstConnectionData connection_data
)
{
    return ConnectionData(const_cast<ib_type>(connection_data.ib()));
}

ConnectionData::ConnectionData() :
    m_ib(NULL)
{
    // nop
}

ConnectionData::ConnectionData(ib_type ib_connection_data) :
    ConstConnectionData(ib_connection_data),
    m_ib(ib_connection_data)
{
    // nop
}

std::ostream& operator<<(std::ostream& o, const ConstConnectionData& connection_data)
{
    if (! connection_data) {
        o << "IronBee::ConnectionData[!singular!]";
    } else {
        o << "IronBee::ConnectionData["
          << string(connection_data.data(), connection_data.length())
          << "]";
    }
    return o;
}

} // IronBee
