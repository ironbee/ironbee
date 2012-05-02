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

#include <ironbeepp/module.hpp>
#include <ironbeepp/context.hpp>
#include <ironbeepp/engine.hpp>
#include <ironbeepp/internal/catch.hpp>
#include <ironbeepp/internal/data.hpp>

#include <ironbee/module.h>
#include <ironbee/engine.h>
#include <ironbee/debug.h>

#include <cassert>

namespace IronBee {

namespace Internal {
namespace {

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

ib_status_t configuration_copy(
    ib_engine_t* ib_engine,
    ib_module_t* ib_module,
    void*        dst,
    const void*  src,
    size_t       length,
    void*        cbdata
)
{
    IB_FTRACE_INIT();

    assert(ib_engine = ib_module->ib);
    assert(dst != NULL);
    assert(src != NULL);

    IB_FTRACE_RET_STATUS(IBPP_TRY_CATCH(ib_engine,
        Internal::data_to_value<
            boost::function<
                void(ib_module_t*, void*, const void*, size_t)
            >
        >(cbdata)(
            ib_module,
            dst,
            src,
            length
        )
    ));
};

} // extern "C"

} // Hooks
} // Anonymous
} // Internal

/* ConstModule */

ConstModule::ConstModule() :
    m_ib(NULL)
{
    // nop
}

ConstModule::ConstModule(const ib_module_t* ib_module) :
    m_ib(ib_module)
{
    // nop
}

Engine ConstModule::engine() const
{
    return Engine(ib()->ib);
}

uint32_t ConstModule::version_number() const
{
    return ib()->vernum;
}

uint32_t ConstModule::abi_number() const
{
    return ib()->abinum;
}

const char* ConstModule::version() const
{
    return ib()->version;
}

const char* ConstModule::filename() const
{
    return ib()->filename;
}

size_t ConstModule::index() const
{
    return ib()->idx;
}

const char* ConstModule::name() const
{
    return ib()->name;
}

/* Module */

Module::Module() :
    m_ib(NULL)
{
    // nop
}

Module::Module(ib_module_t* ib_module) :
    ConstModule(ib_module),
    m_ib(ib_module)
{
    // nop
}

// See api documentation for discussion of remove_const
Module Module::remove_const(const ConstModule& const_module)
{
    return Module(const_cast<ib_module_t*>(const_module.ib()));
}

void Module::chain_initialize(initialize_t f) const
{
    if (! ib()->fn_init) {
        set_initialize(f);
    }
    else {
        set_initialize( Internal::ModuleHookChainModule(
            f,
            Internal::data_to_value<Module::initialize_t>(
                ib()->cbdata_init
            )
        ) );
    }
}

void Module::prechain_initialize(initialize_t f) const
{
    if (! ib()->fn_init) {
        set_initialize(f);
    }
    else {
        set_initialize( Internal::ModuleHookPreChainModule(
            f,
            Internal::data_to_value<Module::initialize_t>(
                ib()->cbdata_init
            )
        ) );
    }
}

void Module::set_initialize(initialize_t f) const
{
    if (f.empty()) {
        ib()->cbdata_init = NULL;
        ib()->fn_init     = NULL;
    }
    else {
        ib()->cbdata_init = Internal::value_to_data(
            f,
            engine().main_memory_pool().ib()
        );
        ib()->fn_init = Internal::Hooks::initialize;
    }
}

void Module::chain_finalize(finalize_t f) const
{
    if (! ib()->fn_fini) {
        set_finalize(f);
    }
    else {
        set_finalize( Internal::ModuleHookChainModule(
            f,
            Internal::data_to_value<Module::finalize_t>(
                ib()->cbdata_fini
            )
        ) );
    }
}

void Module::prechain_finalize(finalize_t f) const
{
    if (! ib()->fn_fini) {
        set_finalize(f);
    }
    else {
        set_finalize( Internal::ModuleHookPreChainModule(
            f,
            Internal::data_to_value<Module::finalize_t>(
                ib()->cbdata_fini
            )
        ) );
    }
}

void Module::set_finalize(finalize_t f) const
{
    if (f.empty()) {
        ib()->cbdata_fini = NULL;
        ib()->fn_fini     = NULL;
    }
    else {
        ib()->cbdata_fini = Internal::value_to_data(
            f,
            engine().main_memory_pool().ib()
        );
        ib()->fn_fini = Internal::Hooks::finalize;
    }
}

void Module::chain_context_open(context_open_t f) const
{
    if (! ib()->fn_ctx_open) {
        set_context_open(f);
    }
    else {
        set_context_open( Internal::ModuleHookChainContext(
            f,
            Internal::data_to_value<Module::context_open_t>(
                ib()->cbdata_ctx_open
            )
        ) );
    }
}

void Module::prechain_context_open(context_open_t f) const
{
    if (! ib()->fn_ctx_open) {
        set_context_open(f);
    }
    else {
        set_context_open( Internal::ModuleHookPreChainContext(
            f,
            Internal::data_to_value<Module::context_open_t>(
                ib()->cbdata_ctx_open
            )
        ) );
    }
}

void Module::set_context_open(context_open_t f) const
{
    if (f.empty()) {
        ib()->cbdata_ctx_open = NULL;
        ib()->fn_ctx_open     = NULL;
    }
    else {
        ib()->cbdata_ctx_open = Internal::value_to_data(
            f,
            engine().main_memory_pool().ib()
        );
        ib()->fn_ctx_open = Internal::Hooks::context_open;
    }
}

void Module::chain_context_close(context_close_t f) const
{
    if (! ib()->fn_ctx_close) {
        set_context_close(f);
    }
    else {
        set_context_close( Internal::ModuleHookChainContext(
            f,
            Internal::data_to_value<Module::context_close_t>(
                ib()->cbdata_ctx_close
            )
        ) );
    }
}

void Module::prechain_context_close(context_close_t f) const
{
    if (! ib()->fn_ctx_close) {
        set_context_close(f);
    }
    else {
        set_context_close( Internal::ModuleHookPreChainContext(
            f,
            Internal::data_to_value<Module::context_close_t>(
                ib()->cbdata_ctx_close
            )
        ) );
    }
}

void Module::set_context_close(context_close_t f) const
{
    if (f.empty()) {
        ib()->cbdata_ctx_close = NULL;
        ib()->fn_ctx_close     = NULL;
    }
    else {
        ib()->cbdata_ctx_close = Internal::value_to_data(
            f,
            engine().main_memory_pool().ib()
        );
        ib()->fn_ctx_close = Internal::Hooks::context_close;
    }
}

void Module::chain_context_destroy(context_destroy_t f) const
{
    if (! ib()->fn_ctx_destroy) {
        set_context_destroy(f);
    }
    else {
        set_context_destroy( Internal::ModuleHookChainContext(
            f,
            Internal::data_to_value<Module::context_destroy_t>(
                ib()->cbdata_ctx_destroy
            )
        ) );
    }
}

void Module::prechain_context_destroy(context_destroy_t f) const
{
    if (! ib()->fn_ctx_destroy) {
        set_context_destroy(f);
    }
    else {
        set_context_destroy( Internal::ModuleHookPreChainContext(
            f,
            Internal::data_to_value<Module::context_destroy_t>(
                ib()->cbdata_ctx_destroy
            )
        ) );
    }
}

void Module::set_context_destroy(context_destroy_t f) const
{
    if (f.empty()) {
        ib()->cbdata_ctx_destroy = NULL;
        ib()->fn_ctx_destroy     = NULL;
    }
    else {
        ib()->cbdata_ctx_destroy = Internal::value_to_data(
            f,
            engine().main_memory_pool().ib()
        );
        ib()->fn_ctx_destroy = Internal::Hooks::context_destroy;
    }
}

void Module::set_configuration_copier_translator(
    configuration_copier_translator_t f
) const
{
    ib()->cbdata_cfg_copy = Internal::value_to_data(
        f,
        engine().main_memory_pool().ib()
    );
    ib()->fn_cfg_copy = Internal::Hooks::configuration_copy;
}

std::ostream& operator<<(std::ostream& o, const ConstModule& module)
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
