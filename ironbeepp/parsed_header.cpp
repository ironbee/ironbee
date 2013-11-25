#include <ironbeepp/parsed_header.hpp>
#include <ironbeepp/byte_string.hpp>

#include <ironbee/parsed_content.h>

namespace IronBee {

// ConstParsedHeader

ConstParsedHeader::ConstParsedHeader() :
    m_ib(NULL)
{
    // nop
}

ConstParsedHeader::ConstParsedHeader(ib_type ib_parsed_header) :
    m_ib(ib_parsed_header)
{
    // nop
}

ByteString ConstParsedHeader::name() const
{
    return ByteString(ib()->name);
}

ByteString ConstParsedHeader::value() const
{
    return ByteString(ib()->value);
}

ParsedHeader ConstParsedHeader::next() const
{
    return ParsedHeader(ib()->next);
}

// ParsedHeader

ParsedHeader ParsedHeader::remove_const(
    ConstParsedHeader parsed_header
)
{
    return ParsedHeader(const_cast<ib_type>(parsed_header.ib()));
}

ParsedHeader::ParsedHeader() :
    m_ib(NULL)
{
    // nop
}

ParsedHeader::ParsedHeader(ib_type ib_parsed_header) :
    ConstParsedHeader(ib_parsed_header),
    m_ib(ib_parsed_header)
{
    // nop
}

ParsedHeader ParsedHeader::create(
    MemoryPool pool,
    ByteString name,
    ByteString value
)
{
    ib_parsed_header_t* ib_pnv
        = pool.allocate<ib_parsed_header_t>();
    ib_pnv->name = name.ib();
    ib_pnv->value = value.ib();
    ib_pnv->next = NULL;

    return ParsedHeader(ib_pnv);
}

std::ostream& operator<<(
    std::ostream& o,
    const ConstParsedHeader& parsed_header
)
{
    if (! parsed_header) {
        o << "IronBee::ParsedHeader[!singular!]";
    } else {
        o << "IronBee::ParsedHeader["
          << parsed_header.name().to_s() << ":"
          << parsed_header.value().to_s() << "]";
    }
    return o;
}

} // IronBee
