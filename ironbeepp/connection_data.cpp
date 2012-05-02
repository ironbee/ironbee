#include <ironbeepp/connection_data.hpp>
#include <ironbeepp/connection.hpp>
#include <ironbeepp/engine.hpp>
#include <ironbeepp/memory_pool.hpp>
#include <ironbeepp/internal/throw.hpp>

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

Connection ConstConnectionData::connection() const
{
    return Connection(ib()->conn);
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

void ConnectionData::set_data(char* data) const
{
    ib()->data = reinterpret_cast<uint8_t*>(data);
}

void ConnectionData::set_length(size_t length) const
{
    ib()->dlen = length;
}

ConnectionData ConnectionData::create(Connection connection, size_t size)
{
    ib_conndata_t* ib_conndata;

    Internal::throw_if_error(
        ib_conn_data_create(connection.ib(), &ib_conndata, size)
    );

    return ConnectionData(ib_conndata);
}

ConnectionData ConnectionData::create(
    Connection  connection,
    const char* data,
    size_t      length
)
{
    ConnectionData cd = create(connection, length);
    copy(data, data+length, cd.data());
    cd.set_length(length);
    return cd;
}

ConnectionData ConnectionData::create(
    Connection         connection,
    const std::string& data
)
{
    return create(connection, data.data(), data.length());
}

ConnectionData ConnectionData::create_alias(
    Connection  connection,
    char*       data,
    size_t      length
)
{
    ConnectionData cd = create(connection);
    cd.set_data(data);
    cd.set_length(length);
    return cd;
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
