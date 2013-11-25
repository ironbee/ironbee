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
 * @brief IronBee++ --- Hooks
 *
 * This file defines HooksRegistrar, a helper class for
 * Engine::register_hooks().
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbeepp/abi_compatibility.hpp>
#include <ironbeepp/engine.hpp>

#include <boost/function.hpp>

#ifndef __IBPP__HOOKS__
#define __IBPP__HOOKS__

namespace IronBee {

class Transaction;
class Connection;
class ParsedHeader;
class ParsedRequestLine;
class ParsedResponseLine;

/**
 * Helper class for Engine::register_hooks().
 *
 * This class is returned by Engine::register_hooks() and can be used to
 * register callback functions for engine state transitions.
 *
 * E.g.,
 * @code
 * engine.register_hooks()
 *       .connection_opened(bind(on_connection, _1, _2, _3, false))
 *       .connection_closed(bind(on_connection, _1, _2, _3, true))
 *       ;
 * @endcode
 *
 * @sa Engine::register_hooks()
 * @sa Engine::state_event_e
 * @nosubgrouping
 **/
class HooksRegistrar
{
public:
    /**
     * Constructor.
     *
     * Usually you will not construct a HooksRegistrar directly.  Instead,
     * call Engine::register_hooks() which returns an appropriate
     * HooksRegistrar.
     *
     * @sa Engine::register_hooks()
     **/
    explicit
    HooksRegistrar(Engine engine);

    /**
     * @name Call back types.
     * Types for hook call backs.
     *
     * Every callback receives an Engine and the state event that triggered
     * the event.  Many events also receive the current transaction.  Some
     * events receive an additional argument.
     *
     * All callbacks should throw an IronBee++ exception on error.
     **/
    ///@{
    /**
     * Call back type that takes no argument.
     *
     * Parameters are:
     * - IronBee engine.
     * - Which event triggered the callback.
     **/
    typedef boost::function<
        void(
            Engine,
            Engine::state_event_e
         )
    > null_t;

    /**
     * Call back type that takes ParsedHeader argument.
     *
     * Parameters are:
     * - IronBee engine.
     * - Current transaction.
     * - Which event triggered the callback.
     * - Argument.
     **/
    typedef boost::function<
        void(
            Engine,
            Transaction,
            Engine::state_event_e,
            ParsedHeader
        )
    > header_data_t;

    /**
     * Call back type that takes RequestLine argument.
     *
     * Parameters are:
     * - IronBee engine.
     * - Current transaction.
     * - Which event triggered the callback.
     * - Argument.
     **/
    typedef boost::function<
        void(
            Engine,
            Transaction,
            Engine::state_event_e,
            ParsedRequestLine
        )
    > request_line_t;

    /**
     * Call back type that takes ResponseLine argument.
     *
     * Parameters are:
     * - IronBee engine.
     * - Current transaction.
     * - Which event triggered the callback.
     * - Argument.
     **/
    typedef boost::function<
        void(
            Engine,
            Transaction,
            Engine::state_event_e,
            ParsedResponseLine
        )
    > response_line_t;

    /**
     * Call back type that takes Connection argument.
     *
     * Parameters are:
     * - IronBee engine.
     * - Which event triggered the callback.
     * - Argument.
     **/
    typedef boost::function<
        void(
            Engine,
            Connection,
            Engine::state_event_e
        )
    > connection_t;

    /**
     * Call back type that takes a Transaction but no argument.
     *
     * Parameters are:
     * - IronBee engine.
     * - Current transaction.
     * - Which event triggered the callback.
     **/
    typedef boost::function<
        void(
            Engine,
            Transaction,
            Engine::state_event_e
        )
    > transaction_t;

    /**
     * Call back type that takes pointer and length arguments.
     *
     * Parameters are:
     * - IronBee engine.
     * - Current transaction.
     * - Which event triggered the callback.
     * - Argument.
     **/
    typedef boost::function<
        void(
            Engine,
            Transaction,
            Engine::state_event_e,
            const char*,
            size_t
        )
    > transaction_data_t;

    /**
     * Call back type that takes a Context argument.
     *
     * Parameters are:
     * - IronBee engine.
     * - Current context.
     * - Which event triggered the callback.
     **/
    typedef boost::function<
        void(
            Engine,
            Context,
            Engine::state_event_e
        )
    > context_t;
    ///@}

    /**
     * @name Generic Registration
     * Register by callback type.
     *
     * There is a generic registration routine for each callback type which
     * takes the event to register the callback for and the functional to
     * register.  An exception is thrown, if the callback type is not correct
     * for the event.  It is recommended that use the Specific Registration
     * methods instead.
     *
     * All methods return @c *this to allow for call chaining.
     **/
    ///@{

    /**
     * Register null callback.
     *
     * @param[in] event Event to register for.
     * @param[in] f     Functional to register.
     * @returns @c *this for call chaining.
     * @throw einval if callback type is not appropriate for @a event.
     **/
    HooksRegistrar& null(
        Engine::state_event_e event,
        null_t                f
     );

    /**
     * Register header data callback.
     *
     * @param[in] event Event to register for.
     * @param[in] f     Functional to register.
     * @returns @c *this for call chaining.
     * @throw einval if callback type is not appropriate for @a event.
     **/
    HooksRegistrar& header_data(
        Engine::state_event_e event,
        header_data_t        f
     );

    /**
     * Register request line callback.
     *
     * @param[in] event Event to register for.
     * @param[in] f     Functional to register.
     * @returns @c *this for call chaining.
     * @throw einval if callback type is not appropriate for @a event.
     **/
    HooksRegistrar& request_line(
        Engine::state_event_e event,
        request_line_t        f
     );

    /**
     * Register response line callback.
     *
     * @param[in] event Event to register for.
     * @param[in] f     Functional to register.
     * @returns @c *this for call chaining.
     * @throw einval if callback type is not appropriate for @a event.
     **/
    HooksRegistrar& response_line(
        Engine::state_event_e event,
        response_line_t       f
     );

    /**
     * Register connection callback.
     *
     * @param[in] event Event to register for.
     * @param[in] f     Functional to register.
     * @returns @c *this for call chaining.
     * @throw einval if callback type is not appropriate for @a event.
     **/
    HooksRegistrar& connection(
        Engine::state_event_e event,
        connection_t          f
     );

    /**
     * Register transaction callback.
     *
     * @param[in] event Event to register for.
     * @param[in] f     Functional to register.
     * @returns @c *this for call chaining.
     * @throw einval if callback type is not appropriate for @a event.
     **/
    HooksRegistrar& transaction(
        Engine::state_event_e event,
        transaction_t         f
     );

    /**
     * Register transaction data callback.
     *
     * @param[in] event Event to register for.
     * @param[in] f     Functional to register.
     * @returns @c *this for call chaining.
     * @throw einval if callback type is not appropriate for @a event.
     **/
    HooksRegistrar& transaction_data(
        Engine::state_event_e event,
        transaction_data_t    f
    );

    /**
     * Register context callback.
     *
     * @param[in] event Event to register for.
     * @param[in] f     Functional to register.
     * @returns @c *this for call chaining.
     * @throw einval if callback type is not appropriate for @a event.
     **/
    HooksRegistrar& context(
        Engine::state_event_e event,
        context_t             f
    );

    ///@}

    /**
     * @name Specific Registration
     * Register by event.
     *
     * There is a method below for every event.  Each method takes a single
     * argument: the callback functional to register.  The method simply
     * calls the appropriate generic registration method above.
     *
     * All methods return @c *this to allow for call chaining.
     **/
    ///@{

    /**
     * Register callback for @ref request_header_data.
     *
     * @sa Engine::state_event_e
     *
     * @param[in] f Callback to register.
     * @returns @c *this for call chaining.
     * @throw IronBee++ exception on failure.
     **/
    HooksRegistrar& request_header_data(header_data_t f);

    /**
     * Register callback for @ref response_header_data.
     *
     * @sa Engine::state_event_e
     *
     * @param[in] f Callback to register.
     * @returns @c *this for call chaining.
     * @throw IronBee++ exception on failure.
     **/
    HooksRegistrar& response_header_data(header_data_t f);

    /**
     * Register callback for @ref request_started.
     *
     * @sa Engine::state_event_e
     *
     * @param[in] f Callback to register.
     * @returns @c *this for call chaining.
     * @throw IronBee++ exception on failure.
     **/
    HooksRegistrar& request_started(request_line_t f);

    /**
     * Register callback for @ref response_started.
     *
     * @sa Engine::state_event_e
     *
     * @param[in] f Callback to register.
     * @returns @c *this for call chaining.
     * @throw IronBee++ exception on failure.
     **/
    HooksRegistrar& response_started(response_line_t f);

    /**
     * Register callback for @ref connection_started.
     *
     * @sa Engine::state_event_e
     *
     * @param[in] f Callback to register.
     * @returns @c *this for call chaining.
     * @throw IronBee++ exception on failure.
     **/
    HooksRegistrar& connection_started(connection_t f);

    /**
     * Register callback for @ref connection_finished.
     *
     * @sa Engine::state_event_e
     *
     * @param[in] f Callback to register.
     * @returns @c *this for call chaining.
     * @throw IronBee++ exception on failure.
     **/
    HooksRegistrar& connection_finished(connection_t f);

    /**
     * Register callback for @ref connection_opened.
     *
     * @sa Engine::state_event_e
     *
     * @param[in] f Callback to register.
     * @returns @c *this for call chaining.
     * @throw IronBee++ exception on failure.
     **/
    HooksRegistrar& connection_opened(connection_t f);

    /**
     * Register callback for @ref connection_closed.
     *
     * @sa Engine::state_event_e
     *
     * @param[in] f Callback to register.
     * @returns @c *this for call chaining.
     * @throw IronBee++ exception on failure.
     **/
    HooksRegistrar& connection_closed(connection_t f);

    /**
     * Register callback for @ref handle_context_connection.
     *
     * @sa Engine::state_event_e
     *
     * @param[in] f Callback to register.
     * @returns @c *this for call chaining.
     * @throw IronBee++ exception on failure.
     **/
    HooksRegistrar& handle_context_connection(connection_t f);

    /**
     * Register callback for @ref handle_connect.
     *
     * @sa Engine::state_event_e
     *
     * @param[in] f Callback to register.
     * @returns @c *this for call chaining.
     * @throw IronBee++ exception on failure.
     **/
    HooksRegistrar& handle_connect(connection_t f);

    /**
     * Register callback for @ref handle_disconnect.
     *
     * @sa Engine::state_event_e
     *
     * @param[in] f Callback to register.
     * @returns @c *this for call chaining.
     * @throw IronBee++ exception on failure.
     **/
    HooksRegistrar& handle_disconnect(connection_t f);

    /**
     * Register callback for @ref transaction_started.
     *
     * @sa Engine::state_event_e
     *
     * @param[in] f Callback to register.
     * @returns @c *this for call chaining.
     * @throw IronBee++ exception on failure.
     **/
    HooksRegistrar& transaction_started(transaction_t f);

    /**
     * Register callback for @ref transaction_process.
     *
     * @sa Engine::state_event_e
     *
     * @param[in] f Callback to register.
     * @returns @c *this for call chaining.
     * @throw IronBee++ exception on failure.
     **/
    HooksRegistrar& transaction_process(transaction_t f);

    /**
     * Register callback for @ref transaction_finished.
     *
     * @sa Engine::state_event_e
     *
     * @param[in] f Callback to register.
     * @returns @c *this for call chaining.
     * @throw IronBee++ exception on failure.
     **/
    HooksRegistrar& transaction_finished(transaction_t f);

    /**
     * Register callback for @ref handle_context_transaction.
     *
     * @sa Engine::state_event_e
     *
     * @param[in] f Callback to register.
     * @returns @c *this for call chaining.
     * @throw IronBee++ exception on failure.
     **/
    HooksRegistrar& handle_context_transaction(transaction_t f);

    /**
     * Register callback for @ref handle_request_header.
     *
     * @sa Engine::state_event_e
     *
     * @param[in] f Callback to register.
     * @returns @c *this for call chaining.
     * @throw IronBee++ exception on failure.
     **/
    HooksRegistrar& handle_request_header(transaction_t f);

    /**
     * Register callback for @ref handle_request.
     *
     * @sa Engine::state_event_e
     *
     * @param[in] f Callback to register.
     * @returns @c *this for call chaining.
     * @throw IronBee++ exception on failure.
     **/
    HooksRegistrar& handle_request(transaction_t f);

    /**
     * Register callback for @ref handle_response_header.
     *
     * @sa Engine::state_event_e
     *
     * @param[in] f Callback to register.
     * @returns @c *this for call chaining.
     * @throw IronBee++ exception on failure.
     **/
    HooksRegistrar& handle_response_header(transaction_t f);

    /**
     * Register callback for @ref handle_response.
     *
     * @sa Engine::state_event_e
     *
     * @param[in] f Callback to register.
     * @returns @c *this for call chaining.
     * @throw IronBee++ exception on failure.
     **/
    HooksRegistrar& handle_response(transaction_t f);

    /**
     * Register callback for @ref handle_postprocess.
     *
     * @sa Engine::state_event_e
     *
     * @param[in] f Callback to register.
     * @returns @c *this for call chaining.
     * @throw IronBee++ exception on failure.
     **/
    HooksRegistrar& handle_postprocess(transaction_t f);

    /**
     * Register callback for @ref handle_logging.
     *
     * @sa Engine::state_event_e
     *
     * @param[in] f Callback to register.
     * @returns @c *this for call chaining.
     * @throw IronBee++ exception on failure.
     **/
    HooksRegistrar& handle_logging(transaction_t f);

    /**
     * Register callback for @ref request_header_finished.
     *
     * @sa Engine::state_event_e
     *
     * @param[in] f Callback to register.
     * @returns @c *this for call chaining.
     * @throw IronBee++ exception on failure.
     **/
    HooksRegistrar& request_header_finished(transaction_t f);

    /**
     * Register callback for @ref request_finished.
     *
     * @sa Engine::state_event_e
     *
     * @param[in] f Callback to register.
     * @returns @c *this for call chaining.
     * @throw IronBee++ exception on failure.
     **/
    HooksRegistrar& request_finished(transaction_t f);

    /**
     * Register callback for @ref response_header_finished.
     *
     * @sa Engine::state_event_e
     *
     * @param[in] f Callback to register.
     * @returns @c *this for call chaining.
     * @throw IronBee++ exception on failure.
     **/
    HooksRegistrar& response_header_finished(transaction_t f);

    /**
     * Register callback for @ref response_finished.
     *
     * @sa Engine::state_event_e
     *
     * @param[in] f Callback to register.
     * @returns @c *this for call chaining.
     * @throw IronBee++ exception on failure.
     **/
    HooksRegistrar& response_finished(transaction_t f);

    /**
     * Register callback for @ref transaction_data_in.
     *
     * @sa Engine::state_event_e
     *
     * @param[in] f Callback to register.
     * @returns @c *this for call chaining.
     * @throw IronBee++ exception on failure.
     **/
    HooksRegistrar& transaction_data_in(transaction_data_t f);

    /**
     * Register callback for @ref transaction_data_out.
     *
     * @sa Engine::state_event_e
     *
     * @param[in] f Callback to register.
     * @returns @c *this for call chaining.
     * @throw IronBee++ exception on failure.
     **/
    HooksRegistrar& transaction_data_out(transaction_data_t f);

    /**
     * Register callback for @ref request_body_data.
     *
     * @sa Engine::state_event_e
     *
     * @param[in] f Callback to register.
     * @returns @c *this for call chaining.
     * @throw IronBee++ exception on failure.
     **/
    HooksRegistrar& request_body_data(transaction_data_t f);

    /**
     * Register callback for @ref response_body_data.
     *
     * @sa Engine::state_event_e
     *
     * @param[in] f Callback to register.
     * @returns @c *this for call chaining.
     * @throw IronBee++ exception on failure.
     **/
    HooksRegistrar& response_body_data(transaction_data_t f);

    /**
     * Register callback for @ref context_open.
     *
     * @sa Engine::state_event_e
     *
     * @param[in] f Callback to register.
     * @returns @c *this for call chaining.
     * @throw IronBee++ exception on failure.
     **/
    HooksRegistrar& context_open(context_t f);

    /**
     * Register callback for @ref context_close.
     *
     * @sa Engine::state_event_e
     *
     * @param[in] f Callback to register.
     * @returns @c *this for call chaining.
     * @throw IronBee++ exception on failure.
     **/
    HooksRegistrar& context_close(context_t f);

    /**
     * Register callback for @ref context_destroy.
     *
     * @sa Engine::state_event_e
     *
     * @param[in] f Callback to register.
     * @returns @c *this for call chaining.
     * @throw IronBee++ exception on failure.
     **/
    HooksRegistrar& context_destroy(context_t f);

    /**
     * Register callback for @ref engine_shutdown_initiated.
     *
     * @sa engine::state_event_e
     *
     * @param[in] f Callback to register.
     * @returns @c *this for call chaining.
     * @throw IronBee++ exception on failure.
     */
    HooksRegistrar& engine_shutdown_initiated(null_t f);

    ///@}

private:
    Engine m_engine;
};

} // IronBee

#endif
