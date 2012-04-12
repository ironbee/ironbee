#include <ironbeepp/connection.hpp>
#include <ironbeepp/engine.hpp>
#include <ironbeepp/memory_pool.hpp>
#include <ironbeepp/transaction.hpp>
#include <ironbeepp/context.hpp>
#include <ironbeepp/clock.hpp>

namespace IronBee {

// ConstConnection

ConstConnection::ConstConnection() :
    m_ib(NULL)
{
    // nop
}

ConstConnection::ConstConnection(ib_type ib_connection) :
    m_ib(ib_connection)
{
    // nop
}

Engine ConstConnection::engine() const
{
    return Engine(ib()->ib);
}

MemoryPool ConstConnection::memory_pool() const
{
    return MemoryPool(ib()->mp);
}

Context ConstConnection::context() const
{
    return Context(ib()->ctx);
}

boost::posix_time::ptime ConstConnection::started_time() const
{
    return ib_to_ptime(ib()->t.started);
}

boost::posix_time::ptime ConstConnection::finished_time() const
{
    return ib_to_ptime(ib()->t.finished);
}

const char* ConstConnection::remote_ip_string() const
{
    return ib()->remote_ipstr;
}

uint16_t ConstConnection::remote_port() const
{
    return ib()->remote_port;
}

const char* ConstConnection::local_ip_string() const
{
    return ib()->local_ipstr;
}

uint16_t ConstConnection::local_port() const
{
    return ib()->local_port;
}

size_t ConstConnection::transaction_count() const
{
    return ib()->tx_count;
}

Transaction ConstConnection::first_transaction() const
{
    return Transaction(ib()->tx_first);
}

Transaction ConstConnection::transaction() const
{
    return Transaction(ib()->tx);
}

Transaction ConstConnection::last_transaction() const
{
    return Transaction(ib()->tx_last);
}

uint32_t ConstConnection::flags() const
{
    return ib()->flags;
}

// Connection

Connection Connection::remove_const(ConstConnection connection)
{
    return Connection(const_cast<ib_type>(connection.ib()));
}

Connection::Connection() :
    m_ib(NULL)
{
    // nop
}

Connection::Connection(ib_type ib_connection) :
    ConstConnection(ib_connection),
    m_ib(ib_connection)
{
    // nop
}

std::ostream& operator<<(std::ostream& o, const ConstConnection& connection)
{
    if (! connection) {
        o << "IronBee::Connection[!singular!]";
    } else {
        o << "IronBee::Connection["
          << connection.remote_ip_string() << ":" << connection.remote_port()
          << connection.local_ip_string() << ":" << connection.local_port()
          << "]";
    }
    return o;
}

} // IronBee
