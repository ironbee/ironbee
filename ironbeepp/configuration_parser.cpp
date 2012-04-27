#include <ironbeepp/configuration_parser.hpp>
#include <ironbeepp/engine.hpp>
#include <ironbeepp/site.hpp>
#include <ironbeepp/context.hpp>

#include <ironbee/config.h>

using namespace std;

namespace IronBee {

// ConstConfigurationParser
ConstConfigurationParser::ConstConfigurationParser() :
    m_ib(NULL)
{
    // nop
}

ConstConfigurationParser::ConstConfigurationParser(
    ib_type ib_configuration_parser
) :
    m_ib(ib_configuration_parser)
{
    // nop
}

Engine ConstConfigurationParser::engine() const
{
    return Engine(ib()->ib);
}

MemoryPool ConstConfigurationParser::memory_pool() const
{
    return MemoryPool(ib()->mp);
}

Context ConstConfigurationParser::current_context() const
{
    return Context(ib()->cur_ctx);
}

Site ConstConfigurationParser::current_site() const
{
    return Site(ib()->cur_site);
}

Location ConstConfigurationParser::current_location() const
{
    return Location(ib()->cur_loc);
}

const char* ConstConfigurationParser::current_block_name() const
{
    return ib()->cur_blkname;
}

// ConfigurationParser

ConfigurationParser ConfigurationParser::remove_const(
     ConstConfigurationParser configuration_parser
)
{
    return ConfigurationParser(
        const_cast<ib_type>(configuration_parser.ib())
    );
}

ConfigurationParser::ConfigurationParser() :
    m_ib(NULL)
{
    // nop
}

ConfigurationParser::ConfigurationParser(ib_type ib_configuration_parser) :
    ConstConfigurationParser(ib_configuration_parser),
    m_ib(ib_configuration_parser)
{
    // nop
}

void ConfigurationParser::parse_file(const string& path) const
{
    Internal::throw_if_error(
        ib_cfgparser_parse(ib(), path.c_str())
    );
}
void ConfigurationParser::parse_buffer(
    const char* buffer,
    size_t      length,
    const char* file,
    unsigned    lineno,
    bool        more
) const
{
    Internal::throw_if_error(
        ib_cfgparser_parse_buffer(
            ib(),
            buffer, length,
            file, lineno,
            (more ? IB_TRUE : IB_FALSE)
        )
    );
}

void ConfigurationParser::parse_buffer(const std::string& s,
                                       const std::string& file,
                                       unsigned lineno,
                                       bool more) const
{
    parse_buffer(s.data(), s.length(), file.c_str(), lineno, more);
}

ostream& operator<<(
    ostream& o,
    const ConstConfigurationParser& configuration_parser
)
{
    if (! configuration_parser) {
        o << "IronBee::ConfigurationParser[!singular!]";
    }
    else {
        o << "IronBee::ConfigurationParser["
          << configuration_parser.current_location().path() << ":"
          << configuration_parser.current_block_name()
          << "]";
    }
    return o;
}

} // IronBee
