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
 * Const Transformation Instance; equivalent to a const pointer to @ref ib_tfn_inst_t.
 *
 * Provides transformations ==, !=, <, >, <=, >= and evaluation as a boolean
 * for singularity via CommonSemantics.
 *
 * @sa Transformation
 * @sa ironbeepp
 * @sa ib_tfn_inst_t
 * @nosubgrouping
 */
 class ConstTransformationInstance :
    public CommonSemantics<ConstTransformationInstance>
{
public:
    //! C Type
    typedef const ib_tfn_inst_t* ib_type;

    ConstTransformationInstance();


    /**
     * Execute a transformation instance.
     *
     * @param[in] memory_manager The memory manager that allocations should be
     *            done out of.
     * @param[in] input The input field.
     *
     * @returns output The result of the transformation is stored here.
     *          It is valid for this to be copied from @a input.
     *          That is, @a input.ib() == @a output.ib().
     */
    ConstField execute(MemoryManager memory_manager, ConstField input) const;

    //! Name of transformation.
    const char *name() const;

    //! Name of transformation.
    const char *param() const;

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! Construct TransformationInstance from ib_tfn_inst_t.
    explicit
    ConstTransformationInstance(ib_type ib_transformation_instance);

    //! ib_tfn_t accessor.
    ib_type ib() const
    {
        return m_ib;
    }

    ///@}

private:
    ib_type m_ib;
};

class ConstTransformation;

class TransformationInstance :
    public ConstTransformationInstance
{
public:
    //! C Type.
    typedef ib_tfn_inst_t* ib_type;

    /**
     * Remove the constness of a ConstTransformation.
     *
     * @warning This is as dangerous as a @c const_cast, use carefully.
     *
     * @param[in] transformation_instance ConstTransformation to remove
     *            const from.
     * @returns Transformation pointing to same underlying transformation as
     *          @a transformation.
     **/
    static TransformationInstance remove_const(
        ConstTransformationInstance transformation_instance);

    /**
     * Construct singular Transformation.
     *
     * All behavior of a singular Transformation is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    TransformationInstance();

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
    TransformationInstance(ib_type ib_transformation);

    /**
     * Create a new transformation instance.
     *
     * @param[in] tfn Transformation to create an instance of.
     * @param[in] memory_manager Allocations are done from this.
     *            This memory manager is also responsible for destroying
     *            the @ref ib_tfn_inst_t that is created and back the
     *            returned ConstTransformationInstance.
     * @param[in] parameters The parameters to the transformation instance.
     *
     * @returns Trasformation instance.
     */
    static TransformationInstance create(
        ConstTransformation tfn,
        MemoryManager memory_manager,
        const char* parameters
    );

    ///@}

private:
    ib_type m_ib;
};

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
     * Create a new transformation instance.
     *
     * @param[in] memory_manager Allocations are done from this.
     *            This memory manager is also responsible for destroying
     *            the @ref ib_tfn_inst_t that is created and back the
     *            returned ConstTransformationInstance.
     * @param[in] parameters The parameters to the transformation instance.
     *
     * @returns Trasformation instance.
     */
    TransformationInstance create_instance(
        MemoryManager memory_manager,
        const char* parameters
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
     * Construct singular Transformation.
     *
     * All behavior of a singular Transformation is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    Transformation();

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
     * Create from 0-3 functionals.
     *
     * @tparam InstanceData Type of data used by instances.
     * @param[in] memory_manager Memory manager to allocate memory from.
     * @param[in] name Name of operator.
     * @param[in] handle_list Does this Transformation handle
     *            a list of values or each element in the list, separately.
     * @param[in] create Functional to call on creation.  Passed context and
     *                   parameters and should return a pointer to
     *                   InstanceData.  If singular, defaults to nop.
     * @param[in] destroy Functional to call on destruction.  Passed instance
     *                    data the create functional returned.  Defaults to
     *                    nop.
     * @returns New transformation.
     **/
    template <typename InstanceData>
    static Transformation create(
        MemoryManager memory_manager,
        const char*   name,
        bool          handle_list,
        boost::function<InstanceData*(ib_mm_t, const char*)>
            create,
        boost::function<void(InstanceData*)>
            destroy,
        boost::function<ConstField(InstanceData*, MemoryManager, ConstField)>
            execute
    );

    /**
     * Transformation as transformation instance generator.
     *
     * A functional to call to generate an TransformationInstance.  See
     * non-templated create().
     *
     * Parameters are a memory manager and the constructor parameter.
     * Return value is an TransformationInstance.
     **/
    typedef boost::function<
        TransformationInstance(ib_mm_t, const char*)
    > transformation_generator_t;

    /**
     * Create transformation from a functional.
     *
     * @param[in] memory_manager Memory manager to allocate memory from.
     * @param[in] name Name of transformation.
     * @param[in] handle_list Handle lists directly?
     * @param[in] transformation_generator Functional to call when a new
     *            instance is needed. Is parameters and should return
     *            a new functional that will be called with
     *            transaction, and input on instance execution.
     * @returns New transformation.
     **/
    static
    Transformation create(
        MemoryManager              memory_manager,
        const char*                name,
        bool                       handle_list,
        transformation_generator_t transformation_generator
    );

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

/**
 * Output transformation for TransformationInstance.
 *
 * Output IronBee::TransformationInstance[@e value(param)] where @e value is the name.
 *
 * @param[in] o Ostream to output to.
 * @param[in] op Transformation to output.
 * @return @a o
 */
std::ostream& operator<<(std::ostream& o, const ConstTransformationInstance& inst);

// Implementation

namespace Impl {

struct transformation_create_data_t
{
    std::pair<ib_tfn_create_fn_t,  void*> create_trampoline;
    std::pair<ib_tfn_execute_fn_t, void*> execute_trampoline;
    std::pair<ib_tfn_destroy_fn_t, void*> destroy_trampoline;
};

void transformation_cleanup(transformation_create_data_t data);

namespace {

template <typename InstanceData>
ib_status_t transformation_create_translator(
    boost::function<InstanceData*(ib_mm_t, const char*)> create,
    InstanceData* instance_data,
    ib_mm_t       mm,
    const char*   parameters
)
{
    try {
        *(void **)instance_data = value_to_data(
            create(mm, parameters),
            mm
        );
    }
    catch (...) {
        return convert_exception();
    }
    return IB_OK;
}

template <typename InstanceData>
ib_status_t transformation_execute_translator(
    boost::function<ConstField(InstanceData*, MemoryManager, ConstField)>
                       execute,
    void*              raw_instance_data,
    ib_mm_t            ib_mm,
    const ib_field_t*  ib_field_in,
    const ib_field_t** ib_field_out
)
{
    try {
        InstanceData* instance_data = NULL;

        /* Only unwrap raw_instance_data if a constructor was defined.
         * Otherwise, this will be NULL and should not be touched. */
        if (raw_instance_data) {
            instance_data = data_to_value<InstanceData*>(raw_instance_data);
        }

        ConstField result = execute(
            instance_data,
            MemoryManager(ib_mm),
            ConstField(ib_field_in)
        );

        *ib_field_out = result.ib();

        return IB_OK;
    }
    catch (...) {
        return convert_exception();
    }
}

template <typename InstanceData>
void transformation_destroy_translator(
    boost::function<void(InstanceData*)>
        destroy,
    void* raw_instance_data
)
{
    try {
        destroy(data_to_value<InstanceData*>(raw_instance_data));
    }
    catch (...) {
        // nop
    }
}



} // anonymous namespace

} // namespace Impl

template <typename InstanceData>
Transformation Transformation::create(
    MemoryManager memory_manager,
    const char*   name,
    bool          handle_list,
    boost::function<InstanceData*(ib_mm_t, const char*)>
        create,
    boost::function<void(InstanceData*)>
        destroy,
    boost::function<ConstField(InstanceData*, MemoryManager, ConstField)>
        execute
)
{
    Impl::transformation_create_data_t data;

    if (create) {
        data.create_trampoline = make_c_trampoline<
            ib_status_t(void*, ib_mm_t, const char *)
        >(
            boost::bind(
                &Impl::transformation_create_translator<InstanceData>,
                create, _1, _2, _3
            )
        );
    }
    if (destroy) {
        data.destroy_trampoline = make_c_trampoline<
            void(void *)
        >(
            boost::bind(
                &Impl::transformation_destroy_translator<InstanceData>,
                destroy, _1
            )
        );
    }
    if (execute) {
        data.execute_trampoline = make_c_trampoline<
            ib_status_t(void*, ib_mm_t, const ib_field_t *, const ib_field_t **)
        >(
            boost::bind(
                &Impl::transformation_execute_translator<InstanceData>,
                execute, _1, _2, _3, _4
            )
        );
    }

    ib_tfn_t *tfn;

    throw_if_error(
        ib_tfn_create(
            const_cast<const ib_tfn_t **>(&tfn),
            memory_manager.ib(),
            name,
            handle_list,
            data.create_trampoline.first,  data.create_trampoline.second,
            data.execute_trampoline.first, data.execute_trampoline.second,
            data.destroy_trampoline.first, data.destroy_trampoline.second
        )
    );

    memory_manager.register_cleanup(
        boost::bind(&Impl::transformation_cleanup, data)
    );

    return Transformation(tfn);
}

} // IronBee

#endif
