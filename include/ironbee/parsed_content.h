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
 *****************************************************************************/

#ifndef _IB_PARSED_CONTENT_H_
#define _IB_PARSED_CONTENT_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * @brief IronBee interface for handling parsed content.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

//#include <ironbee/build.h>
//#include <ironbee_config_auto.h>
#include <ironbee/bytestr.h>
#include <ironbee/engine.h>
#include <ironbee/field.h>
#include <ironbee/types.h>

///! The HTTP request version, typically HTTP/1.1.
typedef ib_bytestr_t ib_parsed_req_version_t;

///! The HTTP request path, such as /index.php?arg1=var1.
typedef ib_bytestr_t ib_parsed_req_path_t;

///! The HTTP request method, such as GET or PUT.
typedef ib_bytestr_t ib_parsed_req_method_t;

///! An HTTP header name.
typedef ib_bytestr_t ib_parsed_header_name_t;

///! An HTTP header value.
typedef ib_bytestr_t ib_parsed_header_value_t;

///! An HTTP trailer name.
typedef ib_bytestr_t ib_parsed_trailer_name_t;

///! An HTTP trailer value.
typedef ib_bytestr_t ib_parsed_trailer_value_t;

///! Response status code.
typedef ib_bytestr_t ib_parsed_resp_status_code_t;

///! Response status message.
typedef ib_bytestr_t ib_parsed_resp_status_msg_t;

///! Bulk data, such as the body of an HTTP GET request.
typedef ib_bytestr_t ib_parsed_data_t;

///! Opaque transaction representation.
typedef struct ib_parsed_tx_t {
    ib_engine_t *ib_engine; /**< The engine handling this transaction. */
    void        *user_data; /**< Opaque pointer containing user data. */
} ib_parsed_tx_t;

/**
 * Create a new IronBee Parsed Transaction.
 *
 * @param[in] ib_engine The IronBee engine that will manage the transaction.
 * @param[out] transaction The new transaction to be created.
 * @param[in] user_data A void* that is carried by the transaction for
 *            the user. It is never accessed or free'ed by this
 *            code. See ib_parsed_tx_get_user_data for accessing @a user_data.
 *
 * @returns IB_OK on success or other status on failure.
 */
DLL_PUBLIC ib_status_t ib_parsed_tx_create(ib_engine_t *ib_engine,
                                           ib_parsed_tx_t **transaction,
                                           void *user_data);

/**
 * Signal that the transaction has begun.
 *
 * Any deferred allocation of resources is completed.
 *
 * @param[in,out] transaction The transaction.
 * @param[in] method The HTTP method.
 * @param[in] path The HTTP request path.
 * @param[in] version The HTTP version.
 *
 * @returns IB_OK.
 */
DLL_PUBLIC ib_status_t ib_parsed_tx_req_begin(
    ib_parsed_tx_t *transaction,
    ib_parsed_req_method_t *method,
    ib_parsed_req_path_t *path,
    ib_parsed_req_version_t *version);

/**
 * Process @a name and @a value as coming from the client.
 *
 * @param[in] transaction The transaction.
 * @param[in] name The name of the HTTP header.
 * @param[in] value The value of the HTTP header.
 *
 * @returns IB_OK or IB_EALLOC if the values cannot be copied.
 */
DLL_PUBLIC ib_status_t ib_parsed_tx_req_header(
    ib_parsed_tx_t *transaction,
    ib_parsed_header_name_t *name,
    ib_parsed_header_value_t *value);

/**
 * Process @a name and @a value as coming from the server.
 *
 * @param[in] transaction The transaction.
 * @param[in] name The name of the HTTP header.
 * @param[in] value The value of the HTTP header.
 *
 * @returns IB_OK or IB_EALLOC if the values cannot be copied.
 */
DLL_PUBLIC ib_status_t ib_parsed_tx_resp_header(
    ib_parsed_tx_t *transaction,
    ib_parsed_header_name_t *name,
    ib_parsed_header_value_t *value);

/**
 * Signal that the request portion of the transaction has completed.
 *
 * @param[in] transaction The transaction.
 *
 * @returns IB_OK.
 */
DLL_PUBLIC ib_status_t ib_parsed_tx_req_end(ib_parsed_tx_t *transaction);

/**
 * Signal that the response portion of the transaction has begun.
 *
 * @param[in] transaction The transaction.
 * @param[in] code HTTP Response code.
 * @param[in] msg HTTP Response message.
 *
 * @returns IB_OK.
 */
DLL_PUBLIC ib_status_t ib_parsed_tx_resp_begin(
    ib_parsed_tx_t *transaction,
    ib_parsed_resp_status_code_t *code,
    ib_parsed_resp_status_msg_t *msg);

/**
 * Handle a chunk of body data.
 * @param[in] transaction The transaction the data belongs to.
 * @param[in] data The data being sent.
 * @return IB_OK.
 */
DLL_PUBLIC ib_status_t ib_parsed_tx_resp_body(ib_parsed_tx_t *transaction,
                                              ib_parsed_data_t *data);

/**
 * Handle a chunk of body data.
 * @param[in] transaction The transaction the data belongs to.
 * @param[in] data The data being sent.
 * @return IB_OK.
 */
DLL_PUBLIC ib_status_t ib_parsed_tx_req_body(ib_parsed_tx_t *transaction,
                                             ib_parsed_data_t *data);

/**
 * Signal that the response portion of the transaction has begun.
 *
 * @param[in] transaction The transaction.
 *
 * @returns IB_OK.
 */
DLL_PUBLIC ib_status_t ib_parsed_tx_resp_end(ib_parsed_tx_t *transaction);

/**
 * The trailer version of ib_parsed_tx_res_header.
 *
 * @see ib_parsed_tx_res_header.
 */
DLL_PUBLIC ib_status_t ib_parsed_tx_req_trailer(
    ib_parsed_tx_t *transaction,
    ib_parsed_trailer_name_t *name,
    ib_parsed_trailer_value_t *value);

/**
 * The trailer version of ib_parsed_tx_resp_header.
 *
 * @see ib_parsed_tx_resp_header.
 */
DLL_PUBLIC ib_status_t ib_parsed_tx_resp_trailer(
    ib_parsed_tx_t *transaction,
    ib_parsed_trailer_name_t *name,
    ib_parsed_trailer_value_t *value);


/**
 * @param[in] transaction The transaction that contains the user data pointer.
 * @returns The user data pointer in the @a transcation structure.
 */
DLL_PUBLIC void* ib_parsed_tx_get_user_data(const ib_parsed_tx_t *transaction);

/**
 * Destroy a transaction, releasing any held resources.
 *
 * @param[in,out] transaction The transaction that will no longer be valid after
 *                this returns.
 * @returns IB_OK.
 */
DLL_PUBLIC ib_status_t ib_parsed_tx_destroy(const ib_parsed_tx_t *transaction);

#ifdef __cplusplus
} // extern "C"
#endif
#endif // _IB_PARSED_CONTENT_H_
