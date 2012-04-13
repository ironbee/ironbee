#include <ironbeepp/parsed_response_line.hpp>
#include <ironbeepp/transaction.hpp>
#include <ironbeepp/byte_string.hpp>

#include <ironbee/parsed_content.h>

namespace IronBee {

// ConstParsedResponseLine

ConstParsedResponseLine::ConstParsedResponseLine() :
    m_ib(NULL)
{
    // nop
}

ConstParsedResponseLine::ConstParsedResponseLine(
    ib_type ib_parsed_response_line
) :
    m_ib(ib_parsed_response_line)
{
    // nop
}

ByteString ConstParsedResponseLine::code() const
{
    return ByteString(ib()->code);
}

ByteString ConstParsedResponseLine::message() const
{
    return ByteString(ib()->msg);
}

// ParsedResponseLine

ParsedResponseLine ParsedResponseLine::remove_const(
    ConstParsedResponseLine parsed_response_line
)
{
    return ParsedResponseLine(const_cast<ib_type>(parsed_response_line.ib()));
}

ParsedResponseLine::ParsedResponseLine() :
    m_ib(NULL)
{
    // nop
}

ParsedResponseLine::ParsedResponseLine(ib_type ib_parsed_response_line) :
    ConstParsedResponseLine(ib_parsed_response_line),
    m_ib(ib_parsed_response_line)
{
    // nop
}

std::ostream& operator<<(std::ostream& o, const ConstParsedResponseLine& parsed_response_line)
{
    if (! parsed_response_line) {
        o << "IronBee::ParsedResponseLine[!singular!]";
    } else {
        o << "IronBee::ParsedResponseLine["
          << parsed_response_line.code().to_s() << " "
          << parsed_response_line.message().to_s() << "]";
    }
    return o;
}

} // IronBee
