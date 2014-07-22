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

#ifndef _IB_ENGINE_STATE_H_
#define _IB_ENGINE_STATE_H_

/**
 * @file
 * @brief IronBee --- Engine state
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Christopher Alfeld <calfeld@qualys.com>
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/engine_types.h>
#include <ironbee/parsed_content.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeEngineState State
 * @ingroup IronBee
 *
 * This API allows for sending state and data information to the engine.
 *
 * The following diagrams the general state processing order of a typical
 * transaction that is allowed to be fully processed.
 *
 * @dot
 * digraph legend1 {
 *   server_state [label="Server\nstate",shape=octagon]
 *   server_async_state [label="Server\nasyncronous\nstate",shape=octagon,peripheries=2]
 *   parser_state [label="Parser\nstate",shape=ellipse]
 *   engine_state [label="Engine\nstate",shape=diamond]
 *   handler_state [label="Handler\nstate",shape=parallelogram]
 *   entity [label="Entity",shape=box]
 *   note [label="Note",shape=note]
 *   {rank=same; server_state server_async_state parser_state engine_state handler_state note}
 * }
 * @enddot
 * @dot
 * digraph legend2 {
 *   edge1a [style=invis]
 *   edge1b [style=invis]
 *   edge2a [style=invis]
 *   edge2b [style=invis]
 *   edge3a [style=invis]
 *   edge3b [style=invis]
 *   { rank=same; edge1a edge1b edge2a edge2b edge3a edge3b }
 *
 *   edge1a -> edge1b [label="Ordered Transition\nto State"]
 *   edge2a -> edge2b [label="Influences Transition\nto State",style=dotted,arrowhead=none]
 *   edge3a -> edge3b [label="Alternate Transition\nto State",style=dashed]
 * }
 * @enddot
 * @dot
 * digraph legend3 {
 *   conn_style [label="connection state",style=bold,shape=box]
 *   tx_style [label="transaction state",style=filled,fillcolor="#e6e6e6",shape=box]
 *   data_style [label="data state",style=solid,shape=box]
 *   { rank=same; conn_style tx_style data_style }
 * }
 * @enddot
 * @dot
 * digraph legend3 {
 *   note1 [label="*  Automatically triggered if\l    not done explicitly (some\l    parsers may not be capable).\l",style=bold,shape=plaintext]
 *   note2 [label="** Special handler states allowing\l    modules to do any setup/init\l    within their context.\l",style=bold,shape=plaintext]
 *   { rank=same; note1 note2 }
 * }
 * @enddot
 *
 * Server states are triggered by the server and parser states by the
 * parser. These states cause the engine to trigger both the engine and
 * handler states. The engine states are meant to be synchronization
 * points. The handler states are meant to be handled by modules to do
 * detection and take actions, while the server and parser states are
 * to be used to generate fields and anything else needed in the handler
 * states.
 *
 * - Connection hook callbacks receive a @ref ib_conn_t parameter.
 * - Transaction hook callbacks receive a @ref ib_tx_t parameter.
 * - Transaction Data hook callbacks receive a `const char*` and
 *   `size_t` parameter.
 *
 * @note Config contexts and some fields are populated during the server
 *       states and thus the following handler state is what should be used
 *       to use these contexts and fields for detection.
 *
 * The following diagram shows a complete connection from start to finish.
 *
 * @dot
 * digraph states {
 *
 *   start [label="start",style=bold,shape=plaintext]
 *   finish [label="finish",style=bold,shape=plaintext]
 *
 *   context_conn_selected [label="At this point the connection context is\nselected. States that follow will use the context\nselected here, which may impose a different\nconfiguration than previous states. Anything\nused in the context selection process must\nbe generated in a previous state handler.",style=bold,shape=note,URL="\ref handle_context_conn_state"]
 *   context_tx_selected [label="At this point the transaction context is\nselected. States that follow will use the context\nselected here, which may impose a different\nconfiguration than previous states. Anything\nused in the context selection process must\nbe generated in a previous state handler.\nAdditionally, any transaction data filters will\nnot be called until after this point so that\nfilters will be called with a single context.",style=filled,fillcolor="#e6e6e6",shape=note,URL="\ref handle_context_tx_state"]
 *
 *   conn_started_state [label="conn_started",style=bold,shape=diamond,URL="\ref conn_started_state"]
 *   conn_opened_state [label="conn_opened",style=bold,shape=octagon,URL="\ref conn_opened_state"]
 *   conn_closed_state [label="conn_closed",style=bold,shape=octagon,URL="\ref conn_closed_state"]
 *   conn_finished_state [label="conn_finished",style=bold,shape=diamond,URL="\ref conn_finished_state"]
 *
 *   tx_started_state [label="tx_started",style=filled,fillcolor="#e6e6e6",shape=diamond,URL="\ref tx_started_state"]
 *   tx_process_state [label="tx_process",style=filled,fillcolor="#e6e6e6",shape=diamond,URL="\ref tx_process_state"]
 *   tx_finished_state [label="tx_finished",style=filled,fillcolor="#e6e6e6",shape=diamond,URL="\ref tx_finished_state"]
 *
 *   request_started_state [label="request_started *",style=filled,fillcolor="#e6e6e6",shape=ellipse,URL="\ref request_started_state"]
 *   request_header_process_state [label="request_header_process",style=filled,fillcolor="#e6e6e6",shape=ellipse,URL="\ref request_header_process_state"]
 *   request_header_finished_state [label="request_header_finished",style=filled,fillcolor="#e6e6e6",shape=ellipse,URL="\ref request_header_finished_state"]
 *   request_body_data_state [label="request_body_data",style=filled,fillcolor="#e6e6e6",shape=ellipse,URL="\ref request_body_data_state"]
 *   request_finished_state [label="request_finished",style=filled,fillcolor="#e6e6e6",shape=ellipse,URL="\ref request_finished_state"]
 *   response_started_state [label="response_started *",style=filled,fillcolor="#e6e6e6",shape=ellipse,URL="\ref response_started_state"]
 *   response_header_finished_state [label="response_header_finished",style=filled,fillcolor="#e6e6e6",shape=ellipse,URL="\ref response_header_finished_state"]
 *   response_body_data_state [label="response_body_data",style=filled,fillcolor="#e6e6e6",shape=ellipse,URL="\ref response_body_data_state"]
 *   response_finished_state [label="response_finished",style=filled,fillcolor="#e6e6e6",shape=ellipse,URL="\ref response_finished_state"]
 *
 *   handle_context_conn_state [label="handle_context_conn **",style=bold,shape=parallelogram,URL="\ref handle_context_conn_state"]
 *   handle_connect_state [label="handle_connect",style=bold,shape=parallelogram,URL="\ref handle_connect_state"]
 *   handle_context_tx_state [label="handle_context_tx **",style=filled,fillcolor="#e6e6e6",shape=parallelogram,URL="\ref handle_context_tx_state"]
 *   handle_request_header_state [label="handle_request_header",style=filled,fillcolor="#e6e6e6",shape=parallelogram,URL="\ref handle_request_header_state"]
 *   handle_request_state [label="handle_request",style=filled,fillcolor="#e6e6e6",shape=parallelogram,URL="\ref handle_request_state"]
 *   handle_response_header_state [label="handle_response_header",style=filled,fillcolor="#e6e6e6",shape=parallelogram,URL="\ref handle_response_header_state"]
 *   handle_response_state [label="handle_response",style=filled,fillcolor="#e6e6e6",shape=parallelogram,URL="\ref handle_response_state"]
 *   handle_disconnect_state [label="handle_disconnect",style=bold,shape=parallelogram,URL="\ref handle_disconnect_state"]
 *   handle_postprocess_state [label="handle_postprocess",style=filled,fillcolor="#e6e6e6",shape=parallelogram,URL="\ref handle_postprocess_state"]
 *   handle_logging_state [label="handle_logging",style=filled,fillcolor="#e6e6e6",shape=parallelogram,URL="\ref handle_logging_state"]
 *
 *   // These are just for organizational purposes
 *   tx_started_state -> tx_finished_state [style=invis,weight=5.0]
 *
 *   conn_started_state -> conn_opened_state [weight=100.0]
 *   conn_opened_state -> context_conn_selected [weight=100.0]
 *   context_conn_selected -> handle_context_conn_state [weight=100.0]
 *   handle_context_conn_state -> handle_connect_state [weight=100.0]
 *   handle_connect_state -> tx_started_state [weight=100.0]
 *
 *   tx_started_state -> request_started_state [weight=5.0]
 *   request_started_state -> request_header_process_state [weight=1.0]
 *   request_header_process_state -> context_tx_selected [weight=1.0]
 *   context_tx_selected -> handle_context_tx_state [weight=1.0]
 *   handle_context_tx_state -> request_header_finished_state [weight=1.0]
 *   request_header_finished_state -> handle_request_header_state [weight=1.0]
 *   handle_request_header_state -> request_started_state [label="HTTP\nPipeline\nRequest",style=dashed,weight=10.0]
 *   handle_request_header_state -> request_body_data_state [weight=1.0]
 *   request_body_data_state -> request_finished_state [weight=1.0]
 *   request_finished_state -> handle_request_state [weight=1.0]
 *   handle_request_state -> tx_process_state [weight=1.0]
 *
 *   tx_process_state -> response_started_state [weight=1.0]
 *
 *   response_started_state -> response_header_finished_state [weight=1.0]
 *   response_header_finished_state -> handle_response_header_state [weight=1.0]
 *   handle_response_header_state -> response_body_data_state [weight=1.0]
 *   response_body_data_state -> response_finished_state [weight=1.0]
 *   response_finished_state -> handle_response_state [weight=5.0]
 *   handle_response_state -> response_started_state [label="HTTP\nPipeline\nResponse",style=dashed,weight=10.0]
 *
 *   handle_response_state -> handle_postprocess_state [weight=5.0]
 *   handle_postprocess_state -> handle_logging_state [weight=5.0]
 *   handle_logging_state -> tx_finished_state [weight=5.0]
 *
 *   tx_finished_state -> tx_started_state [weight=5.0,constraint=false]
 *   tx_finished_state -> conn_closed_state [weight=5.0]
 *
 *   conn_closed_state -> handle_disconnect_state [weight=5.0]
 *   handle_disconnect_state -> conn_finished_state [weight=10.0]
 *
 *   conn_finished_state -> finish [weight=500.0]
 * }
 * @enddot
 *
 * @{
 */

/**
 * State Types
 *
 * @warning Remember to update ib_state_table_init() in engine.c when names
 * change, states are added or removed, etc..
 */
typedef enum {
    /* Engine States */
    conn_started_state,           /**< Connection started
                                   * (Hook type:@ref ib_state_conn_hook_fn_t) */
    conn_finished_state,          /**< Connection finished
                                   * (Hook type:@ref ib_state_conn_hook_fn_t) */
    tx_started_state,             /**< Transaction started
                                   * (Hook type:@ref ib_state_tx_hook_fn_t) */
    tx_process_state,             /**< Transaction is about to be processed
                                   * (Hook type:@ref ib_state_tx_hook_fn_t) */
    tx_finished_state,            /**< Transaction finished
                                   * (Hook type:@ref ib_state_tx_hook_fn_t) */

    /* Handler States */
    handle_context_conn_state,    /**< Handle connection context chosen
                                   * (Hook type:@ref ib_state_conn_hook_fn_t) */
    handle_connect_state,         /**< Handle a connect
                                   * (Hook type:@ref ib_state_conn_hook_fn_t) */
    handle_context_tx_state,      /**< Handle transaction context chosen
                                   * (Hook type:@ref ib_state_tx_hook_fn_t) */
    handle_request_header_state,  /**< Handle the request header
                                   * (Hook type:@ref ib_state_tx_hook_fn_t) */
    handle_request_state,         /**< Handle the full request
                                   * (Hook type:@ref ib_state_tx_hook_fn_t) */
    handle_response_header_state, /**< Handle the response header
                                   * (Hook type:@ref ib_state_tx_hook_fn_t) */
    handle_response_state,        /**< Handle the full response
                                   * (Hook type:@ref ib_state_tx_hook_fn_t) */
    handle_disconnect_state,      /**< Handle a disconnect
                                   * (Hook type:@ref ib_state_conn_hook_fn_t) */
    handle_postprocess_state,     /**< Handle transaction post processing
                                   * (Hook type:@ref ib_state_tx_hook_fn_t) */
    handle_logging_state,         /**< Handle transaction logging
                                   * (Hook type:@ref ib_state_tx_hook_fn_t) */

    /* Server States */
    conn_opened_state,            /**< Server notified connection opened
                                   * (Hook type:@ref ib_state_conn_hook_fn_t) */
    conn_closed_state,            /**< Server notified connection closed
                                   * (Hook type:@ref ib_state_conn_hook_fn_t) */

    /* Parser States */
    request_started_state,        /**< Parser notified request has started
                                   * (Hook type:@ref ib_state_request_line_fn_t) */
    request_header_data_state,    /**< Parser notified of request header data
                                   * (Hook type:@ref ib_state_header_data_fn_t) */
    request_header_process_state, /**< Parser notified of request header process
                                    * (Hook type:@ref ib_state_tx_hook_fn_t) */
    request_header_finished_state, /**< Parser notified of request header
                                    * (Hook type:@ref ib_state_tx_hook_fn_t) */
    request_body_data_state,       /**< Parser notified of request body
                                    * (Hook type:@ref ib_state_txdata_hook_fn_t) */
    request_finished_state,        /**< Parser notified request finished
                                    * (Hook type:@ref ib_state_tx_hook_fn_t) */
    response_started_state,        /**< Parser notified response started
                                    * (Hook type:@ref ib_state_response_line_fn_t) */
    response_header_data_state,    /**< Parser notified of response header data
                                    * (Hook type:@ref ib_state_header_data_fn_t) */
    response_header_finished_state,/**< Parser notified of response header
                                    * (Hook type:@ref ib_state_tx_hook_fn_t) */
    response_body_data_state,      /**< Parser notified of response body
                                    * (Hook type:@ref ib_state_txdata_hook_fn_t) */
    response_finished_state,       /**< Parser notified response finished
                                    * (Hook type:@ref ib_state_tx_hook_fn_t( */

    /* Logevent updated */
    handle_logevent_state,         /**< Logevent updated
                                    * (Hook type:@ref ib_state_tx_hook_fn_t) */

    /* Context states */
    context_open_state,            /**< Context open
                                    * (Hook type:@ref ib_state_ctx_hook_fn_t) */
    context_close_state,           /**< Context close
                                    * (Hook type:@ref ib_state_ctx_hook_fn_t) */
    context_destroy_state,         /**< Context destroy
                                    * (Hook type:@ref ib_state_ctx_hook_fn_t) */

    /* Engine states */
    engine_shutdown_initiated_state,/**< Engine has been requested to shut
                                    * down.
                                    * (Hook type:@ref ib_state_null_hook_fn_t) */
    /* Not an state, but keeps track of the number of states. */
    IB_STATE_NUM,
} ib_state_t;

/**
 * State Hook Types
 **/
typedef enum {
    IB_STATE_HOOK_NULL,     /**< Hook has no parameter
                             * (Hook type: @ref ib_state_null_hook_fn_t) */
    IB_STATE_HOOK_INVALID,  /**< Something went wrong
                             * (Hook type: None) */
    IB_STATE_HOOK_CTX,      /**< Hook receives context data
                             * (Hook type: @ref ib_state_ctx_hook_fn_t) */
    IB_STATE_HOOK_CONN,     /**< Hook receives connection data
                             * (Hook type: @ref ib_state_conn_hook_fn_t) */
    IB_STATE_HOOK_TX,       /**< Hook receives ib_tx_t
                             * (Hook type: @ref ib_state_tx_hook_fn_t) */
    IB_STATE_HOOK_TXDATA,   /**< Hook receives data and length
                             * (Hook type: @ref ib_state_txdata_hook_fn_t) */
    IB_STATE_HOOK_REQLINE,  /**< Hook receives ib_parsed_req_t
                             * (Hook type: @ref ib_state_request_line_fn_t) */
    IB_STATE_HOOK_RESPLINE, /**< Hook receives ib_parsed_resp_t
                             * (Hook type: @ref ib_state_response_line_fn_t) */
    IB_STATE_HOOK_HEADER    /**< Hook receives ib_parsed_header_t
                             * (Hook type: @ref ib_state_header_data_fn_t) */
} ib_state_hook_type_t;

/**
 * Hook type for a state.
 *
 * @param[in] state State type.
 * @return Hook type or IB_STATE_HOOK_INVALID if bad state.
 **/
ib_state_hook_type_t ib_state_hook_type(ib_state_t state);

/**
 * Dataless State Hook Callback Function.
 *
 * Related registration functions:
 * - ib_hook_null_register()
 *
 * Handles states: (None)
 *
 * @param ib Engine handle
 * @param state Which state trigger the callback.
 * @param cbdata Callback data
  */
typedef ib_status_t (*ib_state_null_hook_fn_t)(
    ib_engine_t *ib,
    ib_state_t state,
    void *cbdata
);

/**
 * Data state for parsed header.
 *
 * Related registration functions:
 * - ib_hook_parsed_header_data_register()
 *
 * Handles states:
 * - @ref request_header_data_state
 * - @ref response_header_data_state
 *
 * @param[in] ib Engine handle
 * @param[in] tx Transaction.
 * @param[in] state Which state trigger the callback.
 * @param[in] header Parsed connection header.
 * @param[in] cbdata Callback data
 */
typedef ib_status_t (*ib_state_header_data_fn_t)(
    ib_engine_t *ib,
    ib_tx_t *tx,
    ib_state_t state,
    ib_parsed_header_t *header,
    void *cbdata);

/**
 * Data state for the start of a request.
 *
 * This provides a request line parsed from the start of the request.
 *
 * Related registration functions:
 * - ib_hook_parsed_req_line_register()
 *
 * Handles states:
 * - @ref request_started_state
 *
 * @param[in] ib Engine handle
 * @param[in] tx Transaction.
 * @param[in] state Which state trigger the callback.
 * @param[in] line The parsed request line.
 * @param[in] cbdata Callback data
 */
typedef ib_status_t (*ib_state_request_line_fn_t)(
    ib_engine_t *ib,
    ib_tx_t *tx,
    ib_state_t state,
    ib_parsed_req_line_t *line,
    void *cbdata);

/**
 * Data state for the start of a response.
 *
 * This provides a response line parsed from the start of the response.
 *
 * Related registration functions:
 * - ib_hook_parsed_resp_line_register()
 *
 * Handles states:
 * - @ref response_started_state
 *
 * @param[in] ib Engine handle
 * @param[in] tx Transaction.
 * @param[in] state Which state trigger the callback.
 * @param[in] line The parsed response line.
 * @param[in] cbdata Callback data
 */
typedef ib_status_t (*ib_state_response_line_fn_t)(
    ib_engine_t *ib,
    ib_tx_t *tx,
    ib_state_t state,
    ib_parsed_resp_line_t *line,
    void *cbdata);

/**
 * Connection State Hook Callback Function.
 *
 * Related registration functions:
 * - ib_hook_conn_register()
 *
 * Handles states:
 * - @ref conn_started_state
 * - @ref conn_finished_state
 * - @ref handle_context_conn_state
 * - @ref handle_connect_state
 * - @ref handle_disconnect_state
 * - @ref conn_opened_state
 * - @ref conn_closed_state
 *
 * @param[in] ib Engine handle
 * @param[in] conn Connection.
 * @param[in] state Which state trigger the callback.
 * @param[in] cbdata Callback data
 */
typedef ib_status_t (*ib_state_conn_hook_fn_t)(
    ib_engine_t *ib,
    ib_conn_t *conn,
    ib_state_t state,
    void *cbdata
);

/**
 * Transaction State Hook Callback Function.
 *
 * This matches the NULL callback type as tx is already passed.
 *
 * Related registration functions:
 * - ib_hook_tx_register()
 *
 * Handles states:
 * - @ref tx_started_state
 * - @ref tx_process_state
 * - @ref tx_finished_state
 * - @ref handle_context_tx_state
 * - @ref handle_request_header_state
 * - @ref handle_request_state
 * - @ref handle_response_header_state
 * - @ref handle_response_state
 * - @ref handle_postprocess_state
 * - @ref handle_logging_state
 * - @ref request_header_process_state
 * - @ref request_header_finished_state
 * - @ref request_finished_state
 * - @ref response_header_finished_state
 * - @ref response_finished_state
 * - @ref handle_logevent_state
 *
 * @param[in] ib Engine handle
 * @param[in] tx Transaction.
 * @param[in] state Which state trigger the callback.
 * @param[in] cbdata Callback data
 */
typedef ib_status_t (*ib_state_tx_hook_fn_t)(
    ib_engine_t *ib,
    ib_tx_t *tx,
    ib_state_t state,
    void *cbdata
);

/**
 * Transaction Data State Hook Callback Function.
 *
 * Related registration functions:
 * - ib_hook_txdata_register()
 *
 * Handles states:
 * - @ref request_body_data_state
 * - @ref response_body_data_state
 *
 * @param[in] ib Engine handle
 * @param[in] tx Transaction.
 * @param[in] state Which state trigger the callback.
 * @param[in] data Transaction data.
 * @param[in] data_length Length of @a data.
 * @param[in] cbdata Callback data
 */
typedef ib_status_t (*ib_state_txdata_hook_fn_t)(
    ib_engine_t *ib,
    ib_tx_t *tx,
    ib_state_t state,
    const char *data, size_t data_length,
    void *cbdata
);

/**
 * Context State Hook Callback Function.
 *
 * Related registration functions:
 * - ib_hook_context_register()
 *
 * Handles states:
 * - @ref context_open_state
 * - @ref context_close_state
 * - @ref context_destroy_state
 *
 * @param[in] ib Engine handle
 * @param[in] ctx Config context
 * @param[in] state Which state trigger the callback.
 * @param[in] cbdata Callback data
 *
 * @returns Status code
 */
typedef ib_status_t (*ib_state_ctx_hook_fn_t)(
    ib_engine_t  *ib,
    ib_context_t *ctx,
    ib_state_t state,
    void         *cbdata
);

/**
 * Resolve a state name.
 *
 * @param state State
 *
 * @returns Statically allocated state name
 */
const char *ib_state_name(ib_state_t state);

/**
 * @}
 */


/**
 * @defgroup IronBeeEngineHooks Hooks
 * @ingroup IronBeeEngine
 *
 * Hook into engine states.
 *
 * @{
 */

/**
 * Register a callback for a no data state.
 *
 * @param ib Engine handle
 * @param state State
 * @param cb The callback to register
 * @param cbdata Data passed to the callback (or NULL)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hook_null_register(
    ib_engine_t *ib,
    ib_state_t state,
    ib_state_null_hook_fn_t cb,
    void *cbdata
);

/**
 * Register a callback for a connection state.
 *
 * @param ib Engine handle
 * @param state State
 * @param cb The callback to register
 * @param cbdata Data passed to the callback (or NULL)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hook_conn_register(
    ib_engine_t *ib,
    ib_state_t state,
    ib_state_conn_hook_fn_t cb,
    void *cbdata
);

/**
 * Register a callback for a transaction state.
 *
 * @param ib Engine handle
 * @param state State
 * @param cb The callback to register
 * @param cbdata Data passed to the callback (or NULL)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hook_tx_register(
    ib_engine_t *ib,
    ib_state_t state,
    ib_state_tx_hook_fn_t cb,
    void *cbdata
);

/**
 * Register a callback for a transaction data state.
 *
 * @param ib Engine handle
 * @param state State
 * @param cb The callback to register
 * @param cbdata Data passed to the callback (or NULL)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hook_txdata_register(
    ib_engine_t *ib,
    ib_state_t state,
    ib_state_txdata_hook_fn_t cb,
    void *cbdata
);

/**
 * Register a callback for a header data state.
 *
 * @param[in] ib IronBee engine.
 * @param[in] state The specific state.
 * @param[in] cb The callback to register.
 * @param[in] cbdata Data to provide to the callback.
 *
 * @returns Status code.
 */
ib_status_t DLL_PUBLIC ib_hook_parsed_header_data_register(
    ib_engine_t *ib,
    ib_state_t state,
    ib_state_header_data_fn_t cb,
    void *cbdata);

/**
 * Register a callback for a request line state.
 *
 * @param[in] ib IronBee engine.
 * @param[in] state The specific state.
 * @param[in] cb The callback to register.
 * @param[in] cbdata Data to provide to the callback.
 *
 * @returns Status code.
 */
ib_status_t DLL_PUBLIC ib_hook_parsed_req_line_register(
    ib_engine_t *ib,
    ib_state_t state,
    ib_state_request_line_fn_t cb,
    void *cbdata);

/**
 * Register a callback for a response line state.
 *
 * @param[in] ib IronBee engine.
 * @param[in] state The specific state.
 * @param[in] cb The callback to register.
 * @param[in] cbdata Data to provide to the callback.
 *
 * @returns Status code.
 */
ib_status_t DLL_PUBLIC ib_hook_parsed_resp_line_register(
    ib_engine_t *ib,
    ib_state_t state,
    ib_state_response_line_fn_t cb,
    void *cbdata);

/**
 * Register a callback for a context state.
 *
 * @param ib Engine handle
 * @param state State
 * @param cb The callback to register
 * @param cbdata Data passed to the callback (or NULL)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hook_context_register(
    ib_engine_t *ib,
    ib_state_t state,
    ib_state_ctx_hook_fn_t cb,
    void *cbdata
);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_ENGINE_STATE_H_ */
