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

#ifndef _IB_STATE_NOTIFY_H_
#define _IB_STATE_NOTIFY_H_

#include <ironbee/engine.h>
#include <ironbee/field.h>
#include <ironbee/parsed_content.h>
#include <ironbee/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * @brief IronBee --- Interface for handling parsed content.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

/**
 * @ingroup IronBeeEngineState
 * @{
 */

/**
 * Notify the state machine that a connection started.
 *
 * @param ib Engine handle
 * @param conn Connection
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_state_notify_conn_opened(ib_engine_t *ib,
                                                   ib_conn_t *conn);

/**
 * Notify the state machine that a connection finished.
 *
 * @param ib Engine handle
 * @param conn Connection
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_state_notify_conn_closed(ib_engine_t *ib,
                                                   ib_conn_t *conn);

/**
 * Notify the state machine that the request started and the
 * request line is available.
 *
 * The @a req parsed request object will be stored in @a tx for use after
 * the context has been determined.
 *
 * @param[in] ib Engine handle
 * @param[in] tx Transaction
 * @param[in] line The parsed request line object.
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_state_notify_request_started(
    ib_engine_t *ib,
    ib_tx_t *tx,
    ib_parsed_req_line_t *line);


/**
 * Notify the state machine that more request header data is available.
 *
 * If headers are delivered incrementally @a header should not be reused.
 *
 * To communicate the aggregate header to the proper call backs this will
 * aggregate header. This is done by storing the first @a header.
 * This is an optimistic optimization to avoid memory allocations.
 *
 * Upon subsequent calls to ib_state_notify_request_header_data the
 * original @a header structure is appended to
 * (using ib_parsed_name_value_pair_list_append).
 *
 * After the header is received @a tx->request_header contains a list
 * of all name/value pairs and is available via @a tx when
 * ib_state_notify_request_header_finished() is called.
 *
 * @param[in] ib Engine handle
 * @param[in] tx Transaction
 * @param[in] header Parsed header wrapper object
 *
 * @returns Status code.
 */
ib_status_t ib_state_notify_request_header_data(
    ib_engine_t *ib,
    ib_tx_t *tx,
    ib_parsed_header_wrapper_t *header);

/**
 * Notify the state machine that the request header is available.
 *
 * @param ib Engine handle
 * @param tx Transaction
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_state_notify_request_header_finished(
    ib_engine_t *ib,
    ib_tx_t *tx);

/**
 * Notify the state machine that more request body data is available.
 *
 * @param ib Engine handle
 * @param tx Transaction
 * @param data Transaction data
 * @param data_length Length of @a data.
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_state_notify_request_body_data(ib_engine_t *ib,
                                                         ib_tx_t *tx,
                                                         const char *data,
                                                         size_t data_length);

/**
 * Notify the state machine that the entire request is finished.
 *
 * @param ib Engine handle
 * @param tx Transaction data
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_state_notify_request_finished(ib_engine_t *ib,
                                                        ib_tx_t *tx);

/**
 * Notify the state machine that a response started and that the
 * response line is available.
 *
 * @note The line may be NULL for HTTP/0.9 requests which do not
 *       have a response line.
 *
 * @param[in] ib Engine handle
 * @param[in] tx Transaction data
 * @param[in] line The parsed response line object (NULL for HTTP/0.9)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_state_notify_response_started(
    ib_engine_t *ib,
    ib_tx_t *tx,
    ib_parsed_resp_line_t *line);

/**
 * Notify the state machine that more response header data is available.
 *
 * @param[in] ib IronBee engine.
 * @param[in] tx Transaction object.
 * @param[in] header Parsed header wrapper object
 *
 * @returns Status code.
 */
ib_status_t ib_state_notify_response_header_data(
    ib_engine_t *ib,
    ib_tx_t *tx,
    ib_parsed_header_wrapper_t *header);

/**
 * Notify the state machine that the response header is available.
 *
 * @param ib Engine handle
 * @param tx Transaction data
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_state_notify_response_header_finished(ib_engine_t *ib,
                                                                ib_tx_t *tx);

/**
 * Notify the state machine that more response body data is available.
 *
 * @param ib Engine handle
 * @param tx Transaction
 * @param txdata Transaction data
 * @param data_length Length of @a data
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_state_notify_response_body_data(ib_engine_t *ib,
                                                          ib_tx_t *tx,
                                                          const char *data,
                                                          size_t data_length);

/**
 * Notify the state machine that the entire response is finished.
 *
 * @param ib Engine handle
 * @param tx Transaction data
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_state_notify_response_finished(ib_engine_t *ib,
                                                         ib_tx_t *tx);


/**
 * Notify the state machine that post processing should run.
 *
 * @param ib Engine handle
 * @param tx Transaction data
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_state_notify_postprocess(ib_engine_t *ib,
                                                   ib_tx_t *tx);

/**
 * Notify the state machine that logging should run.
 *
 * @param ib Engine handle
 * @param tx Transaction data
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_state_notify_logging(ib_engine_t *ib,
                                               ib_tx_t *tx);

/**
 * Notify the state machine that a logevent event has occurred
 *
 * @param ib Engine handle
 * @param tx Transaction data
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_state_notify_logevent(ib_engine_t *ib,
                                                ib_tx_t *tx);

/**
 * Notify all listener that the IronBee Engine is being requested to shutdown.
 *
 * @param ib Engine handle
 *
 * @returns
 *   - IB_OK on success.
 *   - Non-IB_OK if a hook fails to fire correctly.
 */
ib_status_t DLL_PUBLIC ib_state_notify_engine_shutdown_initiated(
    ib_engine_t *ib);

/**
 * @} IronBeeEngineState
 */
#ifdef __cplusplus
}
#endif
#endif
