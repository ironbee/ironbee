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
 * @brief IronBee++ &mdash; Engine
 *
 * This code is under construction.  Do not use yet.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP__ENGINE__
#define __IBPP__ENGINE__

#include <ironbeepp/common_semantics.hpp>
#include <iostream>

// IronBee C
typedef struct ib_engine_t ib_engine_t;

namespace IronBee {

/**
 * Const Engine; equivalent to a const pointer to ib_engine_t.
 *
 * Provides operators ==, !=, <, >, <=, >= and evaluation as a boolean for
 * singularity via CommonSemantics.
 *
 * See Engine for discussion of the engine.
 *
 * @sa Engine
 * @sa ironbeepp
 * @sa ib_engine_t
 * @nosubgrouping
 **/
class ConstEngine :
    public CommonSemantics<ConstEngine>
{
public:
    /**
     * Construct singular ConstEngine.
     *
     * All behavior of a singular ConstEngine is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    ConstEngine();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! const ib_engine_t accessor.
    // Intentionally inlined.
    const ib_engine_t* ib() const
    {
        return m_ib;
    }

    //! Construct Engine from ib_engine_t.
    explicit
    ConstEngine(const ib_engine_t* ib_engine);

    ///@}

private:
    const ib_engine_t* m_ib;
};

/**
 * Engine; equivalent to a pointer to ib_engine_t.
 *
 * An Engine can be treated as a ConstEngine.  See @ref ironbeepp for
 * details on IronBee++ object semantics.
 *
 * The IronBee Engine is the central component of IronBee that processes
 * inputs and calls hooks.  It is a complex state machine.  See
 * IronBeeEngineState.
 *
 * This class provides some of the C API functionality.  In particular, it
 * allows module writers to register hooks with the engine and provides
 * logging functionality.
 *
 * @sa ironbeepp
 * @sa IronBeeEngineState
 * @sa ib_engine_t
 * @sa ConstEngine
 * @nosubgrouping
 **/
class Engine :
    public ConstEngine
{
public:
    /**
     * Remove the constness of a ConstEngine.
     *
     * @warning This is as dangerous as a @c const_cast, use carefully.
     *
     * @param[in] engine ConstEngine to remove const from.
     * @returns Engine pointing to same underlying byte string as @a bs.
     **/
    static Engine remove_const(ConstEngine engine);

    /**
     * Construct singular Engine.
     *
     * All behavior of a singular Engine is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    Engine();

    /**
     * @name Hooks
     * Methods to register hooks.
     *
     * See IronBeeEngineState for details on the states and transitions.
     **/
    ///@{

    ///@}

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! const ib_engine_t accessor.
    // Intentionally inlined.
    ib_engine_t* ib() const
    {
        return m_ib;
    }

    //! Construct Engine from ib_engine_t.
    explicit
    Engine(ib_engine_t* ib_engine);

    ///@}

private:
    ib_engine_t* m_ib;
};

/**
 * Output operator for Engine.
 *
 * Outputs Engine[@e value] to @a o where @e value is replaced with
 * the value of the bytestring.
 *
 * @param[in] o      Ostream to output to.
 * @param[in] engine Engine to output.
 * @return @a o
 **/
std::ostream& operator<<(std::ostream& o, const ConstEngine& engine);

} // IronBee

#endif
