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

#include <boost/function.hpp>
#include <boost/operators.hpp>

#include <ostream>

// IronBee C
typedef struct ib_module_t ib_module_t;

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
     */
    //@{
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
    //@}

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
     * @return true iff other.ib() == ib().
     **/
    bool operator==(const Module& other) const;

    /**
     * Less than operator.
     *
     * Modules are totally ordered with all singular Modules as the minimal
     * element.
     *
     * @param[in] other Module to compare to.
     * @return true iff this and other are singular or  ib() < other.ib().
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
};

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
