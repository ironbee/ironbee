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
 * @brief IronBee++ --- Operator Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbeepp/operator.hpp>

#include <ironbee/operator.h>

namespace IronBee {

// ConstOperator

ConstOperator ConstOperator::lookup(Engine engine, const char *name)
{
    const ib_operator_t* op;

    throw_if_error(
        ib_operator_lookup(engine.ib(), name, &op)
    );

    return ConstOperator(op);
}

ConstOperator::ConstOperator() :
    m_ib(NULL)
{
    // nop
}

ConstOperator::ConstOperator(ib_type ib_operator) :
    m_ib(ib_operator)
{
    // nop
}

const char* ConstOperator::name() const
{
    return ib_operator_get_name(ib());
}

ib_flags_t ConstOperator::capabilities() const
{
    return ib_operator_get_capabilities(ib());
}

void ConstOperator::register_with(Engine engine)
{
    throw_if_error(ib_operator_register(engine.ib(), ib()));
}

void ConstOperator::register_stream_with(Engine engine)
{
    throw_if_error(ib_operator_stream_register(engine.ib(), ib()));
}

void* ConstOperator::create_instance(
    Context     context,
    ib_flags_t  required_capabilities,
    const char* parameters
) const
{
    void* instance_data = NULL;
    throw_if_error(
        ib_operator_inst_create(
            ib(),
            context.ib(),
            required_capabilities,
            parameters,
            &instance_data
        )
    );

    return instance_data;
}

int ConstOperator::execute_instance(
    void*       instance_data,
    Transaction transaction,
    ConstField  input,
    Field       capture
) const
{
    ib_num_t result = 0;
    throw_if_error(
        ib_operator_inst_execute(
            ib(),
            instance_data,
            transaction.ib(),
            input.ib(),
            capture.ib(),
            &result
        )
    );

    return result;
}

void ConstOperator::destroy_instance(void* instance_data) const
{
    throw_if_error(
        ib_operator_inst_destroy(ib(), instance_data)
    );
}

// Operator

namespace Impl {

void operator_cleanup(operator_create_data_t data)
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

class operator_create
{
public:
    operator_create(Operator::operator_generator_t generator) :
        m_generator(generator)
    {
        // nop
    }

    void* operator()(Context context, const char* name) const
    {
        return value_to_data(m_generator(context, name));
    }

private:
    Operator::operator_generator_t m_generator;
};

int operator_execute(
    Transaction tx,
    void*       instance_data,
    ConstField  field,
    Field       capture
)
{
    return data_to_value<Operator::operator_instance_t>(instance_data)(
        tx,
        field,
        capture
    );
}

void operator_destroy(
    void* instance_data
)
{
    delete reinterpret_cast<boost::any*>(instance_data);
}

}

Operator Operator::create(
    MemoryManager        memory_manager,
    const char*          name,
    ib_flags_t           capabilities,
    operator_generator_t generator
)
{
    return create<void>(
        memory_manager,
        name,
        capabilities,
        operator_create(generator),
        operator_destroy,
        operator_execute
    );
}

Operator Operator::remove_const(ConstOperator op)
{
    return Operator(const_cast<ib_type>(op.ib()));
}

Operator::Operator() :
    m_ib(NULL)
{
    // nop
}

Operator::Operator(ib_type ib_operator) :
    ConstOperator(ib_operator),
    m_ib(ib_operator)
{
    // nop
}

std::ostream& operator<<(std::ostream& o, const ConstOperator& op)
{
    if (! op) {
        o << "IronBee::Operator[!singular!]";
    } else {
        o << "IronBee::Operator[" << op.name() << "]";
    }
    return o;
}

} // IronBee
