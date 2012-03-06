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
#include <boost/shared_ptr.hpp>
#include <boost/operators.hpp>

#include <ostream>

#ifdef IBPP_EXPOSE_C
struct ib_module_t;
#endif

namespace IronBee {

namespace Internal {
/// @cond Internal

struct ModuleData;

/// @endcond
};

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
     * - An Engine parameter is not provided.  Instead, use Module::engine().
     * - Parameters are passed by copy as they are references to underlying
     *   C objects.  See Module and Context.
     * - Any exceptions thrown will be translated to log messages and
     *   appropriate status returns.  See exception.hpp.
     * - As boost::function's, these are compatible with any function pointer
     *   or function object that matches the signature.
     */
    //@{
    //! Called on module initialization.
    typedef boost::function<void (Module)>          initialize_t;
     //! Called on module finalization.
    typedef boost::function<void (Module)>          finalize_t;
    //! Called on context open.
    typedef boost::function<void (Module, Context)> context_open_t;
    //! Called on context close.
    typedef boost::function<void (Module, Context)> context_close_t;
    //! Called on context destroy.
    typedef boost::function<void (Module, Context)> context_destroy_t;

    //! Set initialization function.
    void set_initialize(initialize_t f);
    //! Set finalize function.
    void set_finalize(finalize_t f);
    //! Set context open function.
    void set_context_open(context_open_t f);
    //! Set context close function.
    void set_context_close(context_close_t f);
    //! Set context destroy function.
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

#ifdef IBPP_EXPOSE_C
    /**
     * @name Expose C
     * Methods to access underlying C types.
     *
     * These methods are only available if IBPP_EXPOSE_C is defined.  This is
     * to avoid polluting the global namespace if they are not needed.
     **/
    ///@{
    //! Non-const ib_module_t accessor.
    ib_module_t*       ib();
    //! Const ib_module_t accessor.
    const ib_module_t* ib() const;
    //! Construct Module from ib_module_t.
    explicit
    Module(ib_module_t* ib_module);
    ///@}
#endif

private:
    typedef boost::shared_ptr<Internal::ModuleData> data_t;

    data_t m_data;
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
