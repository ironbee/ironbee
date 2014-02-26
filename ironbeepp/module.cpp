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
 *
 * @sa module.hpp
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbeepp/module.hpp>
#include <ironbeepp/context.hpp>
#include <ironbeepp/engine.hpp>
#include <ironbeepp/catch.hpp>
#include <ironbeepp/data.hpp>

#include <ironbee/module.h>
#include <ironbee/engine.h>
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

namespace Hooks {

extern "C" {

ib_status_t ibpp_module_initialize(
    ib_engine_t* ib_engine,
    ib_module_t* ib_module,
    void*        cbdata
)
{
    assert(ib_engine == ib_module->ib);

    try {
        data_to_value<Module::initialize_t>(cbdata)(
            Module(ib_module)
        );
    }
    catch (...) {
        return convert_exception(ib_engine);
    }
    return IB_OK;
}

ib_status_t ibpp_module_finalize(
      ib_engine_t* ib_engine,
      ib_module_t* ib_module,
      void*        cbdata
)

{
    assert(ib_engine == ib_module->ib);

    try {
        data_to_value<Module::finalize_t>(cbdata)(
            Module(ib_module)
        );
    }
    catch (...) {
        return convert_exception(ib_engine);
    }
    return IB_OK;
}

ib_status_t ibpp_module_configuration_copy(
    ib_engine_t* ib_engine,
    ib_module_t* ib_module,
    void*        dst,
    const void*  src,
    size_t       length,
    void*        cbdata
)
{
    assert(ib_engine == ib_module->ib);
    assert(dst != NULL);
    assert(src != NULL);

    try {
        data_to_value<
            boost::function<
                void(ib_module_t*, void*, const void*, size_t)
            >
        >(cbdata)(
            ib_module,
            dst,
            src,
            length
        );
    }
    catch (...) {
        return convert_exception(ib_engine);
    }
    return IB_OK;
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

Module Module::with_name(Engine engine, const char* name)
{
    ib_module_t* m;

    throw_if_error(ib_engine_module_get(engine.ib(), name, &m));

    return Module(m);
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
            data_to_value<Module::initialize_t>(
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
            data_to_value<Module::initialize_t>(
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
        ib()->cbdata_init = value_to_data(
            f,
            engine().main_memory_mm().ib()
        );
        ib()->fn_init = Internal::Hooks::ibpp_module_initialize;
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
            data_to_value<Module::finalize_t>(
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
            data_to_value<Module::finalize_t>(
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
        ib()->cbdata_fini = value_to_data(
            f,
            engine().main_memory_mm().ib()
        );
        ib()->fn_fini = Internal::Hooks::ibpp_module_finalize;
    }
}

void Module::set_configuration_copier_translator(
    configuration_copier_translator_t f
) const
{
    ib()->cbdata_cfg_copy = value_to_data(
        f,
        engine().main_memory_mm().ib()
    );
    ib()->fn_cfg_copy = Internal::Hooks::ibpp_module_configuration_copy;
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
