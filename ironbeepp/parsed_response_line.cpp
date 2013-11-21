#include <ironbeepp/parsed_response_line.hpp>
#include <ironbeepp/transaction.hpp>
#include <ironbeepp/byte_string.hpp>
#include <ironbeepp/throw.hpp>

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

ByteString ConstParsedResponseLine::raw() const
{
    return ByteString(ib()->raw);
}

ByteString ConstParsedResponseLine::protocol() const
{
    return ByteString(ib()->protocol);
}

ByteString ConstParsedResponseLine::status() const
{
    return ByteString(ib()->status);
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

ParsedResponseLine ParsedResponseLine::create_alias(
    Transaction transaction,
    const char* raw,
    size_t raw_length,
    const char* protocol,
    size_t protocol_length,
    const char* status,
    size_t status_length,
    const char* message,
    size_t message_length
)
{
    ib_parsed_resp_line_t *ib_prl;
    throw_if_error(
        ib_parsed_resp_line_create(
            &ib_prl,
            transaction.ib(),
            raw,
            raw_length,
            protocol,
            protocol_length,
            status,
            status_length,
            message,
            message_length
        )
    );
    return ParsedResponseLine(ib_prl);
}

std::ostream& operator<<(std::ostream& o, const ConstParsedResponseLine& parsed_response_line)
{
    if (! parsed_response_line) {
        o << "IronBee::ParsedResponseLine[!singular!]";
    } else {
        o << "IronBee::ParsedResponseLine["
          << parsed_response_line.status().to_s() << " "
          << parsed_response_line.message().to_s() << "]";
    }
    return o;
}

} // IronBee
