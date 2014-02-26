#include <ironbeepp/connection.hpp>
#include <ironbeepp/engine.hpp>
#include <ironbeepp/memory_manager.hpp>
#include <ironbeepp/transaction.hpp>
#include <ironbeepp/context.hpp>
#include <ironbeepp/clock.hpp>
#include <ironbeepp/throw.hpp>

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

MemoryManager ConstConnection::memory_manager() const
{
    return MemoryManager(ib()->mm);
}

const char* ConstConnection::id() const
{
    return ib()->id;
}

Context ConstConnection::context() const
{
    return Context(ib()->ctx);
}

boost::posix_time::ptime ConstConnection::started_time() const
{
    return ib_to_ptime(ib()->tv_created);
}

boost::posix_time::ptime ConstConnection::finished_time() const
{
    return ib_to_ptime(ib()->tv_created, (ib()->t.finished - ib()->t.started));
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

ib_flags_t ConstConnection::flags() const
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

Connection Connection::create(Engine engine)
{
    ib_conn_t* ib_conn;

    throw_if_error(
        ib_conn_create(engine.ib(), &ib_conn, NULL)
    );

    return Connection(ib_conn);
}

void Connection::set_remote_ip_string(const char* ip) const
{
    ib()->remote_ipstr = ip;
}

void Connection::set_remote_port(uint16_t port) const
{
    ib()->remote_port = port;
}

void Connection::set_local_ip_string(const char* ip) const
{
    ib()->local_ipstr = ip;
}

void Connection::set_local_port(uint16_t port) const
{
    ib()->local_port = port;
}

void Connection::destroy() const
{
    ib_conn_destroy(ib());
}

std::ostream& operator<<(std::ostream& o, const ConstConnection& connection)
{
    if (! connection) {
        o << "IronBee::Connection[!singular!]";
    } else {
        o << "IronBee::Connection["
          << connection.id() << " "
          << connection.remote_ip_string() << ":" << connection.remote_port()
          << connection.local_ip_string() << ":" << connection.local_port()
          << "]";
    }
    return o;
}

} // IronBee
