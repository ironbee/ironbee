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
 * @brief IronBee++ --- Action
 *
 * This file defines (Const)Action, a wrapper for ib_action_t and
 * (Const)ActionInstance, a wrapper for ib_action_inst_t.
 *
 * @remark Developers should be familiar with @ref ironbeepp to understand
 * aspects of this code, e.g., the public/non-virtual inheritance.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP__ACTION__
#define __IBPP__ACTION__

#include <ironbeepp/catch.hpp>
#include <ironbeepp/common_semantics.hpp>
#include <ironbeepp/context.hpp>
#include <ironbeepp/c_trampoline.hpp>
#include <ironbeepp/engine.hpp>
#include <ironbeepp/field.hpp>
#include <ironbeepp/memory_manager.hpp>
#include <ironbeepp/transaction.hpp>

#include <ironbee/action.h>
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
#include <boost/function.hpp>

#include <ostream>

namespace IronBee {

/**
 * Const Action; equivalent to a const pointer to ib_action_t.
 *
 * Provides actions ==, !=, <, >, <=, >= and evaluation as a boolean for
 * singularity via CommonSemantics.
 *
 * See Action for discussion of Action
 *
 * @sa Action
 * @sa ironbeepp
 * @sa ib_action_t
 * @nosubgrouping
 **/
class ConstAction :
    public CommonSemantics<ConstAction>
{
public:
    //! C Type.
    typedef const ib_action_t* ib_type;

    /**
     * Construct singular ConstAction.
     *
     * All behavior of a singular ConstAction is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    ConstAction();

    /**
     * Lookup action in an engine.
     *
     * @param[in] engine      Engine to lookup in.
     * @param[in] name        Name of action.
     * @param[in] name_length Length of @a name.
     * @returns Action.
     **/
    static
    ConstAction lookup(
        Engine      engine,
        const char *name,
        size_t      name_length
    );

    /**
     * Lookup action in an engine.
     *
     * @param[in] engine      Engine to lookup in.
     * @param[in] name        Name of action.
     * @returns Action.
     **/
    static
    ConstAction lookup(
        Engine             engine,
        const std::string& name
    );

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! const ib_action_t accessor.
    // Intentionally inlined.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct Action from ib_action_t.
    explicit
    ConstAction(ib_type ib_action);

    ///@}

    //! Name of action.
    const char* name() const;

    /**
     * Register with an engine.
     *
     * @param[in] engine Engine to register with.
     **/
    void register_with(Engine engine);

private:
    ib_type m_ib;
};

/**
 * Action; equivalent to a pointer to ib_action_t.
 *
 * Action can be treated as ConstAction.  See @ref ironbeepp
 * for details on IronBee++ object semantics.
 *
 * A action represents a manipulation of data.
 *
 * @sa ConstAction
 * @sa ironbeepp
 * @sa ib_action_t
 * @nosubgrouping
 **/
class Action :
    public ConstAction
{
public:
    //! C Type.
    typedef ib_action_t* ib_type;

    /**
     * Remove the constness of a ConstAction.
     *
     * @warning This is as dangerous as a @c const_cast, use carefully.
     *
     * @param[in] action ConstAction to remove const from.
     * @returns Action pointing to same underlying action as @a action.
     **/
    static Action remove_const(ConstAction action);

    /**
     * Construct singular Action.
     *
     * All behavior of a singular Action is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    Action();

    /**
     * Create from 0-3 functionals.
     *
     * @tparam InstanceData Type of data used by instances.
     * @param[in] memory_manager Memory manager to allocate memory from.
     * @param[in] name Name of action.
     * @param[in] create Functional to call on creation.  Passed memory
     *                   manager, context, and parameters and should return a
     *                   pointer to InstanceData.  If singular, defaults to
     *                   nop.
     * @param[in] destroy Functional to call on destruction.  Passed instance
     *                    data the create functional returned.  Defaults to
     *                    nop.
     * @param[in] execute Functional to call on execution. Passed rule
     *                    execution, and instance data.  Cannot be singular.
     * @returns New action.
     **/
    template <typename InstanceData>
    static Action create(
        MemoryManager memory_manager,
        const char*   name,
        boost::function<InstanceData*(MemoryManager, Context,const char*)>
            create,
        boost::function<void(InstanceData*)>
            destroy,
        boost::function<void(const ib_rule_exec_t*, InstanceData*)>
            execute
    );

    /**
     * Action as functional.
     *
     * A functional to call as an action.  Generated by a
     * @ref action_generator_t.  See non-templated create().
     **/
    typedef boost::function<void(const ib_rule_exec_t *)> action_instance_t;

    /**
     * Action as action instance generator.
     *
     * A functional to call to generate an @ref action_instance_t.
     * See non-templated create().
     *
     * Parameters are engine, memory manager, and parameters.
     * Return value is an @ref action_instance_t.
     **/
    typedef boost::function<
        action_instance_t(
            MemoryManager,
            Context,
            const char*
        )
    > action_generator_t;

    /**
     * Create action from a single generator functional.
     *
     * @param[in] memory_manager Memory manager to allocate memory from.
     * @param[in] name Name of action.
     * @param[in] generator Functional to call when a new instance is needed.
     *                      Is passed context and parameters and should return
     *                      a new functional that will be called with
     *                      transaction, input, and capture on instance
     *                      execution.
     * @returns New action.
     **/
    static
    Action create(
        MemoryManager      memory_manager,
        const char*        name,
        action_generator_t generator
    );

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! ib_action_t accessor.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct Action from ib_action_t.
    explicit
    Action(ib_type ib_action);

    ///@}

private:
    ib_type m_ib;
};

/**
 * Output action for Action.
 *
 * Output IronBee::Action[@e value] where @e value is the name.
 *
 * @param[in] o Ostream to output to.
 * @param[in] action Action to output.
 * @return @a o
 **/
std::ostream& operator<<(std::ostream& o, const ConstAction& action);

/**
 * Const ActionInstance; equivalent to a const pointer to ib_action_inst_t.
 *
 * Provides actions ==, !=, <, >, <=, >= and evaluation as a boolean for
 * singularity via CommonSemantics.
 *
 * See ActionInstance for discussion of ActionInstance
 *
 * @sa ActionInstance
 * @sa ironbeepp
 * @sa ib_action_inst_t
 * @nosubgrouping
 **/
class ConstActionInstance :
    public CommonSemantics<ConstActionInstance>
{
public:
    //! C Type.
    typedef const ib_action_inst_t* ib_type;

    /**
     * Construct singular ConstActionInstance.
     *
     * All behavior of a singular ConstActionInstance is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    ConstActionInstance();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! const ib_action_inst_t accessor.
    // Intentionally inlined.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct ActionInstance from ib_action_inst_t.
    explicit
    ConstActionInstance(ib_type ib_action_instance);

    ///@}

    //! Action accessor.
    ConstAction action() const;

    //! Parameters accessor.
    const char* parameters() const;

    //! Data accessor.
    void* data() const;

    /**
     * Execute action instance.
     *
     * @param[in] rule_exec Rule execution.
     **/
    void execute(
        const ib_rule_exec_t* rule_exec
    ) const;

private:
    ib_type m_ib;
};

/**
 * ActionInstance; equivalent to a pointer to ib_action_inst_t.
 *
 * ActionInstance can be treated as ConstActionInstance.  See @ref ironbeepp for
 * details on IronBee++ object semantics.
 *
 * An action instance is an instantiation of an Action for a particular
 * context and set of parameters.
 *
 * @sa ConstActionInstance
 * @sa ironbeepp
 * @sa ib_action_inst_t
 * @nosubgrouping
 **/
class ActionInstance :
    public ConstActionInstance
{
public:
    //! C Type.
    typedef ib_action_inst_t* ib_type;

    /**
     * Remove the constness of a ConstActionInstance.
     *
     * @warning This is as dangerous as a @c const_cast, use carefully.
     *
     * @param[in] action_instance ConstActionInstance to remove const from.
     * @returns ActionInstance pointing to same underlying action instance as @a action_instance.
     **/
    static ActionInstance remove_const(ConstActionInstance action_instance);

    /**
     * Construct singular ActionInstance.
     *
     * All behavior of a singular ActionInstance is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    ActionInstance();

    /**
     * Create an action instance.
     *
     * @param[in] memory_manager Memory manager to determine lifetime.
     * @param[in] context        Context.
     * @param[in] action         Action.
     * @param[in] parameters     Parameters.
     *
     * @return Action instance for @a op.
     **/
    static
    ActionInstance create(
        MemoryManager memory_manager,
        Context       context,
        ConstAction   action,
        const char*   parameters
    );

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! ib_action_inst_t accessor.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct ActionInstance from ib_action_inst_t.
    explicit
    ActionInstance(ib_type ib_action_instance);

    ///@}

private:
    ib_type m_ib;
};

/**
 * Output action for ActionInstance.
 *
 * Output IronBee::ActionInstance[@e value] where @e value is the name
 * and parameters.
 *
 * @param[in] o Ostream to output to.
 * @param[in] action_instance ActionInstance to output.
 * @return @a o
 **/
std::ostream& operator<<(std::ostream& o, const ConstActionInstance& action_instance);

// Implementation

namespace Impl {

struct action_create_data_t
{
    std::pair<ib_action_create_fn_t,  void*> create_trampoline;
    std::pair<ib_action_execute_fn_t, void*> execute_trampoline;
    std::pair<ib_action_destroy_fn_t, void*> destroy_trampoline;
};

void action_cleanup(action_create_data_t data);

namespace {

template <typename InstanceData>
ib_status_t action_create_translator(
    boost::function<InstanceData*(MemoryManager, Context, const char*)>
        create,
    ib_mm_t       ib_memory_manager,
    ib_context_t* ib_context,
    const char*   parameters,
    void*         instance_data
)
{
    MemoryManager memory_manager(ib_memory_manager);

    try {
        *(void **)instance_data = value_to_data(
            create(memory_manager, Context(ib_context), parameters),
            memory_manager.ib()
        );
    }
    catch (...) {
        return convert_exception(Context(ib_context).engine().ib());
    }
    return IB_OK;
}

template <typename InstanceData>
ib_status_t action_execute_translator(
    boost::function<void(const ib_rule_exec_t*, InstanceData*)>
        execute,
    const ib_rule_exec_t* rule_exec,
    void*                 raw_instance_data
)
{
    try {
        execute(
            rule_exec,
            raw_instance_data ?
                data_to_value<InstanceData*>(raw_instance_data) : NULL
        );
    }
    catch (...) {
        return convert_exception(rule_exec->ib);
    }

    return IB_OK;
}

template <typename InstanceData>
void action_destroy_translator(
    boost::function<void(InstanceData*)> destroy,
    void* raw_instance_data
)
{
    destroy(data_to_value<InstanceData*>(raw_instance_data));
}

}
} // Impl

template <typename InstanceData>
Action Action::create(
    MemoryManager memory_manager,
    const char*   name,
    boost::function<InstanceData*(MemoryManager, Context, const char*)>
        create,
    boost::function<void(InstanceData*)>
        destroy,
    boost::function<void(const ib_rule_exec_t*, InstanceData*)>
        execute
)
{
    Impl::action_create_data_t data;

    if (create) {
        data.create_trampoline = make_c_trampoline<
            ib_status_t(ib_mm_t, ib_context_t *, const char *, void *)
        >(
            boost::bind(
                &Impl::action_create_translator<InstanceData>,
                create, _1, _2, _3, _4
            )
        );
    }
    if (execute) {
        data.execute_trampoline = make_c_trampoline<
            ib_status_t(const ib_rule_exec_t *, void *)
        >(
            boost::bind(
                &Impl::action_execute_translator<InstanceData>,
                execute, _1, _2
            )
        );
    }
    if (destroy) {
        data.destroy_trampoline = make_c_trampoline<
            void(void *)
        >(
            boost::bind(
                &Impl::action_destroy_translator<InstanceData>,
                destroy, _1
            )
        );
    }

    ib_action_t* action;
    throw_if_error(
        ib_action_create(
            &action,
            memory_manager.ib(),
            name,
            data.create_trampoline.first,  data.create_trampoline.second,
            data.destroy_trampoline.first, data.destroy_trampoline.second,
            data.execute_trampoline.first, data.execute_trampoline.second
        )
    );

    memory_manager.register_cleanup(
        boost::bind(&Impl::action_cleanup, data)
    );

    return Action(action);
}

} // IronBee

#endif
