/*****************************************************************************
 * Licensed to Qualys, Inc. (QUALYS) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * QUALYS licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ****************************************************************************/

/**
 * @file
 * @brief IronBee++ Module Implementation
 * @internal
 *
 * @sa module.hpp
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#define IBPP_EXPOSE_C
#include <ironbeepp/module.hpp>
#include <ironbeepp/context.hpp>
#include <ironbeepp/engine.hpp>
#include <ironbeepp/internal/catch.hpp>
#include "data.hpp"

#include <ironbee/module.h>
#include <ironbee/engine.h>

#include <boost/make_shared.hpp>

#include <cassert>

namespace IronBee {

namespace Internal {

struct ModuleData
{
    ib_module_t* ib_module;
};

} // Internal

namespace {
namespace Hooks {

extern "C" {

ib_status_t initialize(
    ib_engine_t* ib_engine,
    ib_module_t* ib_module,
    void*        cbdata
)
{
    IB_FTRACE_INIT();

    assert( ib_engine = ib_module->ib );

    IB_FTRACE_RET_STATUS( IBPP_TRY_CATCH( ib_engine,
        Internal::data_to_value<Module::initialize_t>( cbdata )(
            Module( ib_module )
        )
    ) );
}

ib_status_t finalize(
      ib_engine_t* ib_engine,
      ib_module_t* ib_module,
      void*        cbdata
)

{
    IB_FTRACE_INIT();

    assert( ib_engine = ib_module->ib );

    IB_FTRACE_RET_STATUS( IBPP_TRY_CATCH( ib_engine,
        Internal::data_to_value<Module::finalize_t>( cbdata )(
            Module( ib_module )
        )
    ) );
}

ib_status_t context_open(
    ib_engine_t*  ib_engine,
    ib_module_t*  ib_module,
    ib_context_t* ib_context,
    void*         cbdata
)
{
    IB_FTRACE_INIT();

    assert( ib_engine = ib_module->ib );

    IB_FTRACE_RET_STATUS( IBPP_TRY_CATCH( ib_engine,
        Internal::data_to_value<Module::context_open_t>( cbdata )(
            Module(  ib_module  ),
            Context( ib_context )
        )
    ) );
}

ib_status_t context_close(
    ib_engine_t*  ib_engine,
    ib_module_t*  ib_module,
    ib_context_t* ib_context,
    void*         cbdata
)
{
    IB_FTRACE_INIT();

    assert( ib_engine = ib_module->ib );

    IB_FTRACE_RET_STATUS( IBPP_TRY_CATCH( ib_engine,
        Internal::data_to_value<Module::context_close_t>( cbdata )(
            Module(  ib_module  ),
            Context( ib_context )
        )
    ) );
}

ib_status_t context_destroy(
    ib_engine_t*  ib_engine,
    ib_module_t*  ib_module,
    ib_context_t* ib_context,
    void*         cbdata
)
{
    IB_FTRACE_INIT();

    assert( ib_engine = ib_module->ib );

    IB_FTRACE_RET_STATUS( IBPP_TRY_CATCH( ib_engine,
        Internal::data_to_value<Module::context_destroy_t>( cbdata )(
            Module(  ib_module  ),
            Context( ib_context )
        )
    ) );
}

} // extern "C"

} // Hooks
} // Anonymous

Module::Module()
{
    // nop
}

Engine Module::engine() const
{
    return Engine( m_data->ib_module->ib );
}

uint32_t Module::version_number() const
{
    return m_data->ib_module->vernum;
}

uint32_t Module::abi_number() const
{
    return m_data->ib_module->abinum;
}

const char* Module::version() const
{
    return m_data->ib_module->version;
}

const char* Module::filename() const
{
    return m_data->ib_module->filename;
}

size_t Module::index() const
{
    return m_data->ib_module->idx;
}

const char* Module::name() const
{
    return m_data->ib_module->name;
}

void Module::set_initialize( initialize_t f )
{
    m_data->ib_module->cbdata_init = Internal::value_to_data(
        f,
        ib_engine_pool_main_get( m_data->ib_module->ib )
    );
    m_data->ib_module->fn_init = Hooks::initialize;
}

void Module::set_finalize( finalize_t f )
{
    m_data->ib_module->cbdata_fini = Internal::value_to_data(
        f,
        ib_engine_pool_main_get( m_data->ib_module->ib )
    );
    m_data->ib_module->fn_fini = Hooks::finalize;
}

void Module::set_context_open( context_open_t f )
{
    m_data->ib_module->cbdata_ctx_open = Internal::value_to_data(
        f,
        ib_engine_pool_main_get( m_data->ib_module->ib )
    );
    m_data->ib_module->fn_ctx_open = Hooks::context_open;
}

void Module::set_context_close( context_close_t f )
{
    m_data->ib_module->cbdata_ctx_close = Internal::value_to_data(
        f,
        ib_engine_pool_main_get( m_data->ib_module->ib )
    );
    m_data->ib_module->fn_ctx_close = Hooks::context_close;
}

void Module::set_context_destroy( context_destroy_t f )
{
    m_data->ib_module->cbdata_ctx_destroy = Internal::value_to_data(
        f,
        ib_engine_pool_main_get( m_data->ib_module->ib )
    );
    m_data->ib_module->fn_ctx_destroy = Hooks::context_destroy;
}

bool Module::operator==( const Module& other ) const
{
    return ( ! *this && ! other ) || ( ib() == other.ib() );
}

bool Module::operator<( const Module& other ) const
{
    if ( ! *this ) {
        return ! other;
    } else {
        return ib() < other.ib();
    }
}

ib_module_t* Module::ib()
{
    return m_data->ib_module;
}

const ib_module_t* Module::ib() const
{
    return m_data->ib_module;
}

Module::Module( ib_module_t* ib_module ) :
    m_data( boost::make_shared<Internal::ModuleData>() )
{
    m_data->ib_module = ib_module;
}

Module::operator unspecified_bool_type() const
{
    return m_data ? unspecified_bool : 0;
}

std::ostream& operator<<( std::ostream& o, const Module& module )
{
    o << "IronBee::Module[" << module.name() << "]";

    return o;
}

} // IronBee
