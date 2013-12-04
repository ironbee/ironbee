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
 * @brief IronBee++ Internals -- Engine Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/engine.hpp>
#include <ironbeepp/configuration_directives.hpp>
#include <ironbeepp/configuration_parser.hpp>
#include <ironbeepp/hooks.hpp>
#include <ironbeepp/context.hpp>
#include <ironbeepp/server.hpp>
#include <ironbeepp/notifier.hpp>
#include <ironbeepp/var.hpp>

#include <ironbee/context.h>
#include <ironbee/engine.h>

namespace IronBee {

const char* ConstEngine::state_event_name(state_event_e event)
{
    return ib_state_event_name(static_cast<ib_state_event_type_t>(event));
}

ConstEngine::ConstEngine() :
    m_ib(NULL)
{
    // nop
}

ConstEngine::ConstEngine(const ib_engine_t* ib_engine) :
    m_ib(ib_engine)
{
    // nop
}

Context ConstEngine::main_context() const
{
    return Context(ib_context_main(ib()));
}

ConstVarConfig ConstEngine::var_config() const
{
    return ConstVarConfig(ib_engine_var_config_get_const(ib()));
}

ConstServer ConstEngine::server() const
{
    return ConstServer(ib_engine_server_get(ib()));
}

Engine::Engine() :
    m_ib(NULL)
{
    // nop
}

Engine::Engine(ib_engine_t* ib_engine) :
    ConstEngine(ib_engine),
    m_ib(ib_engine)
{
    // nop
}

Engine Engine::create(Server server)
{
    ib_engine_t* ib_engine;
    throw_if_error(
        ib_engine_create(
            &ib_engine,
            server.ib()
        )
    );
    return Engine(ib_engine);
}

Engine Engine::remove_const(ConstEngine engine)
{
    // See API documentation for discussion of const_cast.
    return Engine(const_cast<ib_engine_t*>(engine.ib()));
}

void Engine::destroy()
{
    ib_engine_destroy(ib());
}

ConfigurationDirectivesRegistrar
     Engine::register_configuration_directives() const
{
    return ConfigurationDirectivesRegistrar(*this);
}

HooksRegistrar Engine::register_hooks() const
{
    return HooksRegistrar(*this);
}

Notifier Engine::notify() const
{
    return Notifier(*this);
}

MemoryPool Engine::main_memory_pool() const
{
    return MemoryPool(ib_engine_pool_main_get(ib()));
}

MemoryPool Engine::configuration_memory_pool() const
{
    return MemoryPool(ib_engine_pool_config_get(ib()));
}

MemoryPool Engine::temporary_memory_pool() const
{
    return MemoryPool(ib_engine_pool_temp_get(ib()));
}

VarConfig Engine::var_config() const
{
    return VarConfig(ib_engine_var_config_get(ib()));
}

void Engine::configuration_started(
    ConfigurationParser configuration_parser
) const
{
    throw_if_error(
        ib_engine_config_started(ib(), configuration_parser.ib())
    );
}

void Engine::configuration_finished() const
{
    throw_if_error(ib_engine_config_finished(ib()));
}

std::ostream& operator<<(std::ostream& o, const ConstEngine& engine)
{
    if (! engine) {
        o << "IronBee::Engine[!singular!]";
    }
    else {
        o << "IronBee::Engine[" << engine.ib() << "]";
    }

    return o;
}

} // IronBee
