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
#include <ironbeepp/c_trampoline.hpp>
#include <ironbeepp/catch.hpp>
#include <ironbeepp/configuration_directives.hpp>
#include <ironbeepp/configuration_parser.hpp>
#include <ironbeepp/hooks.hpp>
#include <ironbeepp/context.hpp>
#include <ironbeepp/server.hpp>
#include <ironbeepp/notifier.hpp>
#include <ironbeepp/var.hpp>

#include <ironbee/context.h>
#include <ironbee/engine.h>
#include <ironbee/rule_engine.h>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdelete-non-virtual-dtor"
#if __has_warning("-Wunused-local-typedef")
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
#endif
#include <boost/bind.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

namespace IronBee {

const char* ConstEngine::state_name(state_e state)
{
    return ib_state_name(static_cast<ib_state_t>(state));
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

MemoryManager Engine::main_memory_mm() const
{
    return MemoryManager(ib_engine_mm_main_get(ib()));
}

MemoryManager Engine::configuration_memory_mm() const
{
    return MemoryManager(ib_engine_mm_config_get(ib()));
}

MemoryManager Engine::temporary_memory_mm() const
{
    return MemoryManager(ib_engine_mm_temp_get(ib()));
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


namespace {

ib_status_t rule_ownership_translator(
    Engine::rule_ownership_t fn,
    const ib_engine_t*       engine,
    const ib_rule_t*         rule,
    const ib_context_t*      ctx
)
{
    try {
        fn(ConstEngine(engine), rule, ConstContext(ctx));
    }
    catch (...) {
        return convert_exception(engine);
    }
    return IB_OK;
}

ib_status_t rule_injection_translator(
    Engine::rule_injection_t fn,
    const ib_engine_t*       engine,
    const ib_rule_exec_t*    rule_exec,
    ib_list_t*               rule_list
)
{
    try {
        fn(ConstEngine(engine), rule_exec, List<const ib_rule_t*>(rule_list));
    }
    catch (...) {
        return convert_exception(engine);
    }
    return IB_OK;
}

}

void Engine::register_rule_ownership(
    const char*      name,
    rule_ownership_t ownership
) const
{
    std::pair<ib_rule_ownership_fn_t, void*> trampoline =
        make_c_trampoline<
            ib_status_t(
                const ib_engine_t*,
                const ib_rule_t*,
                const ib_context_t*
            )
        >(boost::bind(rule_ownership_translator, ownership, _1, _2, _3));

    throw_if_error(
        ib_rule_register_ownership_fn(
            ib(),
            name,
            trampoline.first, trampoline.second
        )
    );

    main_memory_mm().register_cleanup(
        boost::bind(delete_c_trampoline, trampoline.second)
    );
}

void Engine::register_rule_injection(
    const char*         name,
    ib_rule_phase_num_t phase,
    rule_injection_t    injection
) const
{
    std::pair<ib_rule_injection_fn_t, void*> trampoline =
        make_c_trampoline<
            ib_status_t(
                const ib_engine_t*,
                const ib_rule_exec_t*,
                ib_list_t*
            )
        >(boost::bind(rule_injection_translator, injection, _1, _2, _3));

    throw_if_error(
        ib_rule_register_injection_fn(
            ib(),
            name,
            phase,
            trampoline.first, trampoline.second
        )
    );

    main_memory_mm().register_cleanup(
        boost::bind(delete_c_trampoline, trampoline.second)
    );
}

namespace {

ib_status_t block_handler_translator(
    Engine::block_handler_t handler,
    ib_tx_t*                tx,
    ib_block_info_t*        info
)
{
    try {
        handler(Transaction(tx), *info);
    }
    catch (...) {
        return convert_exception();
    }
    return IB_OK;
}

ib_status_t block_pre_hook_translator(
    Engine::block_pre_hook_t hook,
    ib_tx_t*                 tx
)
{
    try {
        hook(Transaction(tx));
    }
    catch (...) {
        return convert_exception();
    }
    return IB_OK;
}

ib_status_t block_post_hook_translator(
    Engine::block_post_hook_t hook,
    ib_tx_t*                  tx,
     const ib_block_info_t*   info
)
{
    try {
        hook(Transaction(tx), *info);
    }
    catch (...) {
        return convert_exception();
    }
    return IB_OK;
}

}

void Engine::register_block_handler(
    const char*     name,
    block_handler_t handler
) const
{
    std::pair<ib_block_handler_fn_t, void*> trampoline =
        make_c_trampoline<
            ib_status_t(
                ib_tx_t*,
                ib_block_info_t*
            )
        >(boost::bind(block_handler_translator, handler, _1, _2));

    throw_if_error(
        ib_register_block_handler(
            ib(),
            name,
            trampoline.first, trampoline.second
        )
    );

    main_memory_mm().register_cleanup(
        boost::bind(delete_c_trampoline, trampoline.second)
    );
}

void Engine::register_block_pre_hook(
    const char*      name,
    block_pre_hook_t hook
) const
{
    std::pair<ib_block_pre_hook_fn_t, void*> trampoline =
        make_c_trampoline<
            ib_status_t(
                ib_tx_t*
            )
        >(boost::bind(block_pre_hook_translator, hook, _1));

    throw_if_error(
        ib_register_block_pre_hook(
            ib(),
            name,
            trampoline.first, trampoline.second
        )
    );

    main_memory_mm().register_cleanup(
        boost::bind(delete_c_trampoline, trampoline.second)
    );
}

void Engine::register_block_post_hook(
    const char*       name,
    block_post_hook_t hook
) const
{
    std::pair<ib_block_post_hook_fn_t, void*> trampoline =
        make_c_trampoline<
            ib_status_t(
                ib_tx_t*,
                const ib_block_info_t*
            )
        >(boost::bind(block_post_hook_translator, hook, _1, _2));

    throw_if_error(
        ib_register_block_post_hook(
            ib(),
            name,
            trampoline.first, trampoline.second
        )
    );

    main_memory_mm().register_cleanup(
        boost::bind(delete_c_trampoline, trampoline.second)
    );
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
