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
 * @brief IronBee++ --- Module
 *
 * This file defines Module, a wrapper for ib_module_t.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP__MODULE__
#define __IBPP__MODULE__


#include <ironbeepp/abi_compatibility.hpp>
#include <ironbeepp/common_semantics.hpp>
#include <ironbeepp/configuration_map.hpp>
#include <ironbeepp/context.hpp>
#include <ironbeepp/data.hpp>
#include <ironbeepp/engine.hpp>
#include <ironbeepp/exception.hpp>
#include <ironbeepp/memory_manager.hpp>
#include <ironbeepp/throw.hpp>

#include <ironbee/mm.h>
#include <ironbee/module.h>

#include <boost/function.hpp>

#include <ostream>
#include <cassert>

namespace IronBee {

class Module;

/**
 * Const Module; equivalent to a const pointer to ib_module_t
 *
 * Provides operators ==, !=, <, >, <=, >= and evaluation as a boolean for
 * singularity via CommonSemantics.
 *
 * See Module for discussion of modules
 *
 * @sa Module
 * @sa ironbeepp
 * @sa ib_module_t
 * @nosubgrouping
 **/
class ConstModule :
    public CommonSemantics<ConstModule>
{
public:
    //! C Type.
    typedef const ib_module_t* ib_type;

    //! Construct singular ConstModule.
    ConstModule();

    /**
     * @name Queries
     * Query aspects of the module.
     **/
    ///@{

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

    ///@}

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! Const ib_module_t accessor.
    // Intentionally inlined.
    const ib_module_t* ib() const
    {
        return m_ib;
    }

    explicit
    ConstModule(const ib_module_t* ib_module);

    ///@}

private:
    const ib_module_t* m_ib;
};

/**
 * Module information; equivalent to a pointer to ib_module_t.
 *
 * Modules can be treated as ConstModules.  See @ref ironbeepp for details on
 * IronBee++ object semantics.
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
 * @sa ConstModule
 * @nosubgrouping
 **/
class Module :
    public ConstModule // Slicing is intentional; see apidoc.hpp
{
public:
    //! C Type.
    typedef ib_module_t* ib_type;

   /**
    * Remove the constness of a ConstModule
    *
    * @warning This is as dangerous as a @c const_cast, use carefully.
    *
    * @param[in] const_module ConstModule to remove const from.
    * @returns Module pointing to same underlying module as @a const_module.
    **/
    static Module remove_const(const ConstModule& const_module);

    //! Construct singular module.
    Module();

    /**
     * Get existing module by name.
     *
     * @param[in] engine Engine to get module from.
     * @param[in] name   Name of module.
     * @returns Module with name @a name.
     **/
    static Module with_name(Engine engine, const char* name);

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
     *   C objects.  See Module.
     * - Any exceptions thrown will be translated to log messages and
     *   appropriate status returns.  See exception.hpp.
     * - As boost::function's, these are compatible with any function pointer
     *   or function object that matches the signature.
     **/
    ///@{

    //! Module callback.
    typedef boost::function<void (Module)> module_callback_t;
    //! Called on module initialization.
    typedef module_callback_t initialize_t;
     //! Called on module finalization.
    typedef module_callback_t finalize_t;

    //! Chain initialization function.
    void chain_initialize(initialize_t f) const;
    //! Chain finalize function.
    void chain_finalize(finalize_t f) const;

    //! Prechain initialization function.  Prefer chain_initialize().
    void prechain_initialize(initialize_t f) const;
    //! Prechain finalize function.  Prefer chain_finalize().
    void prechain_finalize(finalize_t f) const;

    //! Set initialization function.  Prefer chain_initialize().
    void set_initialize(initialize_t f) const;
    //! Set finalize function.  Prefer chain_finalize().
    void set_finalize(finalize_t f) const;

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
     *
     * Both methods returns a ConfigurationMapInit object to facilitate
     * creation of configuration maps.  See configuration_map.hpp for details.
     * E.g.,
     *
     * @code
     * module.set_configuration_data_pod(my_global_data)
     *       .number("num_foos", &my_global_data_t::num_foos)
     *       .byte_string_s("best_foo_name", &my_global_data_t::best_foo_name)
     *       ;
     * @endcode
     *
     * If the configuration data is not a class/struct, the return value
     * is a valid type (ConfigurationMapInit<DataType>) but has no
     * methods.
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
     * @returns ConfigurationMapInit
     **/
    template <typename DataType>
    ConfigurationMapInit<DataType> set_configuration_data_pod(
        const DataType& global_data,
        typename configuration_copier_t<DataType>::type copier =
            typename configuration_copier_t<DataType>::type()
    ) const;

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
     * @returns ConfigurationMapInit
    **/
    template <typename DataType>
    ConfigurationMapInit<DataType> set_configuration_data(
        const DataType& global_data = DataType()
    ) const;

    /**
     * Fetch POD configuration data for @a context.
     *
     * This method fetches configuration data for @a context assuming
     * set_configuration_data_pod() was used during module setup.
     *
     * @warning Do not use this method if set_configuration_data() was used;
     *          instead, use configuration_data().
     *
     * @a DataType must match the data type used in
     * set_configuration_data_pod().
     *
     * @tparam DataType Type of configuration data.
     * @param[in] context Context to fetch configuration for.
     * @returns Configuration data.
     **/
    template <typename DataType>
    DataType& configuration_data_pod(ConstContext context) const;

    /**
     * Fetch configuration data for @a context.
     *
     * This method fetches configuration data for @a context assuming
     * set_configuration_data() was used during module setup.
     *
     * @warning Do not use this method if set_configuration_data_pod() was
     *          used; instead, use configuration_data_pod().
     *
     * @a DataType must match the data type used in
     * set_configuration_data().
     *
     * @tparam DataType Type of configuration data.
     * @param[in] context Context to fetch configuration for.
     * @returns Configuration data.
     **/
    template <typename DataType>
    DataType& configuration_data(ConstContext context) const;
    ///@}

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! Non-const ib_module_t accessor.
    // Intentionally inlined.
    ib_module_t* ib() const
    {
        return m_ib;
    }

    //! Construct Module from ib_module_t.
    explicit
    Module(ib_module_t* ib_module);

    ///@}

private:
    ib_module_t* m_ib;

    // Translator for configuration copier.
    typedef boost::function<
        void(ib_module_t*, void*, const void*, size_t)
    > configuration_copier_translator_t;

    void set_configuration_copier_translator(
        configuration_copier_translator_t f
    ) const;
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
        MemoryManager(
            ib_engine_mm_main_get(module.engine().ib())
        ).register_cleanup(
            configuration_data_destroy<DataType>(dst)
        );
    }
};

/// @endcond
} // Internal

template <typename DataType>
ConfigurationMapInit<DataType> Module::set_configuration_data_pod(
    const DataType& global_data,
    typename configuration_copier_t<DataType>::type copier
) const
{
    MemoryManager mm(ib_engine_mm_main_get(ib()->ib));
    throw_if_error(
        ib_module_config_initialize(
          ib(),
          mm.alloc(sizeof(global_data)),
          sizeof(global_data)
      )
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
    }
    else {
        set_configuration_copier_translator(
            Internal::configuration_copier_translator<DataType>(copier)
        );
    }

    return ConfigurationMapInit<DataType>(ib()->cm_init, mm);
}

template <typename DataType>
ConfigurationMapInit<DataType> Module::set_configuration_data(
    const DataType& global_data
) const
{
    MemoryManager mm(ib_engine_mm_main_get(ib()->ib));
    DataType* global_data_ptr = new DataType(global_data);

    // This will make gcdata a pointer to a pointer.
    set_configuration_data_pod(
        global_data_ptr,
        Internal::configuration_data_copy<DataType>()
    );

    MemoryManager(
        ib_engine_mm_main_get(engine().ib())
    ).register_cleanup(
        Internal::configuration_data_destroy<DataType>(global_data_ptr)
    );

    return ConfigurationMapInit<DataType>(ib()->cm_init, mm, true);
}

template <typename DataType>
DataType& Module::configuration_data_pod(ConstContext context) const
{
    DataType* config;
    throw_if_error(
        ib_context_module_config(context.ib(), ib(), &config)
    );
    return *config;
}

template <typename DataType>
DataType& Module::configuration_data(ConstContext context) const
{
    DataType* config_ptr = configuration_data_pod<DataType*>(context);
    return *config_ptr;
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
std::ostream& operator<<(std::ostream& o, const ConstModule& module);

} // IronBee

#endif
