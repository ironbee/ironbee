#include <ironbeepp/parsed_request_line.hpp>
#include <ironbeepp/transaction.hpp>
#include <ironbeepp/byte_string.hpp>

#include <ironbee/parsed_content.h>

namespace IronBee {

// ConstParsedRequestLine

ConstParsedRequestLine::ConstParsedRequestLine() :
    m_ib(NULL)
{
    // nop
}

ConstParsedRequestLine::ConstParsedRequestLine(
    ib_type ib_parsed_request_line
) :
    m_ib(ib_parsed_request_line)
{
    // nop
}

Transaction ConstParsedRequestLine::transaction() const
{
    return Transaction(ib()->tx);
}

ByteString ConstParsedRequestLine::method() const
{
    return ByteString(ib()->method);
}

ByteString ConstParsedRequestLine::path() const
{
    return ByteString(ib()->path);
}

ByteString ConstParsedRequestLine::version() const
{
    return ByteString(ib()->version);
}

// ParsedRequestLine

ParsedRequestLine ParsedRequestLine::remove_const(
    ConstParsedRequestLine parsed_request_line
)
{
    return ParsedRequestLine(const_cast<ib_type>(parsed_request_line.ib()));
}

ParsedRequestLine::ParsedRequestLine() :
    m_ib(NULL)
{
    // nop
}

ParsedRequestLine::ParsedRequestLine(ib_type ib_parsed_request_line) :
    ConstParsedRequestLine(ib_parsed_request_line),
    m_ib(ib_parsed_request_line)
{
    // nop
}

std::ostream& operator<<(std::ostream& o, const ConstParsedRequestLine& parsed_request_line)
{
    if (! parsed_request_line) {
        o << "IronBee::ParsedRequestLine[!singular!]";
    } else {
        o << "IronBee::ParsedRequestLine["
          << parsed_request_line.method().to_s() << " "
          << parsed_request_line.path().to_s() << " "
          << parsed_request_line.version().to_s() << "]";
    }
    return o;
}

} // IronBee
