
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

ConstOperator ConstOperator::lookup(
    Engine      engine,
    const char *name,
    size_t      name_length
)
{
    const ib_operator_t* op;

    throw_if_error(ib_operator_lookup(engine.ib(), name, name_length, &op));

    return ConstOperator(op);
}

ConstOperator ConstOperator::lookup(
    Engine             engine,
    const std::string& name
)
{
    return lookup(engine, name.data(), name.length());
}

ConstOperator ConstOperator::stream_lookup(
    Engine      engine,
    const char *name,
    size_t      name_length
)
{
    const ib_operator_t* op;

    throw_if_error(
        ib_operator_stream_lookup(engine.ib(), name, name_length, &op)
    );

    return ConstOperator(op);
}

ConstOperator ConstOperator::stream_lookup(
    Engine             engine,
    const std::string& name
)
{
    return stream_lookup(engine, name.data(), name.length());
}

const char* ConstOperator::name() const
{
    return ib_operator_name(ib());
}

ib_flags_t ConstOperator::capabilities() const
{
    return ib_operator_capabilities(ib());
}

void ConstOperator::register_with(Engine engine)
{
    throw_if_error(ib_operator_register(engine.ib(), ib()));
}

void ConstOperator::register_stream_with(Engine engine)
{
    throw_if_error(ib_operator_stream_register(engine.ib(), ib()));
}

// Operator

Operator Operator::remove_const(ConstOperator operator_)
{
    return Operator(const_cast<ib_type>(operator_.ib()));
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

    void* operator()(
        Context context,
        MemoryManager memory_manager,
        const char* name
    ) const
    {
        return value_to_data(m_generator(context, memory_manager, name));
    }

private:
    Operator::operator_generator_t m_generator;
};

int operator_execute(
    Transaction tx,
    ConstField  field,
    Field       capture,
    void*       instance_data
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

std::ostream& operator<<(std::ostream& o, const ConstOperator& operator_)
{
    if (! operator_) {
        o << "IronBee::Operator[!singular!]";
    } else {
        o << "IronBee::Operator[" << operator_.name() << "]";
    }
    return o;
}

// ConstOperatorInstance

ConstOperatorInstance::ConstOperatorInstance() :
    m_ib(NULL)
{
    // nop
}

ConstOperatorInstance::ConstOperatorInstance(ib_type ib_operator_instance) :
    m_ib(ib_operator_instance)
{
    // nop
}

ConstOperator ConstOperatorInstance::operator_() const
{
    return ConstOperator(ib_operator_inst_operator(ib()));
}

const char* ConstOperatorInstance::parameters() const
{
    return ib_operator_inst_parameters(ib());
}

void* ConstOperatorInstance::data() const
{
    return ib_operator_inst_data(ib());
}

int ConstOperatorInstance::execute(
    Transaction tx,
    ConstField input,
    Field capture
) const
{
    ib_num_t result;
    throw_if_error(
        ib_operator_inst_execute(
            ib(),
            tx.ib(),
            input.ib(),
            capture.ib(),
            &result
        )
    );

    return static_cast<int>(result);
}

// OperatorInstance

OperatorInstance OperatorInstance::remove_const(ConstOperatorInstance operator_instance)
{
    return OperatorInstance(const_cast<ib_type>(operator_instance.ib()));
}

OperatorInstance OperatorInstance::create(
    MemoryManager memory_manager,
    Context context,
    ConstOperator op,
    ib_flags_t required_capabilities,
    const char* parameters
)
{
    ib_operator_inst_t* opinst;

    throw_if_error(
        ib_operator_inst_create(
            &opinst,
            memory_manager.ib(),
            context.ib(),
            op.ib(),
            required_capabilities,
            parameters
        )
    );

    return OperatorInstance(opinst);
}

OperatorInstance::OperatorInstance() :
    m_ib(NULL)
{
    // nop
}

OperatorInstance::OperatorInstance(ib_type ib_operator_instance) :
    ConstOperatorInstance(ib_operator_instance),
    m_ib(ib_operator_instance)
{
    // nop
}

std::ostream& operator<<(std::ostream& o, const ConstOperatorInstance& operator_instance)
{
    if (! operator_instance) {
        o << "IronBee::OperatorInstance[!singular!]";
    } else {
        o << "IronBee::OperatorInstance["
          << operator_instance.operator_().name()
          << ":" << operator_instance.parameters() << "]";
    }
    return o;
}

} // IronBee
