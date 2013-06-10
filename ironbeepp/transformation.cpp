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

extern "C" {

static ib_status_t transformation_translator(
    ib_engine_t* ib,
    ib_mpool_t* mp,
    const ib_field_t* fin,
    const ib_field_t** fout,
    void *cbdata
)
{
    try {
        ConstTransformation::transformation_t transformation =
            data_to_value<ConstTransformation::transformation_t>(cbdata);

        ConstField result =
            transformation(Engine(ib), MemoryPool(mp), ConstField(fin));

        *fout = result.ib();
    }
    catch (...) {
        return convert_exception();
    }

    return IB_OK;
}

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

ConstTransformation ConstTransformation::create(
    MemoryPool       memory_pool,
    const char*      name,
    bool             handle_list,
    transformation_t transformation
)
{
    const ib_tfn_t* tfn;
    throw_if_error(ib_tfn_create(
        &tfn,
        memory_pool.ib(),
        name,
        handle_list,
        transformation_translator,
        value_to_data(transformation, memory_pool.ib())
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

ConstField ConstTransformation::execute(
    Engine engine,
    MemoryPool pool,
    ConstField input
) const
{
    const ib_field_t* result;
    throw_if_error(ib_tfn_execute(
        engine.ib(),
        pool.ib(),
        ib(),
        input.ib(),
        &result
    ));

    return ConstField(result);
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

std::ostream& operator<<(std::ostream& o, const ConstTransformation& tfn)
{
    if (! tfn) {
        o << "IronBee::Transformation[!singular!]";
    } else {
        o << "IronBee::Transformation[" << tfn.name() << "]";
    }
    return o;
}

} // IronBee
