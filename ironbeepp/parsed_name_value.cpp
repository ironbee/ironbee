#include <ironbeepp/parsed_name_value.hpp>
#include <ironbeepp/byte_string.hpp>
#include <ironbeepp/transaction.hpp>

#include <ironbee/parsed_content.h>

namespace IronBee {

// ConstParsedNameValue

ConstParsedNameValue::ConstParsedNameValue() :
    m_ib(NULL)
{
    // nop
}

ConstParsedNameValue::ConstParsedNameValue(ib_type ib_parsed_name_value) :
    m_ib(ib_parsed_name_value)
{
    // nop
}

Transaction ConstParsedNameValue::transaction() const
{
    return Transaction(ib()->tx);
}

ByteString ConstParsedNameValue::name() const
{
    return ByteString(ib()->name);
}

ByteString ConstParsedNameValue::value() const
{
    return ByteString(ib()->value);
}

ParsedNameValue ConstParsedNameValue::next() const
{
    return ParsedNameValue(ib()->next);
}

// ParsedNameValue

ParsedNameValue ParsedNameValue::remove_const(
    ConstParsedNameValue parsed_name_value
)
{
    return ParsedNameValue(const_cast<ib_type>(parsed_name_value.ib()));
}

ParsedNameValue::ParsedNameValue() :
    m_ib(NULL)
{
    // nop
}

ParsedNameValue::ParsedNameValue(ib_type ib_parsed_name_value) :
    ConstParsedNameValue(ib_parsed_name_value),
    m_ib(ib_parsed_name_value)
{
    // nop
}

std::ostream& operator<<(
    std::ostream& o,
    const ConstParsedNameValue& parsed_name_value
)
{
    if (! parsed_name_value) {
        o << "IronBee::ParsedNameValue[!singular!]";
    } else {
        o << "IronBee::ParsedNameValue["
          << parsed_name_value.name().to_s() << ":"
          << parsed_name_value.value().to_s() << "]";
    }
    return o;
}

} // IronBee
