
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
 * @brief IronBee++ --- Action Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbeepp/action.hpp>

#include <ironbee/action.h>

namespace IronBee {

// ConstAction

ConstAction::ConstAction() :
    m_ib(NULL)
{
    // nop
}

ConstAction::ConstAction(ib_type ib_action) :
    m_ib(ib_action)
{
    // nop
}

ConstAction ConstAction::lookup(
    Engine      engine,
    const char* name,
    size_t      name_length
)
{
    const ib_action_t* action;

    throw_if_error(
        ib_action_lookup(
            engine.ib(),
            name, name_length,
            &action
        )
    );

    return ConstAction(action);
}

ConstAction ConstAction::lookup(
    Engine             engine,
    const std::string& name
)
{
    return lookup(engine, name.data(), name.length());
}

const char* ConstAction::name() const
{
    return ib_action_name(ib());
}

void ConstAction::register_with(Engine engine)
{
    throw_if_error(ib_action_register(engine.ib(), ib()));
}

// Action

Action Action::remove_const(ConstAction action)
{
    return Action(const_cast<ib_type>(action.ib()));
}

Action::Action() :
    m_ib(NULL)
{
    // nop
}

Action::Action(ib_type ib_action) :
    ConstAction(ib_action),
    m_ib(ib_action)
{
    // nop
}

namespace Impl {

void action_cleanup(action_create_data_t data)
{
    if (data.create_trampoline.second) {
        delete_c_trampoline(data.create_trampoline.second);
    }
    if (data.execute_trampoline.second) {
        delete_c_trampoline(data.execute_trampoline.second);
    }
    if (data.destroy_trampoline.second) {
        delete_c_trampoline(data.destroy_trampoline.second);
    }
}

} // Impl

namespace {

class action_create
{
public:
    action_create(Action::action_generator_t generator) :
        m_generator(generator)
    {
        // nop
    }

    void* operator()(
        MemoryManager memory_manager,
        Context context,
        const char* name
    ) const
    {
        return value_to_data(m_generator(memory_manager, context, name));
    }

private:
    Action::action_generator_t m_generator;
};

void action_execute(
    const ib_rule_exec_t* rule_exec,
    void*                 instance_data
)
{
    data_to_value<Action::action_instance_t>(instance_data)(rule_exec);
}

void action_destroy(
    void* instance_data
)
{
    delete reinterpret_cast<boost::any*>(instance_data);
}

}

Action Action::create(
    MemoryManager      memory_manager,
    const char*        name,
    action_generator_t generator
)
{
    return create<void>(
        memory_manager,
        name,
        action_create(generator),
        action_destroy,
        action_execute
    );
}

std::ostream& operator<<(std::ostream& o, const ConstAction& action)
{
    if (! action) {
        o << "IronBee::Action[!singular!]";
    } else {
        o << "IronBee::Action[" << action.name() << "]";
    }
    return o;
}

// ConstActionInstance

ConstActionInstance::ConstActionInstance() :
    m_ib(NULL)
{
    // nop
}

ConstActionInstance::ConstActionInstance(ib_type ib_action_instance) :
    m_ib(ib_action_instance)
{
    // nop
}

ConstAction ConstActionInstance::action() const
{
    return ConstAction(ib_action_inst_action(ib()));
}

const char* ConstActionInstance::parameters() const
{
    return ib_action_inst_parameters(ib());
}

void* ConstActionInstance::data() const
{
    return ib_action_inst_data(ib());
}

void ConstActionInstance::execute(
    const ib_rule_exec_t* rule_exec
) const
{
    throw_if_error(
        ib_action_inst_execute(
            ib(),
            rule_exec
        )
    );
}

// ActionInstance

ActionInstance ActionInstance::remove_const(ConstActionInstance action_instance)
{
    return ActionInstance(const_cast<ib_type>(action_instance.ib()));
}

ActionInstance ActionInstance::create(
    MemoryManager memory_manager,
    Context       context,
    ConstAction   action,
    const char*   parameters
)
{
    ib_action_inst_t* actioninst;

    throw_if_error(
        ib_action_inst_create(
            &actioninst,
            memory_manager.ib(),
            context.ib(),
            action.ib(),
            parameters
        )
    );

    return ActionInstance(actioninst);
}

ActionInstance::ActionInstance() :
    m_ib(NULL)
{
    // nop
}

ActionInstance::ActionInstance(ib_type ib_action_instance) :
    ConstActionInstance(ib_action_instance),
    m_ib(ib_action_instance)
{
    // nop
}

std::ostream& operator<<(std::ostream& o, const ConstActionInstance& action_instance)
{
    if (! action_instance) {
        o << "IronBee::ActionInstance[!singular!]";
    } else {
        o << "IronBee::ActionInstance["
          << action_instance.action().name()
          << ":" << action_instance.parameters() << "]";
    }
    return o;
}

} // IronBee
