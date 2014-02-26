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
 * @brief IronBee++ --- Operator
 *
 * This file defines (Const)Operator, a wrapper for ib_operator_t.
 *
 * @remark Developers should be familiar with @ref ironbeepp to understand
 * aspects of this code, e.g., the public/non-virtual inheritance.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP__OPERATOR__
#define __IBPP__OPERATOR__

#include <ironbeepp/catch.hpp>
#include <ironbeepp/common_semantics.hpp>
#include <ironbeepp/context.hpp>
#include <ironbeepp/c_trampoline.hpp>
#include <ironbeepp/engine.hpp>
#include <ironbeepp/field.hpp>
#include <ironbeepp/transaction.hpp>

#include <ironbee/operator.h>

#include <boost/bind.hpp>
#include <boost/function.hpp>

#include <ostream>

namespace IronBee {

/**
 * Const Operator; equivalent to a const pointer to ib_operator_t.
 *
 * Provides operators ==, !=, <, >, <=, >= and evaluation as a boolean for
 * singularity via CommonSemantics.
 *
 * See Operator for discussion of Operator
 *
 * @sa Operator
 * @sa ironbeepp
 * @sa ib_operator_t
 * @nosubgrouping
 **/
class ConstOperator :
    public CommonSemantics<ConstOperator>
{
public:
    //! C Type.
    typedef const ib_operator_t* ib_type;

    /**
     * Construct singular ConstOperator.
     *
     * All behavior of a singular ConstOperator is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    ConstOperator();

    /**
     * Lookup operator in an engine.
     *
     * @param[in] engine Engine to lookup in.
     * @param[in] name Name of operator.
     * @returns Operator.
     **/
    static ConstOperator lookup(Engine engine, const char *name);

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! const ib_operator_t accessor.
    // Intentionally inlined.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct Operator from ib_operator_t.
    explicit
    ConstOperator(ib_type ib_operator);

    ///@}

    //! Name of operator.
    const char *name() const;

    //! Capabilities of operator.
    ib_flags_t capabilities() const;

    /**
     * Register with an engine.
     *
     * @param[in] engine Engine to register with.
     **/
    void register_with(Engine engine);

    /**
     * Register stream operator with an engine.
     *
     * @param[in] engine Engine to register with.
     **/
    void register_stream_with(Engine engine);

    /**
     * Create an operator instance.
     *
     * @param[in] context Context of creation.
     * @param[in] required_capabilities Capabilities the operator must have.
     * @param[in] parameters Parameters to operator.
     * @return Instance data to use with execute_instance() and
     *         destroy_instance().
     **/
    void* create_instance(
        Context     context,
        ib_flags_t  required_capabilities,
        const char* parameters
    ) const;

    /**
     * Execute an operator instance.
     *
     * @param[in] instance_data Instance data returned by create_instance().
     * @param[in] transaction Current transaction.
     * @param[in] input Input to operator.
     * @param[in] capture Collection to capture to; can be singular.
     * @return Result.
     **/
    int execute_instance(
        void*       instance_data,
        Transaction transaction,
        ConstField  input,
        Field       capture = Field()
    ) const;

    /**
     * Destroy an operator instance.
     *
     * @param[in] instance_data Instance data return by create_instance().
     **/
    void destroy_instance(void* instance_data) const;

private:
    ib_type m_ib;
};

/**
 * Operator; equivalent to a pointer to ib_operator_t.
 *
 * Operator can be treated as ConstOperator.  See @ref ironbeepp for
 * details on IronBee++ object semantics.
 *
 * Operators are functions that take fields as inputs, possibly capture
 * information to a capture collection, and return a numeric result.  They
 * frequently function as boolean predicates in rules.
 *
 * @sa ConstOperator
 * @sa ironbeepp
 * @sa ib_operator_t
 * @nosubgrouping
 **/
class Operator :
    public ConstOperator
{
public:
    //! C Type.
    typedef ib_operator_t* ib_type;

    /**
     * Remove the constness of a ConstOperator.
     *
     * @warning This is as dangerous as a @c const_cast, use carefully.
     *
     * @param[in] op ConstOperator to remove const from.
     * @returns Operator pointing to same underlying operator as @a operator.
     **/
    static Operator remove_const(ConstOperator op);

    /**
     * Create from 0-3 functionals.
     *
     * @tparam InstanceData Type of data used by instances.
     * @param[in] memory_manager Memory manager to allocate memory from.
     * @param[in] name Name of operator.
     * @param[in] capabilities Capabilities of operator.
     * @param[in] create Functional to call on creation.  Passed context and
     *                   parameters and should return a pointer to
     *                   InstanceData.  If singular, defaults to nop.
     * @param[in] execute Functional to call on execution. Passed transaction,
     *                    the instance data the create functional returned,
     *                    an input field, and a capture field.  Should return
     *                    result (0 = false, 1 = true usually).  If singular,
     *                    defaults to constant 1 function.
     * @param[in] destroy Functional to call on destruction.  Passed instance
     *                    data the create functional returned.  Defaults to
     *                    nop.
     * @returns New operator.
     **/
    template <typename InstanceData>
    static Operator create(
        MemoryManager memory_manager,
        const char*   name,
        ib_flags_t    capabilities,
        boost::function<InstanceData*(Context, const char*)>
            create,
        boost::function<void(InstanceData*)>
            destroy,
        boost::function<int(Transaction, InstanceData*, ConstField, Field)>
            execute
    );

    /**
     * Operator instance as functional.
     *
     * A functional to call as an operator instance.  Generated by an
     * operator_generator_t.  See non-templated create().
     *
     * Parameters are current transaction, input field, and capture field.
     * Return value is result.
     **/
    typedef boost::function<int(Transaction, ConstField, Field)> operator_instance_t;

    /**
     * Operator as operator instance generator.
     *
     * A functional to call to generate an operator_instance_t.  See
     * non-templated create().
     *
     * Parameters are current context and parameters.  Return value is an
     * operator_instance_t.
     **/
    typedef boost::function<operator_instance_t(Context, const char*)> operator_generator_t;

    /**
     * Create operator from a single generator functional.
     *
     * @param[in] memory_manager Memory manager to allocate memory from.
     * @param[in] name Name of operator.
     * @param[in] capabilities Capabilities of operator.
     * @param[in] generator Functional to call when a new instance is needed.
     *                      Is passed context and parameters and should return
     *                      a new functional that will be called with
     *                      transaction, input, and capture on instance
     *                      execution.
     * @returns New operator.
     **/
    static
    Operator create(
        MemoryManager        memory_manager,
        const char*          name,
        ib_flags_t           capabilities,
        operator_generator_t generator
    );

    /**
     * Construct singular Operator.
     *
     * All behavior of a singular Operator is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    Operator();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! ib_operator_t accessor.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct Operator from ib_operator_t.
    explicit
    Operator(ib_type ib_operator);

    ///@}

private:
    ib_type m_ib;
};

/**
 * Output operator for Operator.
 *
 * Output IronBee::Operator[@e value] where @e value is the name.
 *
 * @param[in] o Ostream to output to.
 * @param[in] op Operator to output.
 * @return @a o
 **/
std::ostream& operator<<(std::ostream& o, const ConstOperator& op);

// Implementation

namespace Impl {

struct operator_create_data_t
{
    std::pair<ib_operator_create_fn_t,  void*> create_trampoline;
    std::pair<ib_operator_execute_fn_t, void*> execute_trampoline;
    std::pair<ib_operator_destroy_fn_t, void*> destroy_trampoline;
};

void operator_cleanup(operator_create_data_t data);

namespace {

template <typename InstanceData>
ib_status_t operator_create_translator(
    boost::function<InstanceData*(Context, const char*)>
        create,
    ib_context_t* ib_context,
    const char*   parameters,
    void*         instance_data
)
{
    Context context(ib_context);
    try {
        *(void **)instance_data = value_to_data(
            create(context, parameters),
            context.engine().main_memory_mm().ib()
        );
    }
    catch (...) {
        return convert_exception(context.engine());
    }
    return IB_OK;
}

template <typename InstanceData>
ib_status_t operator_execute_translator(
    boost::function<int(Transaction, InstanceData*, ConstField, Field)>
        execute,
    ib_tx_t*    ib_tx,
    void*       raw_instance_data,
    const ib_field_t* ib_field,
    ib_field_t* ib_capture,
    ib_num_t*   result
)
{
    Transaction tx(ib_tx);
    ConstField field(ib_field);
    Field capture(ib_capture);
    try {
        *result = execute(
            tx,
            data_to_value<InstanceData*>(raw_instance_data),
            field,
            capture
        );
    }
    catch (...) {
        return convert_exception(tx.engine());
    }
    return IB_OK;
}

template <typename InstanceData>
ib_status_t operator_destroy_translator(
    boost::function<void(InstanceData*)>
        destroy,
    void* raw_instance_data
)
{
    try {
        destroy(data_to_value<InstanceData*>(raw_instance_data));
    }
    catch (...) {
        return convert_exception();
    }
    return IB_OK;
}

}
} // Impl

template <typename InstanceData>
Operator Operator::create(
    MemoryManager memory_manager,
    const char*   name,
    ib_flags_t    capabilities,
    boost::function<InstanceData*(Context, const char*)> create,
    boost::function<void(InstanceData*)> destroy,
    boost::function<int(Transaction, InstanceData*, ConstField, Field)> execute
)
{
    Impl::operator_create_data_t data;

    if (create) {
        data.create_trampoline = make_c_trampoline<
            ib_status_t(ib_context_t*, const char *, void *)
        >(
            boost::bind(
                &Impl::operator_create_translator<InstanceData>,
                create, _1, _2, _3
            )
        );
    }
    if (execute) {
        data.execute_trampoline = make_c_trampoline<
            ib_status_t(ib_tx_t*, void*, const ib_field_t*, ib_field_t*, ib_num_t*)
        >(
            boost::bind(
                &Impl::operator_execute_translator<InstanceData>,
                execute, _1, _2, _3, _4, _5
            )
        );
    }
    if (destroy) {
        data.destroy_trampoline = make_c_trampoline<
            ib_status_t(void *)
        >(
            boost::bind(
                &Impl::operator_destroy_translator<InstanceData>,
                destroy, _1
            )
        );
    }

    ib_operator_t* op;
    throw_if_error(
        ib_operator_create(
            &op,
            memory_manager.ib(),
            name,
            capabilities,
            data.create_trampoline.first,  data.create_trampoline.second,
            data.destroy_trampoline.first, data.destroy_trampoline.second,
            data.execute_trampoline.first, data.execute_trampoline.second
        )
    );

    memory_manager.register_cleanup(
        boost::bind(&Impl::operator_cleanup, data)
    );

    return Operator(op);
}

} // IronBee

#endif
