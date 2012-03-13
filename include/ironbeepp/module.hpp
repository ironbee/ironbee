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
 * @brief IronBee++ &mdash; Module
 *
 * This file defines Module, a wrapper for ib_module_t.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP__MODULE__
#define __IBPP__MODULE__

#include <ironbeepp/exception.hpp>
#include <ironbeepp/engine.hpp>
#include <ironbeepp/memory_pool.hpp>
#include <ironbeepp/internal/throw.hpp>
#include <ironbeepp/internal/data.hpp>

#include <ironbee/module.h>
#include <ironbee/mpool.h>

#include <boost/function.hpp>
#include <boost/operators.hpp>

#include <ostream>
#include <cassert>

namespace IronBee {

class Context;

/**
 * Module information; equivalent to a pointer to ib_module_t.
 *
 * An IronBee Module adds functionality to IronBee.  This class represents
 * the information each module provides to the engine.
 *
 * This class behaves similar to @c ib_module_t*.  In particular, it can
 * be singular (equivalent to NULL).  See object semantics in @ref ironbeepp.
 *
 * If you are interested in writing a module in C++, see module_bootstrap.hpp.
 *
 * @sa ironbeepp
 * @sa module_bootstrap.hpp
 * @sa ib_module_t
 * @nosubgrouping
 **/
class Module :
    boost::less_than_comparable<Module>,
    boost::equality_comparable<Module>
{
public:
    //! Construct singular module.
    Module();

    //! Associated Engine.
    Engine engine() const;

    //! Version number.
    uint32_t version_number() const;

    //! ABI number.
    uint32_t abi_number() const;

    //! Version string.
    const char* version() const;

    //! Filename.
    const char* filename() const;

    //! Index.
    size_t index() const;

    //! Name.
    const char* name() const;

    /**
     * @name Callbacks
     * Types and methods associated with callbacks.
     *
     * Callbacks can be chained.  Usually all callbacks in the chain are
     * called, but if any throws an exception it will abort the chain.  When
     * possible, prefer the @c chain_* methods.  Other IronBee++ components
     * will prechain callbacks to handle their own callbacks.
     *
     * All @c set_* callbacks can be passed an empty functional, e.g.,
     * @c module_callback_t(), to remove the callback (and any chained)
     * callbacks.
     *
     * @warning The @c set_* callbacks will clear all other chained callbacks.
     *          Only do this if you are sure of what you are doing.
     *
     * - An Engine parameter is not provided.  Instead, use Module::engine().
     * - Parameters are passed by copy as they are references to underlying
     *   C objects.  See Module and Context.
     * - Any exceptions thrown will be translated to log messages and
     *   appropriate status returns.  See exception.hpp.
     * - As boost::function's, these are compatible with any function pointer
     *   or function object that matches the signature.
     **/
    ///@{

    //! Module callback.
    typedef boost::function<void (Module)> module_callback_t;
    //! Context callback.
    typedef boost::function<void (Module, Context)> context_callback_t;
    //! Called on module initialization.
    typedef module_callback_t initialize_t;
     //! Called on module finalization.
    typedef module_callback_t finalize_t;
    //! Called on context open.
    typedef context_callback_t context_open_t;
    //! Called on context close.
    typedef context_callback_t context_close_t;
    //! Called on context destroy.
    typedef context_callback_t context_destroy_t;

    //! Chain initialization function.
    void chain_initialize(initialize_t f);
    //! Chain finalize function.
    void chain_finalize(finalize_t f);
    //! Chain context open function.
    void chain_context_open(context_open_t f);
    //! Chain context close function.
    void chain_context_close(context_close_t f);
    //! Chain context destroy function.
    void chain_context_destroy(context_destroy_t f);

    //! Prechain initialization function.  Prefer chain_initialize().
    void prechain_initialize(initialize_t f);
    //! Prechain finalize function.  Prefer chain_finalize().
    void prechain_finalize(finalize_t f);
    //! Prechain context open function.  Prefer chain_context_open().
    void prechain_context_open(context_open_t f);
    //! Prechain context close function.  Prefer chain_context_close().
    void prechain_context_close(context_close_t f);
    //! Prechain context destroy function.  Prefer chain_context_destroy().
    void prechain_context_destroy(context_destroy_t f);

    //! Set initialization function.  Prefer chain_initialize().
    void set_initialize(initialize_t f);
    //! Set finalize function.  Prefer chain_finalize().
    void set_finalize(finalize_t f);
    //! Set context open function.  Prefer chain_context_open().
    void set_context_open(context_open_t f);
    //! Set context close function.  Prefer chain_context_close().
    void set_context_close(context_close_t f);
    //! Set context destroy function.  Prefer chain_context_destroy().
    void set_context_destroy(context_destroy_t f);

    ///@}

    /**
     * @name Configuration Data
     * Types and methods associated with configuration data.
     *
     * A module can have configuration data.  This data is set as the
     * "global" configuration data.  Each context then copies the
     * configuration data of the parent context or the global configuration
     * data if there is no parent context.
     *
     * There are two levels that Module can handle configuration data:
     *
     * At the lowest level is set_configuration_data_pod() which fairly
     * directly matches the C interface.  It requires that configuration data
     * by POD (plain old data; essentially any C type).  POD data is copied
     * either via direct memory copy (default) or a user provided copy
     * functional.
     *
     * At the higher level is set_configuration_data() which uses C++
     * semantics, i.e., copy constructors, destructors, etc.  To accommodate
     * the C interface, it stores a pointer to the type rather than the type
     * in the C interface.
     *
     * It is highly recommended to use set_configuration_data() unless you
     * need to closely interoperate with the C code.
     **/
    ///@{

    /**
     * Metafunction providing type of configuration data copier.
     *
     * Will define @c type to be the the appropriate functional type.
     *
     * @warning @a DataType must be POD.  At present, issues with clang and
     * __is_pod() make it impossible to statically assert podness in a
     * compatible way.  And such, behavior is undefined if @a DataType is not
     * POD.
     *
     * @sa set_configuration_data()
     *
     * @tparam DataType Type of configuration data.
     * @tparam Enable   Internal use only.
     **/
    template <typename DataType>
    struct configuration_copier_t
    {
        //! Type of copier for set_configuration_data().
        typedef boost::function<
            void(Module, DataType&, const DataType&)
        > type;
    };

    /**
     * Set configuration data for POD
     *
     * See group member group documentation for details.
     *
     * For C++ object semantics with configuration data, see
     * set_configuration_data().
     *
     * For type safety, the type of configuration data (@a DataType), global
     * data (@a global_data), and any copier function (@a copier) must be
     * provided simultaneously.
     *
     * This is a low level routine and, as such, requires that @a DataType
     * be POD.  Behavior is undefined if @a DataType is not POD (see
     * configuration_copier_t for discussion).
     *
     * You should strongly consider using set_configuration_data() instead
     * of using this directly.
     *
     * The copier functional must take a Module, a reference to @a DataType
     * specifying the destination and a const reference to @a DataType
     * specifying the source.  No assumptions should be made about the
     * contents of the destination.
     *
     * @warning Only call one of set_configuration_data() and
     * set_configuration_data_pod().  They will overwrite each other.
     *
     * @tparam DataType Type of configuration data.  Must be POD.
     * @param[in] global_data The global configuration data copied to every
     *                        top level context.
     * @param[in] copier      Optional copier functional.  Defaults to
     *                        memory copy (IB engine default).
     **/
    template <typename DataType>
    void set_configuration_data_pod(
        const DataType& global_data,
        typename configuration_copier_t<DataType>::type copier =
            typename configuration_copier_t<DataType>::type()
    );

     /**
      * Set configuration for C++ objects.
      *
      * See group member group documentation for details.
      *
      * Under the hood, this calls set_configuration_data_pod() with a pointer
      * to DataType (pointers are POD).  That is, @c gcdata is a pointer to
      * a pointer to a @a DataType.
      *
      * @warning Only call one of set_configuration_data() and
      * set_configuration_data_pod().  They will overwrite each other.
      *
      * @tparam DataType Type of configuration data.  Must be copyable.
      * @param[in] global_data The global configuration data copied to
      *                        every toplevel context.
      **/
    template <typename DataType>
    void set_configuration_data(
        const DataType& global_data
    );

    ///@}

    /// @cond Internal
    typedef void (*unspecified_bool_type)(Module***);
    /// @endcond
    /**
     * Is not singular?
     *
     * This operator returns a type that converts to bool in appropriate
     * circumstances and is true iff this object is not singular.
     *
     * @returns true iff is not singular.
     **/
    operator unspecified_bool_type() const;

    /**
     * Equality operator.  Do they refer to the same underlying module.
     *
     * Two Modules are considered equal if they refer to the same underlying
     * ib_module_t.
     *
     * @param[in] other Module to compare to.
     * @return true iff @c other.ib() == ib().
     **/
    bool operator==(const Module& other) const;

    /**
     * Less than operator.
     *
     * Modules are totally ordered with all singular Modules as the minimal
     * element.
     *
     * @param[in] other Module to compare to.
     * @return true iff this and other are singular or  ib() < @c other.ib().
     **/
    bool operator<(const Module& other) const;

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! Non-const ib_module_t accessor.
    // Intentionally inlined.
    ib_module_t* ib()
    {
        return m_ib;
    }

    //! Const ib_module_t accessor.
    // Intentionally inlined.
    const ib_module_t* ib() const
    {
        return m_ib;
    }

    //! Construct Module from ib_module_t.
    explicit
    Module(ib_module_t* ib_module);

    ///@}

private:
    ib_module_t* m_ib;

    // Used for unspecified_bool_type.
    static void unspecified_bool(Module***) {};

    // Translator for configuration copier.
    typedef boost::function<
        void(ib_module_t*, void*, const void*, size_t)
    > configuration_copier_translator_t;

    void set_configuration_copier_translator(
        configuration_copier_translator_t f
    );
};

namespace Internal {
/// @cond Internal

/**
 * Helper functional to translated untyped interface to typed.
 *
 * Used by set_configuration_data_pod().
 *
 * @tparam DataType DataType of configuration data.
 **/
template <typename DataType>
class configuration_copier_translator
{
public:
    /**
     * Default constructor.
     **/
    configuration_copier_translator()
    {
        // nop
    }

    /**
     * Constructor.
     *
     * @param[in] copier Typed copier to forward to.
     **/
    explicit
    configuration_copier_translator(
        typename Module::configuration_copier_t<DataType>::type copier
    ) :
        m_copier( copier )
    {
        // nop
    }

    /**
     * Call operator.
     *
     * Converts arguments to appropriate C++ types and calls copier.
     **/
    void operator()(
        ib_module_t* ib_module,
        void*        dst,
        const void*  src,
        size_t       length
    ) const
    {
        assert(length == sizeof(DataType));
        m_copier(
            Module(ib_module),
            *reinterpret_cast<DataType*>(dst),
            *reinterpret_cast<const DataType*>(src)
        );
    }

private:
    typename Module::configuration_copier_t<DataType>::type m_copier;
};

/**
 * Functional to delete a given object.
 *
 * Used by set_configuration_data().
 *
 * @tparam DataType Type of configuration data.
 **/
template <typename DataType>
class configuration_data_destroy
{
public:
    /**
     * Default constructor.
     **/
    configuration_data_destroy() :
        m_p(NULL)
    {
        // nop
    }

    /**
     * Constructor.
     *
     * @param[in] p Pointer to object to destroy.
     **/
    explicit
    configuration_data_destroy(DataType* p) :
        m_p(p)
    {
        // nop
    }

    /**
     * Destroy object.
     *
     * Does nothing if default constructed.
     **/
    void operator()()
    {
        if (m_p) {
            delete m_p;
        }
    }

private:
    DataType* m_p;
};

/**
 * Functional to copy configuration data when C++.
 *
 * Used by set_configuration_data().
 *
 * Used copy constructor of @a DataType.
 *
 * @tparam DataType Type of configuration data.  Note: C++ type, not POD
 *         type.  The POD type is @c DataType*.
 **/
template <typename DataType>
class configuration_data_copy
{
public:
    /**
     * Create copy of @c *src and store in @c *dst.
     *
     * Also ensures that copy is properly destroyed.
     *
     * @param[in]  module Module involved.
     * @param[out] dst    Where to store pointer to copy.
     * @param[in]  src    Pointer to source.
     **/
    void operator()(
        Module           module,
        DataType*&       dst,
        DataType* const& src
    ) const
    {
        dst = new DataType(*src);
        MemoryPool(
            ib_engine_pool_main_get(module.engine().ib())
        ).register_cleanup(
            configuration_data_destroy<DataType>(dst)
        );
    }
};

/// @endcond
} // Internal

template <typename DataType>
void Module::set_configuration_data_pod(
    const DataType& global_data,
    typename configuration_copier_t<DataType>::type copier
)
{
    ib()->gclen = sizeof(global_data);
    ib()->gcdata = ib_mpool_alloc(
        ib_engine_pool_main_get(ib()->ib),
        ib()->gclen
    );
    if (ib()->gcdata == NULL) {
        BOOST_THROW_EXCEPTION(
          ealloc() << errinfo_what(
            "Could not allocate memory for configuration data."
          )
        );
    }
    *reinterpret_cast<DataType*>(ib()->gcdata) = global_data;

    if (copier.empty()) {
        ib()->fn_cfg_copy     = NULL;
        ib()->cbdata_cfg_copy = NULL;
    } else {
        set_configuration_copier_translator(
            Internal::configuration_copier_translator<DataType>(copier)
        );
    }
}

template <typename DataType>
void Module::set_configuration_data(
    const DataType& global_data
)
{
    DataType* global_data_ptr = new DataType(global_data);

    // This will make gcdata a pointer to a pointer.
    set_configuration_data_pod(
        global_data_ptr,
        Internal::configuration_data_copy<DataType>()
    );

    MemoryPool(
        ib_engine_pool_main_get(engine().ib())
    ).register_cleanup(
        Internal::configuration_data_destroy<DataType>(global_data_ptr)
    );
}

/**
 * Output operator for Module.
 *
 * Outputs Module[@e name] to @a o where @e name is replaced with
 * @a module.@c name().
 *
 * @param[in] o      Ostream to output to.
 * @param[in] module Module to output.
 * @return @a o
 **/
std::ostream& operator<<(std::ostream& o, const Module& module);

} // IronBee

#endif
