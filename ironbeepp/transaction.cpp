#include <ironbeepp/transaction.hpp>
#include <ironbeepp/connection.hpp>
#include <ironbeepp/context.hpp>
#include <ironbeepp/engine.hpp>
#include <ironbeepp/memory_pool.hpp>
#include <ironbeepp/clock.hpp>

namespace IronBee {

// ConstTransaction

ConstTransaction::ConstTransaction() :
    m_ib(NULL)
{
    // nop
}

ConstTransaction::ConstTransaction(ib_type ib_transaction) :
    m_ib(ib_transaction)
{
    // nop
}

Engine ConstTransaction::engine() const
{
    return Engine(ib()->ib);
}

MemoryPool ConstTransaction::memory_pool() const
{
    return MemoryPool(ib()->mp);
}

const char* ConstTransaction::id() const
{
    return ib()->id;
}

Connection ConstTransaction::connection() const
{
    return Connection(ib()->conn);
}

Context ConstTransaction::context() const
{
    return Context(ib()->ctx);
}

boost::posix_time::ptime ConstTransaction::started() const
{
    return ib_to_ptime(ib()->t.started);
}

boost::posix_time::ptime ConstTransaction::request_started() const
{
    return ib_to_ptime(ib()->t.request_started);
}

boost::posix_time::ptime ConstTransaction::request_headers() const
{
    return ib_to_ptime(ib()->t.request_headers);
}

boost::posix_time::ptime ConstTransaction::request_body() const
{
    return ib_to_ptime(ib()->t.request_body);
}

boost::posix_time::ptime ConstTransaction::request_finished() const
{
    return ib_to_ptime(ib()->t.request_finished);
}

boost::posix_time::ptime ConstTransaction::response_started() const
{
    return ib_to_ptime(ib()->t.response_started);
}

boost::posix_time::ptime ConstTransaction::response_headers() const
{
    return ib_to_ptime(ib()->t.response_headers);
}

boost::posix_time::ptime ConstTransaction::response_body() const
{
    return ib_to_ptime(ib()->t.response_body);
}

boost::posix_time::ptime ConstTransaction::response_finished() const
{
    return ib_to_ptime(ib()->t.response_finished);
}

boost::posix_time::ptime ConstTransaction::postprocess() const
{
    return ib_to_ptime(ib()->t.postprocess);
}

boost::posix_time::ptime ConstTransaction::logtime() const
{
    return ib_to_ptime(ib()->t.logtime);
}

boost::posix_time::ptime ConstTransaction::finished() const
{
    return ib_to_ptime(ib()->t.finished);
}

Transaction ConstTransaction::next() const
{
    return Transaction(ib()->next);
}

const char* ConstTransaction::hostname() const
{
    return ib()->hostname;
}

const char* ConstTransaction::effective_remote_ip_string() const
{
    return ib()->er_ipstr;
}

const char* ConstTransaction::path() const
{
    return ib()->path;
}

uint32_t ConstTransaction::flags() const
{
    return ib()->flags;
}

// Transaction

Transaction Transaction::remove_const(ConstTransaction transaction)
{
    return Transaction(const_cast<ib_type>(transaction.ib()));
}

Transaction::Transaction() :
    m_ib(NULL)
{
    // nop
}

Transaction::Transaction(ib_type ib_transaction) :
    ConstTransaction(ib_transaction),
    m_ib(ib_transaction)
{
    // nop
}

std::ostream& operator<<(std::ostream& o, const ConstTransaction& transaction)
{
    if (! transaction) {
        o << "IronBee::Transaction[!singular!]";
    } else {
        o << "IronBee::Transaction[" << transaction.id() << "]";
    }
    return o;
}

} // IronBee
