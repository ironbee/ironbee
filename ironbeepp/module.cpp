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
#include <ironbee/debug.h>

#include <boost/make_shared.hpp>

#include <cassert>

namespace IronBee {

namespace Internal {

struct ModuleData
{
    ib_module_t* ib_module;
};

namespace  {

class ModuleHookPreChainModule
{
public:
    ModuleHookPreChainModule(
        Module::module_callback_t f,
        Module::module_callback_t next
    ) :
        m_f(f),
        m_next(next)
    {
        // nop
    }

    void operator()(Module m) const
    {
        m_f(m);
        m_next(m);
    }

private:
    Module::module_callback_t m_f;
    Module::module_callback_t m_next;
};

class ModuleHookPreChainContext
{
public:
    ModuleHookPreChainContext(
        Module::context_callback_t f,
        Module::context_callback_t next
    ) :
        m_f(f),
        m_next(next)
    {
        // nop
    }

    void operator()(Module m, Context c) const
    {
        m_f(m, c);
        m_next(m, c);
    }

private:
    Module::context_callback_t m_f;
    Module::context_callback_t m_next;
};

class ModuleHookChainModule
{
public:
    ModuleHookChainModule(
        Module::module_callback_t f,
        Module::module_callback_t prev
    ) :
        m_f(f),
        m_prev(prev)
    {
        // nop
    }

    void operator()(Module m) const
    {
        m_prev(m);
        m_f(m);
    }

private:
    Module::module_callback_t m_f;
    Module::module_callback_t m_prev;
};

class ModuleHookChainContext
{
public:
    ModuleHookChainContext(
        Module::context_callback_t f,
        Module::context_callback_t prev
    ) :
        m_f(f),
        m_prev(prev)
    {
        // nop
    }

    void operator()(Module m, Context c) const
    {
        m_prev(m, c);
        m_f(m, c);
    }

private:
    Module::context_callback_t m_f;
    Module::context_callback_t m_prev;
};

} // Anonymous

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

    assert(ib_engine = ib_module->ib);

    IB_FTRACE_RET_STATUS(IBPP_TRY_CATCH(ib_engine,
        Internal::data_to_value<Module::initialize_t>(cbdata)(
            Module(ib_module)
        )
    ));
}

ib_status_t finalize(
      ib_engine_t* ib_engine,
      ib_module_t* ib_module,
      void*        cbdata
)

{
    IB_FTRACE_INIT();

    assert(ib_engine = ib_module->ib);

    IB_FTRACE_RET_STATUS(IBPP_TRY_CATCH(ib_engine,
        Internal::data_to_value<Module::finalize_t>(cbdata)(
            Module(ib_module)
        )
    ));
}

ib_status_t context_open(
    ib_engine_t*  ib_engine,
    ib_module_t*  ib_module,
    ib_context_t* ib_context,
    void*         cbdata
)
{
    IB_FTRACE_INIT();

    assert(ib_engine = ib_module->ib);

    IB_FTRACE_RET_STATUS(IBPP_TRY_CATCH(ib_engine,
        Internal::data_to_value<Module::context_open_t>(cbdata)(
            Module(ib_module),
            Context(ib_context)
        )
    ));
}

ib_status_t context_close(
    ib_engine_t*  ib_engine,
    ib_module_t*  ib_module,
    ib_context_t* ib_context,
    void*         cbdata
)
{
    IB_FTRACE_INIT();

    assert(ib_engine = ib_module->ib);

    IB_FTRACE_RET_STATUS(IBPP_TRY_CATCH(ib_engine,
        Internal::data_to_value<Module::context_close_t>(cbdata)(
            Module(ib_module),
            Context(ib_context)
        )
    ));
}

ib_status_t context_destroy(
    ib_engine_t*  ib_engine,
    ib_module_t*  ib_module,
    ib_context_t* ib_context,
    void*         cbdata
)
{
    IB_FTRACE_INIT();

    assert(ib_engine = ib_module->ib);

    IB_FTRACE_RET_STATUS(IBPP_TRY_CATCH(ib_engine,
        Internal::data_to_value<Module::context_destroy_t>(cbdata)(
            Module(ib_module),
            Context(ib_context)
        )
    ));
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
    return Engine(m_data->ib_module->ib);
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

void Module::chain_initialize(initialize_t f)
{
    if (! m_data->ib_module->fn_init) {
        set_initialize(f);
    } else {
        set_initialize( Internal::ModuleHookChainModule(
            f,
            Internal::data_to_value<Module::initialize_t>(
                m_data->ib_module->cbdata_init
            )
        ) );
    }
}

void Module::prechain_initialize(initialize_t f)
{
    if (! m_data->ib_module->fn_init) {
        set_initialize(f);
    } else {
        set_initialize( Internal::ModuleHookPreChainModule(
            f,
            Internal::data_to_value<Module::initialize_t>(
                m_data->ib_module->cbdata_init
            )
        ) );
    }
}

void Module::set_initialize(initialize_t f)
{
    m_data->ib_module->cbdata_init = Internal::value_to_data(
        f,
        ib_engine_pool_main_get(m_data->ib_module->ib)
    );
    m_data->ib_module->fn_init = Hooks::initialize;
}

void Module::chain_finalize(finalize_t f)
{
    if (! m_data->ib_module->fn_fini) {
        set_finalize(f);
    } else {
        set_finalize( Internal::ModuleHookChainModule(
            f,
            Internal::data_to_value<Module::finalize_t>(
                m_data->ib_module->cbdata_fini
            )
        ) );
    }
}

void Module::prechain_finalize(finalize_t f)
{
    if (! m_data->ib_module->fn_fini) {
        set_finalize(f);
    } else {
        set_finalize( Internal::ModuleHookPreChainModule(
            f,
            Internal::data_to_value<Module::finalize_t>(
                m_data->ib_module->cbdata_fini
            )
        ) );
    }
}

void Module::set_finalize(finalize_t f)
{
    m_data->ib_module->cbdata_fini = Internal::value_to_data(
        f,
        ib_engine_pool_main_get(m_data->ib_module->ib)
    );
    m_data->ib_module->fn_fini = Hooks::finalize;
}

void Module::chain_context_open(context_open_t f)
{
    if (! m_data->ib_module->fn_ctx_open) {
        set_context_open(f);
    } else {
        set_context_open( Internal::ModuleHookChainContext(
            f,
            Internal::data_to_value<Module::context_open_t>(
                m_data->ib_module->cbdata_ctx_open
            )
        ) );
    }
}

void Module::prechain_context_open(context_open_t f)
{
    if (! m_data->ib_module->fn_ctx_open) {
        set_context_open(f);
    } else {
        set_context_open( Internal::ModuleHookPreChainContext(
            f,
            Internal::data_to_value<Module::context_open_t>(
                m_data->ib_module->cbdata_ctx_open
            )
        ) );
    }
}

void Module::set_context_open(context_open_t f)
{
    m_data->ib_module->cbdata_ctx_open = Internal::value_to_data(
        f,
        ib_engine_pool_main_get(m_data->ib_module->ib)
    );
    m_data->ib_module->fn_ctx_open = Hooks::context_open;
}

void Module::chain_context_close(context_close_t f)
{
    if (! m_data->ib_module->fn_ctx_close) {
        set_context_close(f);
    } else {
        set_context_close( Internal::ModuleHookChainContext(
            f,
            Internal::data_to_value<Module::context_close_t>(
                m_data->ib_module->cbdata_ctx_close
            )
        ) );
    }
}

void Module::prechain_context_close(context_close_t f)
{
    if (! m_data->ib_module->fn_ctx_close) {
        set_context_close(f);
    } else {
        set_context_close( Internal::ModuleHookPreChainContext(
            f,
            Internal::data_to_value<Module::context_close_t>(
                m_data->ib_module->cbdata_ctx_close
            )
        ) );
    }
}

void Module::set_context_close(context_close_t f)
{
    m_data->ib_module->cbdata_ctx_close = Internal::value_to_data(
        f,
        ib_engine_pool_main_get(m_data->ib_module->ib)
    );
    m_data->ib_module->fn_ctx_close = Hooks::context_close;
}

void Module::chain_context_destroy(context_destroy_t f)
{
    if (! m_data->ib_module->fn_ctx_destroy) {
        set_context_destroy(f);
    } else {
        set_context_destroy( Internal::ModuleHookChainContext(
            f,
            Internal::data_to_value<Module::context_destroy_t>(
                m_data->ib_module->cbdata_ctx_destroy
            )
        ) );
    }
}

void Module::prechain_context_destroy(context_destroy_t f)
{
    if (! m_data->ib_module->fn_ctx_destroy) {
        set_context_destroy(f);
    } else {
        set_context_destroy( Internal::ModuleHookPreChainContext(
            f,
            Internal::data_to_value<Module::context_destroy_t>(
                m_data->ib_module->cbdata_ctx_destroy
            )
        ) );
    }
}

void Module::set_context_destroy(context_destroy_t f)
{
    m_data->ib_module->cbdata_ctx_destroy = Internal::value_to_data(
        f,
        ib_engine_pool_main_get(m_data->ib_module->ib)
    );
    m_data->ib_module->fn_ctx_destroy = Hooks::context_destroy;
}

bool Module::operator==(const Module& other) const
{
    return (! *this && ! other) || (*this && other && ib() == other.ib());
}

bool Module::operator<(const Module& other) const
{
    if (! *this) {
        return other;
    }
    else if (! other) {
        return this;
    }
    else {
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

Module::Module(ib_module_t* ib_module) :
    m_data(boost::make_shared<Internal::ModuleData>())
{
    m_data->ib_module = ib_module;
}

Module::operator unspecified_bool_type() const
{
    return m_data ? unspecified_bool : 0;
}

std::ostream& operator<<(std::ostream& o, const Module& module)
{
    if (! module) {
        o << "IronBee::Module[!singular!]";
    }
    else {
        o << "IronBee::Module[" << module.name() << "]";
    }

    return o;
}

} // IronBee
