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
 *   conn_style [label="connection event",style=bold,shape=box]
 *   tx_style [label="transaction event",style=filled,fillcolor="#e6e6e6",shape=box]
 *   data_style [label="data event",style=solid,shape=box]
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
 * - Connection event hook callbacks receive a @ref ib_conn_t parameter.
 * - Transaction event hook callbacks receive a @ref ib_tx_t parameter.
 * - Transaction Data event hook callbacks receive a `const char*` and
 *   `size_t` parameter.
 *
 * @note Config contexts and some fields are populated during the server
 *       events and thus the following handler event is what should be used
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
 *   context_conn_selected [label="At this point the connection context is\nselected. Events that follow will use the context\nselected here, which may impose a different\nconfiguration than previous events. Anything\nused in the context selection process must\nbe generated in a previous event handler.",style=bold,shape=note,URL="\ref handle_context_conn_event"]
 *   context_tx_selected [label="At this point the transaction context is\nselected. Events that follow will use the context\nselected here, which may impose a different\nconfiguration than previous events. Anything\nused in the context selection process must\nbe generated in a previous event handler.\nAdditionally, any transaction data filters will\nnot be called until after this point so that\nfilters will be called with a single context.",style=filled,fillcolor="#e6e6e6",shape=note,URL="\ref handle_context_tx_event"]
 *
 *   conn_started_event [label="conn_started",style=bold,shape=diamond,URL="\ref conn_started_event"]
 *   conn_opened_event [label="conn_opened",style=bold,shape=octagon,URL="\ref conn_opened_event"]
 *   conn_closed_event [label="conn_closed",style=bold,shape=octagon,URL="\ref conn_closed_event"]
 *   conn_finished_event [label="conn_finished",style=bold,shape=diamond,URL="\ref conn_finished_event"]
 *
 *   tx_started_event [label="tx_started",style=filled,fillcolor="#e6e6e6",shape=diamond,URL="\ref tx_started_event"]
 *   tx_process_event [label="tx_process",style=filled,fillcolor="#e6e6e6",shape=diamond,URL="\ref tx_process_event"]
 *   tx_finished_event [label="tx_finished",style=filled,fillcolor="#e6e6e6",shape=diamond,URL="\ref tx_finished_event"]
 *
 *   request_started_event [label="request_started *",style=filled,fillcolor="#e6e6e6",shape=ellipse,URL="\ref request_started_event"]
 *   request_header_process_event [label="request_header_process",style=filled,fillcolor="#e6e6e6",shape=ellipse,URL="\ref request_header_process_event"]
 *   request_header_finished_event [label="request_header_finished",style=filled,fillcolor="#e6e6e6",shape=ellipse,URL="\ref request_header_finished_event"]
 *   request_body_data_event [label="request_body_data",style=filled,fillcolor="#e6e6e6",shape=ellipse,URL="\ref request_body_data_event"]
 *   request_finished_event [label="request_finished",style=filled,fillcolor="#e6e6e6",shape=ellipse,URL="\ref request_finished_event"]
 *   response_started_event [label="response_started *",style=filled,fillcolor="#e6e6e6",shape=ellipse,URL="\ref response_started_event"]
 *   response_header_finished_event [label="response_header_finished",style=filled,fillcolor="#e6e6e6",shape=ellipse,URL="\ref response_header_finished_event"]
 *   response_body_data_event [label="response_body_data",style=filled,fillcolor="#e6e6e6",shape=ellipse,URL="\ref response_body_data_event"]
 *   response_finished_event [label="response_finished",style=filled,fillcolor="#e6e6e6",shape=ellipse,URL="\ref response_finished_event"]
 *
 *   handle_context_conn_event [label="handle_context_conn **",style=bold,shape=parallelogram,URL="\ref handle_context_conn_event"]
 *   handle_connect_event [label="handle_connect",style=bold,shape=parallelogram,URL="\ref handle_connect_event"]
 *   handle_context_tx_event [label="handle_context_tx **",style=filled,fillcolor="#e6e6e6",shape=parallelogram,URL="\ref handle_context_tx_event"]
 *   handle_request_header_event [label="handle_request_header",style=filled,fillcolor="#e6e6e6",shape=parallelogram,URL="\ref handle_request_header_event"]
 *   handle_request_event [label="handle_request",style=filled,fillcolor="#e6e6e6",shape=parallelogram,URL="\ref handle_request_event"]
 *   handle_response_header_event [label="handle_response_header",style=filled,fillcolor="#e6e6e6",shape=parallelogram,URL="\ref handle_response_header_event"]
 *   handle_response_event [label="handle_response",style=filled,fillcolor="#e6e6e6",shape=parallelogram,URL="\ref handle_response_event"]
 *   handle_disconnect_event [label="handle_disconnect",style=bold,shape=parallelogram,URL="\ref handle_disconnect_event"]
 *   handle_postprocess_event [label="handle_postprocess",style=filled,fillcolor="#e6e6e6",shape=parallelogram,URL="\ref handle_postprocess_event"]
 *   handle_logging_event [label="handle_logging",style=filled,fillcolor="#e6e6e6",shape=parallelogram,URL="\ref handle_logging_event"]
 *
 *   // These are just for organizational purposes
 *   tx_started_event -> tx_finished_event [style=invis,weight=5.0]
 *
 *   conn_started_event -> conn_opened_event [weight=100.0]
 *   conn_opened_event -> context_conn_selected [weight=100.0]
 *   context_conn_selected -> handle_context_conn_event [weight=100.0]
 *   handle_context_conn_event -> handle_connect_event [weight=100.0]
 *   handle_connect_event -> tx_started_event [weight=100.0]
 *
 *   tx_started_event -> request_started_event [weight=5.0]
 *   request_started_event -> request_header_process_event [weight=1.0]
 *   request_header_process_event -> context_tx_selected [weight=1.0]
 *   context_tx_selected -> handle_context_tx_event [weight=1.0]
 *   handle_context_tx_event -> request_header_finished_event [weight=1.0]
 *   request_header_finished_event -> handle_request_header_event [weight=1.0]
 *   handle_request_header_event -> request_started_event [label="HTTP\nPipeline\nRequest",style=dashed,weight=10.0]
 *   handle_request_header_event -> request_body_data_event [weight=1.0]
 *   request_body_data_event -> request_finished_event [weight=1.0]
 *   request_finished_event -> handle_request_event [weight=1.0]
 *   handle_request_event -> tx_process_event [weight=1.0]
 *
 *   tx_process_event -> response_started_event [weight=1.0]
 *
 *   response_started_event -> response_header_finished_event [weight=1.0]
 *   response_header_finished_event -> handle_response_header_event [weight=1.0]
 *   handle_response_header_event -> response_body_data_event [weight=1.0]
 *   response_body_data_event -> response_finished_event [weight=1.0]
 *   response_finished_event -> handle_response_event [weight=5.0]
 *   handle_response_event -> response_started_event [label="HTTP\nPipeline\nResponse",style=dashed,weight=10.0]
 *
 *   handle_response_event -> handle_postprocess_event [weight=5.0]
 *   handle_postprocess_event -> handle_logging_event [weight=5.0]
 *   handle_logging_event -> tx_finished_event [weight=5.0]
 *
 *   tx_finished_event -> tx_started_event [weight=5.0,constraint=false]
 *   tx_finished_event -> conn_closed_event [weight=5.0]
 *
 *   conn_closed_event -> handle_disconnect_event [weight=5.0]
 *   handle_disconnect_event -> conn_finished_event [weight=10.0]
 *
 *   conn_finished_event -> finish [weight=500.0]
 * }
 * @enddot
 *
 * @{
 */

/**
 * State Event Types
 *
 * @warning Remember to update ib_event_table_init() in engine.c when names
 * change, states are added or removed, etc..
 */
typedef enum {
    /* Engine States */
    conn_started_event,           /**< Connection started
                                   * (Hook type:@ref ib_state_conn_hook_fn_t) */
    conn_finished_event,          /**< Connection finished
                                   * (Hook type:@ref ib_state_conn_hook_fn_t) */
    tx_started_event,             /**< Transaction started
                                   * (Hook type:@ref ib_state_tx_hook_fn_t) */
    tx_process_event,             /**< Transaction is about to be processed
                                   * (Hook type:@ref ib_state_tx_hook_fn_t) */
    tx_finished_event,            /**< Transaction finished
                                   * (Hook type:@ref ib_state_tx_hook_fn_t) */

    /* Handler States */
    handle_context_conn_event,    /**< Handle connection context chosen
                                   * (Hook type:@ref ib_state_conn_hook_fn_t) */
    handle_connect_event,         /**< Handle a connect
                                   * (Hook type:@ref ib_state_conn_hook_fn_t) */
    handle_context_tx_event,      /**< Handle transaction context chosen
                                   * (Hook type:@ref ib_state_tx_hook_fn_t) */
    handle_request_header_event,  /**< Handle the request header
                                   * (Hook type:@ref ib_state_tx_hook_fn_t) */
    handle_request_event,         /**< Handle the full request
                                   * (Hook type:@ref ib_state_tx_hook_fn_t) */
    handle_response_header_event, /**< Handle the response header
                                   * (Hook type:@ref ib_state_tx_hook_fn_t) */
    handle_response_event,        /**< Handle the full response
                                   * (Hook type:@ref ib_state_tx_hook_fn_t) */
    handle_disconnect_event,      /**< Handle a disconnect
                                   * (Hook type:@ref ib_state_conn_hook_fn_t) */
    handle_postprocess_event,     /**< Handle transaction post processing
                                   * (Hook type:@ref ib_state_tx_hook_fn_t) */
    handle_logging_event,         /**< Handle transaction logging
                                   * (Hook type:@ref ib_state_tx_hook_fn_t) */

    /* Server States */
    conn_opened_event,            /**< Server notified connection opened
                                   * (Hook type:@ref ib_state_conn_hook_fn_t) */
    conn_closed_event,            /**< Server notified connection closed
                                   * (Hook type:@ref ib_state_conn_hook_fn_t) */

    /* Parser States */
    request_started_event,        /**< Parser notified request has started
                                   * (Hook type:@ref ib_state_request_line_fn_t) */
    request_header_data_event,    /**< Parser notified of request header data
                                   * (Hook type:@ref ib_state_header_data_fn_t) */
    request_header_process_event, /**< Parser notified of request header process
                                    * (Hook type:@ref ib_state_tx_hook_fn_t) */
    request_header_finished_event, /**< Parser notified of request header
                                    * (Hook type:@ref ib_state_tx_hook_fn_t) */
    request_body_data_event,       /**< Parser notified of request body
                                    * (Hook type:@ref ib_state_txdata_hook_fn_t) */
    request_finished_event,        /**< Parser notified request finished
                                    * (Hook type:@ref ib_state_tx_hook_fn_t) */
    response_started_event,        /**< Parser notified response started
                                    * (Hook type:@ref ib_state_response_line_fn_t) */
    response_header_data_event,    /**< Parser notified of response header data
                                    * (Hook type:@ref ib_state_header_data_fn_t) */
    response_header_finished_event,/**< Parser notified of response header
                                    * (Hook type:@ref ib_state_tx_hook_fn_t) */
    response_body_data_event,      /**< Parser notified of response body
                                    * (Hook type:@ref ib_state_txdata_hook_fn_t) */
    response_finished_event,       /**< Parser notified response finished
                                    * (Hook type:@ref ib_state_tx_hook_fn_t( */

    /* Logevent updated */
    handle_logevent_event,         /**< Logevent updated
                                    * (Hook type:@ref ib_state_tx_hook_fn_t) */

    /* Context states */
    context_open_event,            /**< Context open
                                    * (Hook type:@ref ib_state_ctx_hook_fn_t) */
    context_close_event,           /**< Context close
                                    * (Hook type:@ref ib_state_ctx_hook_fn_t) */
    context_destroy_event,         /**< Context destroy
                                    * (Hook type:@ref ib_state_ctx_hook_fn_t) */

    /* Engine Events */
    engine_shutdown_initiated_event,/**< Engine has been requested to shut
                                    * down.
                                    * (Hook type:@ref ib_state_null_hook_fn_t) */
    /* Not an event, but keeps track of the number of events. */
    IB_STATE_EVENT_NUM,
} ib_state_event_type_t;

/**
 * State Event Hook Types
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
 * Hook type for an event.
 *
 * @param[in] event Event type.
 * @return Hook type or IB_STATE_HOOK_INVALID if bad event.
 **/
ib_state_hook_type_t ib_state_hook_type(ib_state_event_type_t event);

/**
 * Dataless Event Hook Callback Function.
 *
 * Related registration functions:
 * - ib_hook_null_register()
 *
 * Handles events: (None)
 *
 * @param ib Engine handle
 * @param event Which event trigger the callback.
 * @param cbdata Callback data
  */
typedef ib_status_t (*ib_state_null_hook_fn_t)(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    void *cbdata
);

/**
 * Data event for parsed header.
 *
 * Related registration functions:
 * - ib_hook_parsed_header_data_register()
 *
 * Handles events:
 * - @ref request_header_data_event
 * - @ref response_header_data_event
 *
 * @param[in] ib Engine handle
 * @param[in] tx Transaction.
 * @param[in] event Which event trigger the callback.
 * @param[in] header Parsed connection header.
 * @param[in] cbdata Callback data
 */
typedef ib_status_t (*ib_state_header_data_fn_t)(
    ib_engine_t *ib,
    ib_tx_t *tx,
    ib_state_event_type_t event,
    ib_parsed_header_t *header,
    void *cbdata);

/**
 * Data event for the start of a request.
 *
 * This provides a request line parsed from the start of the request.
 *
 * Related registration functions:
 * - ib_hook_parsed_req_line_register()
 *
 * Handles events:
 * - @ref request_started_event
 *
 * @param[in] ib Engine handle
 * @param[in] tx Transaction.
 * @param[in] event Which event trigger the callback.
 * @param[in] line The parsed request line.
 * @param[in] cbdata Callback data
 */
typedef ib_status_t (*ib_state_request_line_fn_t)(
    ib_engine_t *ib,
    ib_tx_t *tx,
    ib_state_event_type_t event,
    ib_parsed_req_line_t *line,
    void *cbdata);

/**
 * Data event for the start of a response.
 *
 * This provides a response line parsed from the start of the response.
 *
 * Related registration functions:
 * - ib_hook_parsed_resp_line_register()
 *
 * Handles events:
 * - @ref response_started_event
 *
 * @param[in] ib Engine handle
 * @param[in] tx Transaction.
 * @param[in] event Which event trigger the callback.
 * @param[in] line The parsed response line.
 * @param[in] cbdata Callback data
 */
typedef ib_status_t (*ib_state_response_line_fn_t)(
    ib_engine_t *ib,
    ib_tx_t *tx,
    ib_state_event_type_t event,
    ib_parsed_resp_line_t *line,
    void *cbdata);

/**
 * Connection Event Hook Callback Function.
 *
 * Related registration functions:
 * - ib_hook_conn_register()
 *
 * Handles events:
 * - @ref conn_started_event
 * - @ref conn_finished_event
 * - @ref handle_context_conn_event
 * - @ref handle_connect_event
 * - @ref handle_disconnect_event
 * - @ref conn_opened_event
 * - @ref conn_closed_event
 *
 * @param[in] ib Engine handle
 * @param[in] conn Connection.
 * @param[in] event Which event trigger the callback.
 * @param[in] cbdata Callback data
 */
typedef ib_status_t (*ib_state_conn_hook_fn_t)(
    ib_engine_t *ib,
    ib_conn_t *conn,
    ib_state_event_type_t event,
    void *cbdata
);

/**
 * Transaction Event Hook Callback Function.
 *
 * This matches the NULL callback type as tx is already passed.
 *
 * Related registration functions:
 * - ib_hook_tx_register()
 *
 * Handles events:
 * - @ref tx_started_event
 * - @ref tx_process_event
 * - @ref tx_finished_event
 * - @ref handle_context_tx_event
 * - @ref handle_request_header_event
 * - @ref handle_request_event
 * - @ref handle_response_header_event
 * - @ref handle_response_event
 * - @ref handle_postprocess_event
 * - @ref handle_logging_event
 * - @ref request_header_process_event
 * - @ref request_header_finished_event
 * - @ref request_finished_event
 * - @ref response_header_finished_event
 * - @ref response_finished_event
 * - @ref handle_logevent_event
 *
 * @param[in] ib Engine handle
 * @param[in] tx Transaction.
 * @param[in] event Which event trigger the callback.
 * @param[in] cbdata Callback data
 */
typedef ib_status_t (*ib_state_tx_hook_fn_t)(
    ib_engine_t *ib,
    ib_tx_t *tx,
    ib_state_event_type_t event,
    void *cbdata
);

/**
 * Transaction Data Event Hook Callback Function.
 *
 * Related registration functions:
 * - ib_hook_txdata_register()
 *
 * Handles events:
 * - @ref request_body_data_event
 * - @ref response_body_data_event
 *
 * @param[in] ib Engine handle
 * @param[in] tx Transaction.
 * @param[in] event Which event trigger the callback.
 * @param[in] data Transaction data.
 * @param[in] data_length Length of @a data.
 * @param[in] cbdata Callback data
 */
typedef ib_status_t (*ib_state_txdata_hook_fn_t)(
    ib_engine_t *ib,
    ib_tx_t *tx,
    ib_state_event_type_t event,
    const char *data, size_t data_length,
    void *cbdata
);

/**
 * Context Event Hook Callback Function.
 *
 * Related registration functions:
 * - ib_hook_context_register()
 *
 * Handles events:
 * - @ref context_open_event
 * - @ref context_close_event
 * - @ref context_destroy_event
 *
 * @param[in] ib Engine handle
 * @param[in] ctx Config context
 * @param[in] event Which event trigger the callback.
 * @param[in] cbdata Callback data
 *
 * @returns Status code
 */
typedef ib_status_t (*ib_state_ctx_hook_fn_t)(
    ib_engine_t  *ib,
    ib_context_t *ctx,
    ib_state_event_type_t event,
    void         *cbdata
);

/**
 * Resolve an event name.
 *
 * @param event Event type
 *
 * @returns Statically allocated event name
 */
const char *ib_state_event_name(ib_state_event_type_t event);

/**
 * @}
 */


/**
 * @defgroup IronBeeEngineHooks Hooks
 * @ingroup IronBeeEngine
 *
 * Hook into engine events.
 *
 * @{
 */

/**
 * Register a callback for a no data event.
 *
 * @param ib Engine handle
 * @param event Event
 * @param cb The callback to register
 * @param cbdata Data passed to the callback (or NULL)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hook_null_register(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_null_hook_fn_t cb,
    void *cbdata
);

/**
 * Register a callback for a connection event.
 *
 * @param ib Engine handle
 * @param event Event
 * @param cb The callback to register
 * @param cbdata Data passed to the callback (or NULL)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hook_conn_register(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_conn_hook_fn_t cb,
    void *cbdata
);

/**
 * Register a callback for a transaction event.
 *
 * @param ib Engine handle
 * @param event Event
 * @param cb The callback to register
 * @param cbdata Data passed to the callback (or NULL)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hook_tx_register(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_tx_hook_fn_t cb,
    void *cbdata
);

/**
 * Register a callback for a transaction data event.
 *
 * @param ib Engine handle
 * @param event Event
 * @param cb The callback to register
 * @param cbdata Data passed to the callback (or NULL)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hook_txdata_register(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_txdata_hook_fn_t cb,
    void *cbdata
);

/**
 * Register a callback for a header data event.
 *
 * @param[in] ib IronBee engine.
 * @param[in] event The specific event.
 * @param[in] cb The callback to register.
 * @param[in] cbdata Data to provide to the callback.
 *
 * @returns Status code.
 */
ib_status_t DLL_PUBLIC ib_hook_parsed_header_data_register(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_header_data_fn_t cb,
    void *cbdata);

/**
 * Register a callback for a request line event.
 *
 * @param[in] ib IronBee engine.
 * @param[in] event The specific event.
 * @param[in] cb The callback to register.
 * @param[in] cbdata Data to provide to the callback.
 *
 * @returns Status code.
 */
ib_status_t DLL_PUBLIC ib_hook_parsed_req_line_register(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_request_line_fn_t cb,
    void *cbdata);

/**
 * Register a callback for a response line event.
 *
 * @param[in] ib IronBee engine.
 * @param[in] event The specific event.
 * @param[in] cb The callback to register.
 * @param[in] cbdata Data to provide to the callback.
 *
 * @returns Status code.
 */
ib_status_t DLL_PUBLIC ib_hook_parsed_resp_line_register(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_response_line_fn_t cb,
    void *cbdata);

/**
 * Register a callback for a context event.
 *
 * @param ib Engine handle
 * @param event Event
 * @param cb The callback to register
 * @param cbdata Data passed to the callback (or NULL)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hook_context_register(
    ib_engine_t *ib,
    ib_state_event_type_t event,
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
