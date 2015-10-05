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
 * @brief IronBee++ --- Context
 *
 * This file defines (Const)Context, a wrapper for ib_context_t.
 *
 * @remark Developers should be familiar with @ref ironbeepp to understand
 * aspects of this code, e.g., the public/non-virtual inheritance.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP__CONTEXT__
#define __IBPP__CONTEXT__

#include <ironbeepp/abi_compatibility.hpp>
#include <ironbeepp/common_semantics.hpp>
#include <ironbee/context.h>
#include <ironbeepp/memory_manager.hpp>

#include <ostream>

// IronBee C Type
typedef struct ib_context_t ib_context_t;

namespace IronBee {

class ConstSite;
class Site;
class Engine;
class Context;

/**
 * Const Context; equivalent to a const pointer to ib_context_t.
 *
 * Provides operators ==, !=, <, >, <=, >= and evaluation as a boolean for
 * singularity via CommonSemantics.
 *
 * See Context for discussion of Context
 *
 * @sa Context
 * @sa ironbeepp
 * @sa ib_context_t
 * @nosubgrouping
 **/
class ConstContext :
    public CommonSemantics<ConstContext>
{
public:
    //! C Type.
    typedef const ib_context_t* ib_type;

    /**
     * Construct singular ConstContext.
     *
     * All behavior of a singular ConstContext is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    ConstContext();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! const ib_context_t accessor.
    // Intentionally inlined.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct Context from ib_context_t.
    explicit
    ConstContext(ib_type ib_context);

    ///@}

    //! Type of context.
    const char* type() const;

    //! Name of context.
    const char* name() const;

    //! Full name of context.
    const char* full_name() const;

    //! Parent context; singular if none.
    Context parent() const;

    //! Engine context is associated with.
    Engine engine() const;

    //! Site of context.
    ConstSite site() const;

private:
    ib_type m_ib;
};

/**
 * Context; equivalent to a pointer to ib_context_t.
 *
 * Context can be treated as ConstContext.  See @ref ironbeepp for
 * details on IronBee++ object semantics.
 *
 * Placeholder for future functionality.
 *
 * @sa ConstContext
 * @sa ironbeepp
 * @sa ib_context_t
 * @nosubgrouping
 **/
class Context :
    public ConstContext
{
public:
    //! C Type.
    typedef ib_context_t* ib_type;

    /**
     * Remove the constness of a ConstContext.
     *
     * @warning This is as dangerous as a @c const_cast, use carefully.
     *
     * @param[in] context ConstContext to remove const from.
     * @returns Context pointing to same underlying context as @a context.
     **/
    static Context remove_const(ConstContext context);

    /**
     * Construct singular Context.
     *
     * All behavior of a singular Context is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    Context();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! ib_context_t accessor.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Memory manager accessor.
    MemoryManager memory_manager();

    //! Construct Context from ib_context_t.
    explicit
    Context(ib_type ib_context);

    ///@}

private:
    ib_type m_ib;
};

/**
 * Output operator for Context.
 *
 * Output IronBee::Context[@e value] where @e value is the full name.
 *
 * @param[in] o Ostream to output to.
 * @param[in] context Context to output.
 * @return @a o
 **/
std::ostream& operator<<(std::ostream& o, const ConstContext& context);

} // IronBee

#endif
