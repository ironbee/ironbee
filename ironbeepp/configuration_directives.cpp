#include <ironbeepp/configuration_directives.hpp>
#include <ironbeepp/configuration_parser.hpp>
#include <ironbeepp/memory_pool.hpp>
#include <ironbeepp/context.hpp>
#include <ironbeepp/site.hpp>

#include <ironbeepp/internal/data.hpp>
#include <ironbeepp/internal/catch.hpp>

#include <ironbee/config.h>
#include <ironbee/debug.h>

#include <boost/foreach.hpp>

using namespace std;

namespace IronBee {

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

    try {
        Internal::data_to_value<
            ConfigurationDirectivesRegistrar::start_block_t
        >(cbdata)(
            ConfigurationParser(cfgparser),
            name,
            param
        );
    }
    catch (...) {
        IB_FTRACE_RET_STATUS(Internal::convert_exception(cfgparser->ib));
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t config_end_block(
    ib_cfgparser_t* cfgparser,
    const char*     name,
    void*           cbdata
)
{
    IB_FTRACE_INIT();

    try {
        Internal::data_to_value<
            ConfigurationDirectivesRegistrar::end_block_t
        >(cbdata)(
            ConfigurationParser(cfgparser),
            name
        );
    }
    catch (...) {
        IB_FTRACE_RET_STATUS(Internal::convert_exception(cfgparser->ib));
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t config_param1(
    ib_cfgparser_t* cfgparser,
    const char*     name,
    const char*     param,
    void*           cbdata
)
{
    IB_FTRACE_INIT();

    try {
        Internal::data_to_value<
            ConfigurationDirectivesRegistrar::param1_t
        >(cbdata)(
            ConfigurationParser(cfgparser),
            name,
            param
        );
    }
    catch (...) {
        IB_FTRACE_RET_STATUS(Internal::convert_exception(cfgparser->ib));
    }
    IB_FTRACE_RET_STATUS(IB_OK);
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

    try {
        Internal::data_to_value<
            ConfigurationDirectivesRegistrar::param2_t
        >(cbdata)(
            ConfigurationParser(cfgparser),
            name,
            param1,
            param2
        );
    }
    catch (...) {
        IB_FTRACE_RET_STATUS(Internal::convert_exception(cfgparser->ib));
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t config_list(
    ib_cfgparser_t* cfgparser,
    const char*     name,
    ib_list_t*      params,
    void*           cbdata
)
{
    IB_FTRACE_INIT();

    try {
        Internal::data_to_value<
            ConfigurationDirectivesRegistrar::list_t
        >(cbdata)(
            ConfigurationParser(cfgparser),
            name,
            List<const char*>(params)
        );
    }
    catch (...) {
        IB_FTRACE_RET_STATUS(Internal::convert_exception(cfgparser->ib));
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t config_on_off(
    ib_cfgparser_t* cfgparser,
    const char*     name,
    int             value,
    void*           cbdata
)
{
    IB_FTRACE_INIT();

    try {
        Internal::data_to_value<
            ConfigurationDirectivesRegistrar::on_off_t
        >(cbdata)(
            ConfigurationParser(cfgparser),
            name,
            value
        );
    }
    catch (...) {
        IB_FTRACE_RET_STATUS(Internal::convert_exception(cfgparser->ib));
    }
    IB_FTRACE_RET_STATUS(IB_OK);
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

    try {
        Internal::data_to_value<
            ConfigurationDirectivesRegistrar::op_flags_t
        >(cbdata)(
            ConfigurationParser(cfgparser),
            name,
            value,
            mask
        );
    }
    catch (...) {
        IB_FTRACE_RET_STATUS(Internal::convert_exception(cfgparser->ib));
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}

}
} // Hooks
} // Internal

ConfigurationDirectivesRegistrar::ConfigurationDirectivesRegistrar(
    Engine engine
) :
    m_engine(engine)
{
    // nop
}

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
            m_engine.main_memory_pool().ib()
        ),
        Internal::value_to_data(
            end_function,
            m_engine.main_memory_pool().ib()
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
            m_engine.main_memory_pool().ib()
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
            m_engine.main_memory_pool().ib()
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
            m_engine.main_memory_pool().ib()
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
            m_engine.main_memory_pool().ib()
        ),
        NULL,
        NULL
    ));

    return *this;
}

ConfigurationDirectivesRegistrar& ConfigurationDirectivesRegistrar::op_flags(
    const char*          name,
    op_flags_t           function,
    map<string, int64_t> value_map
)
{
    typedef map<string, int64_t>::value_type value_type;

    MemoryPool mp = m_engine.main_memory_pool();
    ib_strval_t* valmap = mp.allocate<ib_strval_t>(value_map.size()+1);

    int i = 0;
    BOOST_FOREACH(const value_type& v, value_map) {
        char* buf = mp.allocate<char>(v.first.size()+1);
        copy(v.first.begin(), v.first.end(), buf);
        buf[v.first.size()] = '\0';
        valmap[i].str = buf;
        valmap[i].val = v.second;
        ++i;
    }
    valmap[i].str = NULL;
    valmap[i].val = 0;

    Internal::throw_if_error(ib_config_register_directive(
        m_engine.ib(),
        name,
        IB_DIRTYPE_OPFLAGS,
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
