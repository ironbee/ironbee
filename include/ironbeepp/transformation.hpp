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
 * This file defines (Const)Transformation, a wrapper for ib_transformation_t and
 * (Const)TransformationInstance, a wrapper for ib_transformation_inst_t.
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
#include <ironbeepp/context.hpp>
#include <ironbeepp/c_trampoline.hpp>
#include <ironbeepp/engine.hpp>
#include <ironbeepp/field.hpp>
#include <ironbeepp/memory_manager.hpp>
#include <ironbeepp/transaction.hpp>

#include <ironbee/transformation.h>

#ifdef __clang__
#pragma clang diagnostic push
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
 * Const Transformation; equivalent to a const pointer to ib_transformation_t.
 *
 * Provides transformations ==, !=, <, >, <=, >= and evaluation as a boolean for
 * singularity via CommonSemantics.
 *
 * See Transformation for discussion of Transformation
 *
 * @sa Transformation
 * @sa ironbeepp
 * @sa ib_transformation_t
 * @nosubgrouping
 **/
class ConstTransformation :
    public CommonSemantics<ConstTransformation>
{
public:
    //! C Type.
    typedef const ib_transformation_t* ib_type;

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
     * @param[in] engine      Engine to lookup in.
     * @param[in] name        Name of transformation.
     * @param[in] name_length Length of @a name.
     * @returns Transformation.
     **/
    static
    ConstTransformation lookup(
        Engine      engine,
        const char *name,
        size_t      name_length
    );

    /**
     * Lookup transformation in an engine.
     *
     * @param[in] engine      Engine to lookup in.
     * @param[in] name        Name of transformation.
     * @returns Transformation.
     **/
    static
    ConstTransformation lookup(
        Engine             engine,
        const std::string& name
    );

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! const ib_transformation_t accessor.
    // Intentionally inlined.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct Transformation from ib_transformation_t.
    explicit
    ConstTransformation(ib_type ib_transformation);

    ///@}

    //! Name of transformation.
    const char* name() const;

    //! Does the transformation handle lists.
    bool handle_list() const;

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
 * Transformation; equivalent to a pointer to ib_transformation_t.
 *
 * Transformation can be treated as ConstTransformation.  See @ref ironbeepp
 * for details on IronBee++ object semantics.
 *
 * A transformation represents a manipulation of data.
 *
 * @sa ConstTransformation
 * @sa ironbeepp
 * @sa ib_transformation_t
 * @nosubgrouping
 **/
class Transformation :
    public ConstTransformation
{
public:
    //! C Type.
    typedef ib_transformation_t* ib_type;

    /**
     * Remove the constness of a ConstTransformation.
     *
     * @warning This is as dangerous as a @c const_cast, use carefully.
     *
     * @param[in] transformation ConstTransformation to remove const from.
     * @returns Transformation pointing to same underlying transformation as @a transformation.
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
     * Create from 0-3 functionals.
     *
     * @tparam InstanceData Type of data used by instances.
     * @param[in] memory_manager Memory manager to allocate memory from.
     * @param[in] name Name of transformation.
     * @param[in] handle_list Handle lists.
     * @param[in] create Functional to call on creation.  Passed memory
     *                   manager and parameters and should return a pointer
     *                   to InstanceData.  If singular, defaults to nop.
     * @param[in] destroy Functional to call on destruction.  Passed instance
     *                    data the create functional returned.  Defaults to
     *                    nop.
     * @param[in] execute Functional to call on execution. Passed memory
     *                    manager, input, and instance data.  Should return
     *                    output.  Cannot be singular.
     * @returns New transformation.
     **/
    template <typename InstanceData>
    static Transformation create(
        MemoryManager memory_manager,
        const char*   name,
        bool          handle_list,
        boost::function<InstanceData*(MemoryManager, const char*)>
            create,
        boost::function<void(InstanceData*)>
            destroy,
        boost::function<ConstField(MemoryManager, ConstField, InstanceData*)>
            execute
    );

    /**
     * Transformation as functional.
     *
     * A functional to call as an transformation.  Generated by a
     * @ref transformation_generator_t.  See non-templated create().
     *
     * Parameters are memory manager and input field.
     * Return value is result.
     **/
    typedef boost::function<
        ConstField(
            MemoryManager,
            ConstField
        )
    > transformation_instance_t;

    /**
     * Transformation as transformation instance generator.
     *
     * A functional to call to generate an @ref transformation_instance_t.
     * See non-templated create().
     *
     * Parameters are memory manager, and parameters.
     * Return value is an @ref transformation_instance_t.
     **/
    typedef boost::function<
        transformation_instance_t(
            MemoryManager,
            const char*
        )
    > transformation_generator_t;

    /**
     * Create transformation from a single generator functional.
     *
     * @param[in] memory_manager Memory manager to allocate memory from.
     * @param[in] name Name of transformation.
     * @param[in] handle_list Handle lists.
     * @param[in] generator Functional to call when a new instance is needed.
     *                      Is passed context and parameters and should return
     *                      a new functional that will be called with
     *                      transaction, input, and capture on instance
     *                      execution.
     * @returns New transformation.
     **/
    static
    Transformation create(
        MemoryManager              memory_manager,
        const char*                name,
        bool                       handle_list,
        transformation_generator_t generator
    );

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! ib_transformation_t accessor.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct Transformation from ib_transformation_t.
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
 * @param[in] transformation Transformation to output.
 * @return @a o
 **/
std::ostream& operator<<(std::ostream& o, const ConstTransformation& transformation);

/**
 * Const TransformationInstance; equivalent to a const pointer to ib_transformation_inst_t.
 *
 * Provides transformations ==, !=, <, >, <=, >= and evaluation as a boolean for
 * singularity via CommonSemantics.
 *
 * See TransformationInstance for discussion of TransformationInstance
 *
 * @sa TransformationInstance
 * @sa ironbeepp
 * @sa ib_transformation_inst_t
 * @nosubgrouping
 **/
class ConstTransformationInstance :
    public CommonSemantics<ConstTransformationInstance>
{
public:
    //! C Type.
    typedef const ib_transformation_inst_t* ib_type;

    /**
     * Construct singular ConstTransformationInstance.
     *
     * All behavior of a singular ConstTransformationInstance is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    ConstTransformationInstance();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! const ib_transformation_inst_t accessor.
    // Intentionally inlined.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct TransformationInstance from ib_transformation_inst_t.
    explicit
    ConstTransformationInstance(ib_type ib_transformation_instance);

    ///@}

    //! Transformation accessor.
    ConstTransformation transformation() const;

    //! Parameters accessor.
    const char* parameters() const;

    //! Data accessor.
    void* data() const;

    /**
     * Execute transformation instance.
     *
     * @param[in] mm    Memory Manager.
     * @param[in] input Input.
     *
     * @return Modified @a input.
     **/
    ConstField execute(
        MemoryManager mm,
        ConstField    input
    ) const;

private:
    ib_type m_ib;
};

/**
 * TransformationInstance; equivalent to a pointer to ib_transformation_inst_t.
 *
 * TransformationInstance can be treated as ConstTransformationInstance.  See @ref ironbeepp for
 * details on IronBee++ object semantics.
 *
 * An transformation instance is an instantiation of an Transformation for a particular
 * context and set of parameters.
 *
 * @sa ConstTransformationInstance
 * @sa ironbeepp
 * @sa ib_transformation_inst_t
 * @nosubgrouping
 **/
class TransformationInstance :
    public ConstTransformationInstance
{
public:
    //! C Type.
    typedef ib_transformation_inst_t* ib_type;

    /**
     * Remove the constness of a ConstTransformationInstance.
     *
     * @warning This is as dangerous as a @c const_cast, use carefully.
     *
     * @param[in] transformation_instance ConstTransformationInstance to remove const from.
     * @returns TransformationInstance pointing to same underlying transformation instance as @a transformation_instance.
     **/
    static TransformationInstance remove_const(ConstTransformationInstance transformation_instance);

    /**
     * Construct singular TransformationInstance.
     *
     * All behavior of a singular TransformationInstance is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    TransformationInstance();

    /**
     * Create an transformation instance.
     *
     * @param[in] memory_manager        Memory manager to determine lifetime.
     * @param[in] transformation        Transformation.
     * @param[in] parameters            Parameters.
     *
     * @return Transformation instance for @a op.
     **/
    static
    TransformationInstance create(
        MemoryManager       memory_manager,
        ConstTransformation transformation,
        const char*         parameters
    );

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! ib_transformation_inst_t accessor.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct TransformationInstance from ib_transformation_inst_t.
    explicit
    TransformationInstance(ib_type ib_transformation_instance);

    ///@}

private:
    ib_type m_ib;
};

/**
 * Output transformation for TransformationInstance.
 *
 * Output IronBee::TransformationInstance[@e value] where @e value is the name
 * and parameters.
 *
 * @param[in] o Ostream to output to.
 * @param[in] transformation_instance TransformationInstance to output.
 * @return @a o
 **/
std::ostream& operator<<(std::ostream& o, const ConstTransformationInstance& transformation_instance);

// Implementation

namespace Impl {

struct transformation_create_data_t
{
    std::pair<ib_transformation_create_fn_t,  void*> create_trampoline;
    std::pair<ib_transformation_execute_fn_t, void*> execute_trampoline;
    std::pair<ib_transformation_destroy_fn_t, void*> destroy_trampoline;
};

void transformation_cleanup(transformation_create_data_t data);

namespace {

template <typename InstanceData>
ib_status_t transformation_create_translator(
    boost::function<InstanceData*(MemoryManager, const char*)>
        create,
    ib_mm_t       ib_memory_manager,
    const char*   parameters,
    void*         instance_data
)
{
    MemoryManager memory_manager(ib_memory_manager);

    try {
        *(void **)instance_data = value_to_data(
            create(memory_manager, parameters),
            memory_manager.ib()
        );
    }
    catch (...) {
        return convert_exception();
    }
    return IB_OK;
}

template <typename InstanceData>
ib_status_t transformation_execute_translator(
    boost::function<ConstField(MemoryManager, ConstField, InstanceData*)>
        execute,
    ib_mm_t            ib_mm,
    const ib_field_t*  ib_input,
    const ib_field_t** ib_result,
    void*              raw_instance_data
)
{
    ConstField result;
    try {
        result = execute(
            MemoryManager(ib_mm),
            ConstField(ib_input),
            raw_instance_data ?
                data_to_value<InstanceData*>(raw_instance_data) : NULL
        );
    }
    catch (...) {
        return convert_exception();
    }

    if (ib_result != NULL) {
        *ib_result = result.ib();
    }

    return IB_OK;
}

template <typename InstanceData>
void transformation_destroy_translator(
    boost::function<void(InstanceData*)> destroy,
    void* raw_instance_data
)
{
    destroy(data_to_value<InstanceData*>(raw_instance_data));
}

}
} // Impl

template <typename InstanceData>
Transformation Transformation::create(
    MemoryManager memory_manager,
    const char*   name,
    bool          handle_list,
    boost::function<InstanceData*(MemoryManager, const char*)>
        create,
    boost::function<void(InstanceData*)>
        destroy,
    boost::function<ConstField(MemoryManager, ConstField, InstanceData*)>
        execute
)
{
    Impl::transformation_create_data_t data;

    if (create) {
        data.create_trampoline = make_c_trampoline<
            ib_status_t(ib_mm_t, const char *, void *)
        >(
            boost::bind(
                &Impl::transformation_create_translator<InstanceData>,
                create, _1, _2, _3
            )
        );
    }
    if (execute) {
        data.execute_trampoline = make_c_trampoline<
            ib_status_t(ib_mm_t, const ib_field_t*, const ib_field_t**, void*)
        >(
            boost::bind(
                &Impl::transformation_execute_translator<InstanceData>,
                execute, _1, _2, _3, _4
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

    ib_transformation_t* tfn;
    throw_if_error(
        ib_transformation_create(
            &tfn,
            memory_manager.ib(),
            name,
            handle_list,
            data.create_trampoline.first,  data.create_trampoline.second,
            data.destroy_trampoline.first, data.destroy_trampoline.second,
            data.execute_trampoline.first, data.execute_trampoline.second
        )
    );

    memory_manager.register_cleanup(
        boost::bind(&Impl::transformation_cleanup, data)
    );

    return Transformation(tfn);
}

} // IronBee

#endif
