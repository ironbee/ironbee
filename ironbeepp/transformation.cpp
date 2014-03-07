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
 * @brief IronBee++ --- Transformation Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbeepp/transformation.hpp>

#include <ironbee/transformation.h>

namespace IronBee {

// ConstTransformationInstance
ConstTransformationInstance::ConstTransformationInstance() :
    m_ib(NULL)
{
    // nop
}

ConstTransformationInstance::ConstTransformationInstance(
    ib_type ib_transformation_instance
) :
    m_ib(ib_transformation_instance)
{
    // nop
}

const char* ConstTransformationInstance::name() const
{
    return ib_tfn_inst_name(ib());
}

const char* ConstTransformationInstance::param() const
{
    return ib_tfn_inst_param(ib());
}

ConstField ConstTransformationInstance::execute(
    MemoryManager memory_manager,
    ConstField input
) const
{
    const ib_field_t *field;

    throw_if_error(
        ib_tfn_inst_execute(
            ib(),
            memory_manager.ib(),
            input.ib(),
            &field
        )
    );
    return ConstField(field);
}

// TransformationInstance
TransformationInstance::TransformationInstance(ib_type ib_transformation) :
    ConstTransformationInstance(ib_transformation),
    m_ib(ib_transformation)
{
    // nop
}

TransformationInstance::TransformationInstance() :
    m_ib(NULL)
{
    // nop
}

TransformationInstance TransformationInstance::remove_const(
        ConstTransformationInstance transformation_instance
)
{
    return TransformationInstance(
        const_cast<ib_type>(
            transformation_instance.ib()
        )
    );
}

// ConstTransformation

ConstTransformation::ConstTransformation() :
    m_ib(NULL)
{
    // nop
}

ConstTransformation::ConstTransformation(ib_type ib_tfn) :
    m_ib(ib_tfn)
{
    // nop
}

const char* ConstTransformation::name() const
{
    return ib_tfn_name(ib());
}

bool ConstTransformation::handle_list() const
{
    return ib_tfn_handle_list(ib());
}

TransformationInstance TransformationInstance::create(
    ConstTransformation tfn,
    MemoryManager memory_manager,
    const char* parameters
)
{
    ib_tfn_inst_t* instance_data = NULL;

    throw_if_error(
        ib_tfn_inst_create(
            const_cast<const ib_tfn_inst_t**>(&instance_data),
            memory_manager.ib(),
            tfn.ib(),
            parameters
        )
    );

    return TransformationInstance(instance_data);
}

TransformationInstance ConstTransformation::create_instance(
    MemoryManager memory_manager,
    const char*   parameters
) const
{
    return TransformationInstance::create(*this, memory_manager, parameters);
}

ConstTransformation ConstTransformation::lookup(
    Engine      engine,
    const char *name
)
{
    const ib_tfn_t* tfn;
    throw_if_error(ib_tfn_lookup(
        engine.ib(),
        name,
        &tfn
    ));

    return ConstTransformation(tfn);
}

void ConstTransformation::register_with(Engine engine)
{
    throw_if_error(ib_tfn_register(
        engine.ib(),
        ib()
    ));
}

Transformation Transformation::remove_const(ConstTransformation tfn)
{
    return Transformation(const_cast<ib_type>(tfn.ib()));
}

Transformation::Transformation() :
    m_ib(NULL)
{
    // nop
}

Transformation::Transformation(ib_type ib_tfn) :
    ConstTransformation(ib_tfn),
    m_ib(ib_tfn)
{
    // nop
}


namespace Impl {

void transformation_cleanup(transformation_create_data_t data)
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

class transformation_create
{
public:
    transformation_create(Transformation::transformation_generator_t generator) :
        m_generator(generator)
    {
        // nop
    }

    void* operator()(ib_mm_t mm, const char* name) const
    {
        return value_to_data(m_generator(mm, name), mm);
    }

private:
    Transformation::transformation_generator_t m_generator;
};

ConstField transformation_execute(
    void*         instance_data,
    MemoryManager mm,
    ConstField    fin
)
{
    return data_to_value<ConstTransformationInstance>(instance_data)
        .execute(MemoryManager(mm), ConstField(fin));

}
} // Impl

Transformation Transformation::create(
        MemoryManager              memory_manager,
        const char*                name,
        bool                       handle_list,
        transformation_generator_t transformation_generator
)
{
    return create<void>(
        memory_manager,
        name,
        handle_list,
        Impl::transformation_create(transformation_generator),
        NULL,
        Impl::transformation_execute
    );
}

std::ostream& operator<<(std::ostream& o, const ConstTransformation& tfn)
{
    if (! tfn) {
        o << "IronBee::Transformation[!singular!]";
    } else {
        o << "IronBee::Transformation[" << tfn.name() << "]";
    }
    return o;
}

std::ostream& operator<<(
    std::ostream& o,
    const ConstTransformationInstance& inst
)
{
    if (! inst) {
        o << "IronBee::Transformation[!singular!]";
    } else {
        o << "IronBee::Transformation[" << inst.name() << "(" << inst.param() << ")" << "]";
    }
    return o;
}


} // IronBee
