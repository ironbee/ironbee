#include <ironbeepp/parsed_request_line.hpp>
#include <ironbeepp/transaction.hpp>
#include <ironbeepp/byte_string.hpp>
#include <ironbeepp/throw.hpp>

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

ByteString ConstParsedRequestLine::raw() const
{
    return ByteString(ib()->raw);
}

ByteString ConstParsedRequestLine::method() const
{
    return ByteString(ib()->method);
}

ByteString ConstParsedRequestLine::uri() const
{
    return ByteString(ib()->uri);
}

ByteString ConstParsedRequestLine::protocol() const
{
    return ByteString(ib()->protocol);
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

ParsedRequestLine ParsedRequestLine::create_alias(
    Transaction transaction,
    const char* raw,
    size_t raw_length,
    const char* method,
    size_t method_length,
    const char* uri,
    size_t uri_length,
    const char* protocol,
    size_t protocol_length
)
{
    ib_parsed_req_line_t *ib_prl;
    throw_if_error(
        ib_parsed_req_line_create(
            &ib_prl,
            transaction.ib(),
            raw,
            raw_length,
            method,
            method_length,
            uri,
            uri_length,
            protocol,
            protocol_length
        )
    );
    return ParsedRequestLine(ib_prl);
}

std::ostream& operator<<(std::ostream& o, const ConstParsedRequestLine& parsed_request_line)
{
    if (! parsed_request_line) {
        o << "IronBee::ParsedRequestLine[!singular!]";
    } else {
        o << "IronBee::ParsedRequestLine["
          << parsed_request_line.method().to_s() << " "
          << parsed_request_line.uri().to_s() << " "
          << parsed_request_line.protocol().to_s() << "]";
    }
    return o;
}

} // IronBee
