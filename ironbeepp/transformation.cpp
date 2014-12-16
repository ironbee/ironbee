
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

// ConstTransformation

ConstTransformation::ConstTransformation() :
    m_ib(NULL)
{
    // nop
}

ConstTransformation::ConstTransformation(ib_type ib_transformation) :
    m_ib(ib_transformation)
{
    // nop
}

ConstTransformation ConstTransformation::lookup(
    Engine      engine,
    const char* name,
    size_t      name_length
)
{
    const ib_transformation_t* tfn;

    ib_status_t rc;

    rc = ib_transformation_lookup(
        engine.ib(),
        name, name_length,
        &tfn
    );

    if (rc != IB_OK) {
        std::string msg =
            std::string("Transformation \"") +
            name +
            "\" not found.";
        throw_if_error(rc, msg.c_str());
    }

    return ConstTransformation(tfn);
}

ConstTransformation ConstTransformation::lookup(
    Engine             engine,
    const std::string& name
)
{
    return lookup(engine, name.data(), name.length());
}

const char* ConstTransformation::name() const
{
    return ib_transformation_name(ib());
}

bool ConstTransformation::handle_list() const
{
    return ib_transformation_handle_list(ib());
}

void ConstTransformation::register_with(Engine engine)
{
    throw_if_error(ib_transformation_register(engine.ib(), ib()));
}

// Transformation

Transformation Transformation::remove_const(ConstTransformation transformation)
{
    return Transformation(const_cast<ib_type>(transformation.ib()));
}

Transformation::Transformation() :
    m_ib(NULL)
{
    // nop
}

Transformation::Transformation(ib_type ib_transformation) :
    ConstTransformation(ib_transformation),
    m_ib(ib_transformation)
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

} // Impl

namespace {

class transformation_create
{
public:
    transformation_create(Transformation::transformation_generator_t generator) :
        m_generator(generator)
    {
        // nop
    }

    void* operator()(
        MemoryManager memory_manager,
        const char* name
    ) const
    {
        return value_to_data(m_generator(memory_manager, name));
    }

private:
    Transformation::transformation_generator_t m_generator;
};

ConstField transformation_execute(
    MemoryManager mm,
    ConstField    input,
    void*         instance_data
)
{
    return data_to_value<Transformation::transformation_instance_t>(instance_data)(
        mm,
        input
    );
}

void transformation_destroy(
    void* instance_data
)
{
    delete reinterpret_cast<boost::any*>(instance_data);
}

}

Transformation Transformation::create(
    MemoryManager              memory_manager,
    const char*                name,
    bool                       handle_list,
    transformation_generator_t generator
)
{
    return create<void>(
        memory_manager,
        name,
        handle_list,
        transformation_create(generator),
        transformation_destroy,
        transformation_execute
    );
}

std::ostream& operator<<(std::ostream& o, const ConstTransformation& transformation)
{
    if (! transformation) {
        o << "IronBee::Transformation[!singular!]";
    } else {
        o << "IronBee::Transformation[" << transformation.name() << "]";
    }
    return o;
}

// ConstTransformationInstance

ConstTransformationInstance::ConstTransformationInstance() :
    m_ib(NULL)
{
    // nop
}

ConstTransformationInstance::ConstTransformationInstance(ib_type ib_transformation_instance) :
    m_ib(ib_transformation_instance)
{
    // nop
}

ConstTransformation ConstTransformationInstance::transformation() const
{
    return ConstTransformation(ib_transformation_inst_transformation(ib()));
}

const char* ConstTransformationInstance::parameters() const
{
    return ib_transformation_inst_parameters(ib());
}

void* ConstTransformationInstance::data() const
{
    return ib_transformation_inst_data(ib());
}

ConstField ConstTransformationInstance::execute(
    MemoryManager mm,
    ConstField    input
) const
{
    const ib_field_t* result;
    throw_if_error(
        ib_transformation_inst_execute(
            ib(),
            mm.ib(),
            input.ib(),
            &result
        )
    );

    return ConstField(result);
}

// TransformationInstance

TransformationInstance TransformationInstance::remove_const(ConstTransformationInstance transformation_instance)
{
    return TransformationInstance(const_cast<ib_type>(transformation_instance.ib()));
}

TransformationInstance TransformationInstance::create(
    MemoryManager       memory_manager,
    ConstTransformation transformation,
    const char*         parameters
)
{
    ib_transformation_inst_t* tfninst;

    throw_if_error(
        ib_transformation_inst_create(
            &tfninst,
            memory_manager.ib(),
            transformation.ib(),
            parameters
        )
    );

    return TransformationInstance(tfninst);
}

TransformationInstance::TransformationInstance() :
    m_ib(NULL)
{
    // nop
}

TransformationInstance::TransformationInstance(ib_type ib_transformation_instance) :
    ConstTransformationInstance(ib_transformation_instance),
    m_ib(ib_transformation_instance)
{
    // nop
}

std::ostream& operator<<(std::ostream& o, const ConstTransformationInstance& transformation_instance)
{
    if (! transformation_instance) {
        o << "IronBee::TransformationInstance[!singular!]";
    } else {
        o << "IronBee::TransformationInstance["
          << transformation_instance.transformation().name()
          << ":" << transformation_instance.parameters() << "]";
    }
    return o;
}

} // IronBee
