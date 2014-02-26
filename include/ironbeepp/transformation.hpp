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
 * @brief IronBee++ --- Transformation
 *
 * This file defines (Const)Transformation, a wrapper for ib_tfn_t.
 *
 * @remark Developers should be familiar with @ref ironbeepp to understand
 * aspects of this code, e.g., the public/non-virtual inheritance.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP__TRANSFORMATION__
#define __IBPP__TRANSFORMATION__

#include <ironbeepp/catch.hpp>
#include <ironbeepp/common_semantics.hpp>
#include <ironbeepp/c_trampoline.hpp>
#include <ironbeepp/engine.hpp>
#include <ironbeepp/field.hpp>

#include <ironbee/transformation.h>

#include <boost/bind.hpp>
#include <boost/function.hpp>

#include <ostream>

namespace IronBee {

/**
 * Const Transformation; equivalent to a const pointer to ib_tfn_t.
 *
 * Provides transformations ==, !=, <, >, <=, >= and evaluation as a boolean
 * for singularity via CommonSemantics.
 *
 * See Transformation for discussion of Transformation
 *
 * @sa Transformation
 * @sa ironbeepp
 * @sa ib_tfn_t
 * @nosubgrouping
 **/
class ConstTransformation :
    public CommonSemantics<ConstTransformation>
{
public:
    //! C Type.
    typedef const ib_tfn_t* ib_type;

    /**
     * Construct singular ConstTransformation.
     *
     * All behavior of a singular ConstTransformation is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    ConstTransformation();

    /**
     * Lookup transformation in an engine.
     *
     * @param[in] engine Engine to lookup in.
     * @param[in] name Name of transformation.
     * @returns Transformation.
     **/
    static ConstTransformation lookup(Engine engine, const char *name);

    /**
     * Transformation.
     *
     * Arguments are memory manager to use, and input field.
     * Should return result of transformation.
     **/
    typedef boost::function<ConstField(MemoryManager, ConstField)>
        transformation_t;

    /**
     * Create transformation from a functional.
     *
     * @param[in] memory_manager Memory manager to allocate memory from.
     * @param[in] name Name of transformation.
     * @param[in] handle_list Handle lists directly?
     * @param[in] transformation Transformation function.
     * @returns New transformation.
     **/
    static
    ConstTransformation create(
        MemoryManager    memory_manager,
        const char*      name,
        bool             handle_list,
        transformation_t transformation
    );

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! const ib_tfn_t accessor.
    // Intentionally inlined.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct Transformation from ib_tfn_t.
    explicit
    ConstTransformation(ib_type ib_transformation);

    ///@}

    //! Name of transformation.
    const char *name() const;

    //! Directly handle lists?
    bool handle_list() const;

    /**
     * Register with an engine.
     *
     * @param[in] engine Engine to register with.
     **/
    void register_with(Engine engine);

    /**
     * Execute a transformation.
     *
     * @param[in] mm     Memory manager to use.
     * @param[in] input  Input to transformation.
     * @return Result.
     **/
    ConstField execute(
        MemoryManager mm,
        ConstField input
    ) const;

private:
    ib_type m_ib;
};

/**
 * Transformation; equivalent to a pointer to ib_tfn_t.
 *
 * Transformation can be treated as ConstTransformation.  See @ref ironbeepp
 * for details on IronBee++ object semantics.
 *
 * Transformations are functions that take fields as inputs and produce
 * fields as outputs, possibly the same field.  Unlike operators, they have
 * no state or auxiliary outputs.
 *
 * Transformations almost never exist in non-const form.
 *
 * @sa ConstTransformation
 * @sa ironbeepp
 * @sa ib_tfn_t
 * @nosubgrouping
 **/
class Transformation :
    public ConstTransformation
{
public:
    //! C Type.
    typedef ib_tfn_t* ib_type;

    /**
     * Remove the constness of a ConstTransformation.
     *
     * @warning This is as dangerous as a @c const_cast, use carefully.
     *
     * @param[in] transformation ConstTransformation to remove const from.
     * @returns Transformation pointing to same underlying transformation as
     *          @a transformation.
     **/
    static Transformation remove_const(ConstTransformation transformation);

    /**
     * Construct singular Transformation.
     *
     * All behavior of a singular Transformation is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    Transformation();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! ib_tfn_t accessor.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct Transformation from ib_tfn_t.
    explicit
    Transformation(ib_type ib_transformation);

    ///@}

private:
    ib_type m_ib;
};

/**
 * Output transformation for Transformation.
 *
 * Output IronBee::Transformation[@e value] where @e value is the name.
 *
 * @param[in] o Ostream to output to.
 * @param[in] op Transformation to output.
 * @return @a o
 **/
std::ostream& operator<<(std::ostream& o, const ConstTransformation& op);

} // IronBee

#endif
