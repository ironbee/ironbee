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

#include <ironbee/engine.h>

namespace IronBee {

class ConfigurationDirectivesRegistrar;

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
     * Events in the engine state machine.
     *
     * This enum defines constants representing the states of the engine
     * state machine.  The main use to module writers is that they are passed
     * in to hook callbacks.
     **/
    enum state_event_e {
        connection_started         = conn_started_event,
        connection_finished        = conn_finished_event,
        connection_opened          = conn_opened_event,
        connection_data_in         = conn_data_in_event,
        connection_data_out        = conn_data_out_event,
        connection_closed          = conn_closed_event,
        transaction_started        = tx_started_event,
        transaction_process        = tx_process_event,
        transaction_finished       = tx_finished_event,
        transaction_data_in        = tx_data_in_event,
        transaction_data_out       = tx_data_out_event,
        handle_context_connection  = handle_context_conn_event,
        handle_connect             = handle_connect_event,
        handle_context_transaction = handle_context_tx_event,
        handle_request_headers     = handle_request_headers_event,
        handle_request             = handle_request_event,
        handle_response_headers    = handle_response_headers_event,
        handle_response            = handle_response_event,
        handle_disconnect          = handle_disconnect_event,
        handle_postprocess         = handle_postprocess_event,
        configuration_started      = cfg_started_event,
        configuration_finished     = cfg_finished_event,
        request_started            = request_started_event,
        request_headers            = request_headers_event,
        request_headers_data       = request_headers_data_event,
        request_body_data          = request_body_data_event,
        request_finished           = request_finished_event,
        response_started           = response_started_event,
        response_headers           = response_headers_event,
        response_headers_data      = response_headers_data_event,
        response_body_data         = response_body_data_event,
        response_finished          = response_finished_event
    };

    /**
     * Provides human readable version of @a event.
     *
     * @param[in] event Event.
     * @returns Human readable string name of @a event.
     **/
    static const char* state_event_name(state_event_e event);

    //! C Type.
    typedef const ib_engine_t* ib_type;

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
    //! C Type.
    typedef ib_engine_t* ib_type;

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

    /**
     * Register configuration directives.
     *
     * This method returns a ConfigurationDirectivesRegistrar, a helper class
     * to assist registering configuration directives.  See
     * ConfigurationDirectivesRegistrar for details on how to use it.
     *
     * @return ConfigurationDirectivesRegistrar
     **/
    ConfigurationDirectivesRegistrar
         register_configuration_directives() const;

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
