#include <ironbeepp/configuration_directives.hpp>
#include <ironbeepp/memory_pool.hpp>
#include <ironbeepp/context.hpp>
#include <ironbeepp/site.hpp>

#include <ironbeepp/internal/data.hpp>
#include <ironbeepp/internal/catch.hpp>

#include <ironbee/config.h>
#include <ironbee/debug.h>

#include <boost/foreach.hpp>

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
    m_ib(NULL)
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

std::ostream& operator<<(
    std::ostream& o,
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

// ConfigurationDirectivesRegistrar
namespace Internal {
namespace Hooks {
namespace {

ib_status_t config_start_block(
    ib_cfgparser_t* cfgparser,
    const char*     name,
    const char*     param,
    void*           cbdata
)
{
    IB_FTRACE_INIT();

    IB_FTRACE_RET_STATUS(IBPP_TRY_CATCH(cfgparser->ib,
        Internal::data_to_value<
            ConfigurationDirectivesRegistrar::start_block_t
        >(cbdata)(
            ConfigurationParser(cfgparser),
            name,
            param
        )
    ));
}

ib_status_t config_end_block(
    ib_cfgparser_t* cfgparser,
    const char*     name,
    void*           cbdata
)
{
    IB_FTRACE_INIT();

    IB_FTRACE_RET_STATUS(IBPP_TRY_CATCH(cfgparser->ib,
        Internal::data_to_value<
            ConfigurationDirectivesRegistrar::end_block_t
        >(cbdata)(
            ConfigurationParser(cfgparser),
            name
        )
    ));
}

ib_status_t config_param1(
    ib_cfgparser_t* cfgparser,
    const char*     name,
    const char*     param,
    void*           cbdata
)
{
    IB_FTRACE_INIT();

    IB_FTRACE_RET_STATUS(IBPP_TRY_CATCH(cfgparser->ib,
        Internal::data_to_value<
            ConfigurationDirectivesRegistrar::param1_t
        >(cbdata)(
            ConfigurationParser(cfgparser),
            name,
            param
        )
    ));
}

ib_status_t config_param2(
    ib_cfgparser_t* cfgparser,
    const char*     name,
    const char*     param1,
    const char*     param2,
    void*           cbdata
)
{
    IB_FTRACE_INIT();

    IB_FTRACE_RET_STATUS(IBPP_TRY_CATCH(cfgparser->ib,
        Internal::data_to_value<
            ConfigurationDirectivesRegistrar::param2_t
        >(cbdata)(
            ConfigurationParser(cfgparser),
            name,
            param1,
            param2
        )
    ));
}

ib_status_t config_list(
    ib_cfgparser_t* cfgparser,
    const char*     name,
    ib_list_t*      params,
    void*           cbdata
)
{
    IB_FTRACE_INIT();

    IB_FTRACE_RET_STATUS(IBPP_TRY_CATCH(cfgparser->ib,
        Internal::data_to_value<
            ConfigurationDirectivesRegistrar::list_t
        >(cbdata)(
            ConfigurationParser(cfgparser),
            name,
            List<const char*>(params)
        )
    ));
}

ib_status_t config_on_off(
    ib_cfgparser_t* cfgparser,
    const char*     name,
    int             value,
    void*           cbdata
)
{
    IB_FTRACE_INIT();

    IB_FTRACE_RET_STATUS(IBPP_TRY_CATCH(cfgparser->ib,
        Internal::data_to_value<
            ConfigurationDirectivesRegistrar::on_off_t
        >(cbdata)(
            ConfigurationParser(cfgparser),
            name,
            value
        )
    ));
}

ib_status_t config_op_flags(
    ib_cfgparser_t* cfgparser,
    const char*     name,
    uint32_t        value,
    uint32_t        mask,
    void*           cbdata
)
{
    IB_FTRACE_INIT();

    IB_FTRACE_RET_STATUS(IBPP_TRY_CATCH(cfgparser->ib,
        Internal::data_to_value<
            ConfigurationDirectivesRegistrar::op_flags_t
        >(cbdata)(
            ConfigurationParser(cfgparser),
            name,
            value,
            mask
        )
    ));
}

}
} // Hooks
} // Internal

ConfigurationDirectivesRegistrar& ConfigurationDirectivesRegistrar::block(
    const char*   name,
    start_block_t start_function,
    end_block_t   end_function
)
{
    Internal::throw_if_error(ib_config_register_directive(
        m_engine.ib(),
        name,
        IB_DIRTYPE_SBLK1,
        reinterpret_cast<ib_void_fn_t>(&Internal::Hooks::config_start_block),
        &Internal::Hooks::config_end_block,
        Internal::value_to_data(
            start_function,
            ib_engine_pool_main_get(m_engine.ib())
        ),
        Internal::value_to_data(
            end_function,
            ib_engine_pool_main_get(m_engine.ib())
        ),
        NULL
    ));

    return *this;
}

ConfigurationDirectivesRegistrar& ConfigurationDirectivesRegistrar::on_off(
    const char* name,
    on_off_t    function
)
{
    Internal::throw_if_error(ib_config_register_directive(
        m_engine.ib(),
        name,
        IB_DIRTYPE_ONOFF,
        reinterpret_cast<ib_void_fn_t>(&Internal::Hooks::config_on_off),
        NULL,
        Internal::value_to_data(
            function,
            ib_engine_pool_main_get(m_engine.ib())
        ),
        NULL,
        NULL
    ));

    return *this;
}

ConfigurationDirectivesRegistrar& ConfigurationDirectivesRegistrar::param1(
    const char* name,
    param1_t    function
)
{
    Internal::throw_if_error(ib_config_register_directive(
        m_engine.ib(),
        name,
        IB_DIRTYPE_PARAM1,
        reinterpret_cast<ib_void_fn_t>(&Internal::Hooks::config_param1),
        NULL,
        Internal::value_to_data(
            function,
            ib_engine_pool_main_get(m_engine.ib())
        ),
        NULL,
        NULL
    ));

    return *this;
}

ConfigurationDirectivesRegistrar& ConfigurationDirectivesRegistrar::param2(
    const char* name,
    param2_t    function
)
{
    Internal::throw_if_error(ib_config_register_directive(
        m_engine.ib(),
        name,
        IB_DIRTYPE_PARAM2,
        reinterpret_cast<ib_void_fn_t>(&Internal::Hooks::config_param2),
        NULL,
        Internal::value_to_data(
            function,
            ib_engine_pool_main_get(m_engine.ib())
        ),
        NULL,
        NULL
    ));

    return *this;
}

ConfigurationDirectivesRegistrar& ConfigurationDirectivesRegistrar::list(
    const char* name,
    list_t      function
)
{
    Internal::throw_if_error(ib_config_register_directive(
        m_engine.ib(),
        name,
        IB_DIRTYPE_LIST,
        reinterpret_cast<ib_void_fn_t>(&Internal::Hooks::config_list),
        NULL,
        Internal::value_to_data(
            function,
            ib_engine_pool_main_get(m_engine.ib())
        ),
        NULL,
        NULL
    ));

    return *this;
}

ConfigurationDirectivesRegistrar& ConfigurationDirectivesRegistrar::op_flags(
    const char*                    name,
    op_flags_t                     function,
    std::map<std::string, int64_t> value_map
)
{
    typedef List<ib_strval_t*> list_t;
    typedef std::map<std::string, int64_t>::value_type value_type;

    MemoryPool mp(ib_engine_pool_main_get(m_engine.ib()));
    ib_strval_t* valmap = mp.allocate<ib_strval_t>(value_map.size());

    int i = 0;
    BOOST_FOREACH(const value_type& v, value_map) {
        char* buf = mp.allocate<char>(v.first.size());
        std::copy(v.first.begin(), v.first.end(), buf);
        valmap[i].str = buf;
        valmap[i].val = v.second;
    }

    Internal::throw_if_error(ib_config_register_directive(
        m_engine.ib(),
        name,
        IB_DIRTYPE_LIST,
        reinterpret_cast<ib_void_fn_t>(&Internal::Hooks::config_op_flags),
        NULL,
        Internal::value_to_data(
            function,
            mp.ib()
        ),
        NULL,
        valmap
    ));

    return *this;
}

} // IronBee