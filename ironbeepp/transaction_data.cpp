#include <ironbeepp/transaction_data.hpp>
#include <ironbeepp/transaction.hpp>
#include <ironbeepp/engine.hpp>
#include <ironbeepp/memory_pool.hpp>

namespace IronBee {

// ConstTransactionData

ConstTransactionData::ConstTransactionData() :
    m_ib(NULL)
{
    // nop
}

ConstTransactionData::ConstTransactionData(ib_type ib_transaction_data) :
    m_ib(ib_transaction_data)
{
    // nop
}

Engine ConstTransactionData::engine() const
{
    return Engine(ib()->ib);
}

MemoryPool ConstTransactionData::memory_pool() const
{
    return MemoryPool(ib()->mp);
}

Transaction ConstTransactionData::transaction() const
{
    return Transaction(ib()->tx);
}

ConstTransactionData::type_e ConstTransactionData::type() const
{
    return static_cast<type_e>(ib()->dtype);
}

size_t ConstTransactionData::allocated() const
{
    return ib()->dalloc;
}

size_t ConstTransactionData::length() const
{
    return ib()->dlen;
}

char* ConstTransactionData::data() const
{
    return reinterpret_cast<char*>(ib()->data);
}


// TransactionData

TransactionData TransactionData::remove_const(ConstTransactionData transaction_data)
{
    return TransactionData(const_cast<ib_type>(transaction_data.ib()));
}

TransactionData::TransactionData() :
    m_ib(NULL)
{
    // nop
}

TransactionData::TransactionData(ib_type ib_transaction_data) :
    ConstTransactionData(ib_transaction_data),
    m_ib(ib_transaction_data)
{
    // nop
}

std::ostream& operator<<(std::ostream& o, const ConstTransactionData& transaction_data)
{
    if (! transaction_data) {
        o << "IronBee::TransactionData[!singular!]";
    } else {
        o << "IronBee::TransactionData["
          << std::string(transaction_data.data(), transaction_data.length())
          << "]";
    }
    return o;
}

} // IronBee
