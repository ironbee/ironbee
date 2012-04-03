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
 *
 * @defgroup IronBee IronBee
 * @{
 * @defgroup IronBeeEngine IronBee Engine
 * @{
 * @defgroup IronBeeParsedContent Parsed Content
 *
 * API For passing parsed or partially parsed content to the IronBee Engine.
 *
 * @{
 */

#include <ironbee/build.h>
#include <ironbee_config_auto.h>
#include <ironbee/bytestr.h>
#include <ironbee/engine.h>
#include <ironbee/field.h>
#include <ironbee/types.h>

/**
 * An opaque representation of the first line of an HTTP request.
 */
typedef struct ib_parsed_req_line_t ib_parsed_req_line_t;

/**
 * An opaque list representation of a header list.
 */
typedef struct ib_parsed_name_value_pair_list_t ib_parsed_header_t;

/**
 * A trailer is structurally identical to an ib_parsed_header_t.
 */
typedef struct ib_parsed_name_value_pair_list ib_parsed_trailer_t;

/**
 * Parsed response line.
 *
 * This is the first line in the server response to the user.
 */
typedef struct ib_parsed_resp_line_t ib_parsed_resp_line_t;

/**
 * A pointer into a read-only buffer with a begin and offset value.
 */
typedef struct ib_parsed_data_t ib_parsed_data_t;

/**
 * Opaque transaction representation.
 */
typedef struct ib_parsed_tx_t {
    ib_engine_t *ib_engine; /**< The engine handling this transaction. */
} ib_parsed_tx_t;

/**
 * Callback for iterating through a list of headers.
 * IB_OK must be returned. Otherwise the loop will terminate prematurely.
 */
typedef ib_status_t (*ib_parsed_tx_each_header_callback)(const char *name,
                                                         size_t name_len,
                                                         const char *value,
                                                         size_t value_len,
                                                         void* user_data);

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
                                           ib_parsed_tx_t **transaction);

/**
 * Signal that the transaction has begun.
 *
 * Any deferred allocation of resources is completed.
 *
 * @param[in,out] transaction The transaction.
 * @param[in] line The HTTP status line.
 *
 * @returns IB_OK.
 */
DLL_PUBLIC ib_status_t ib_parsed_tx_notify_req_begin(
    ib_parsed_tx_t *transaction,
    ib_parsed_req_line_t *req_line);

/**
 * Process @a name and @a value as coming from the client.
 *
 * @param[in] transaction The transaction.
 * @param[in] headers List of HTTP headers.
 *
 * @returns IB_OK or IB_EALLOC if the values cannot be copied.
 */
DLL_PUBLIC ib_status_t ib_parsed_tx_notify_req_header(
    ib_parsed_tx_t *transaction,
    ib_parsed_header_t *headers);

/**
 * Process @a name and @a value as coming from the server.
 *
 * @param[in] transaction The transaction.
 * @param[in] headers List of HTTP headers.
 *
 * @returns IB_OK or IB_EALLOC if the values cannot be copied.
 */
DLL_PUBLIC ib_status_t ib_parsed_tx_notify_resp_header(
    ib_parsed_tx_t *transaction,
    ib_parsed_header_t *headers);

/**
 * Signal that the request portion of the transaction has completed.
 *
 * @param[in] transaction The transaction.
 *
 * @returns IB_OK.
 */
DLL_PUBLIC ib_status_t ib_parsed_tx_notify_req_end(
    ib_parsed_tx_t *transaction);

/**
 * Signal that the response portion of the transaction has begun.
 *
 * @param[in] transaction The transaction.
 * @param[in] line The HTTP response line.
 *
 * @returns IB_OK.
 */
DLL_PUBLIC ib_status_t ib_parsed_tx_notify_resp_begin(
    ib_parsed_tx_t *transaction,
    ib_parsed_resp_line_t *line);

/**
 * Handle a chunk of body data.
 * @param[in] transaction The transaction the data belongs to.
 * @param[in] data The data being sent.
 * @return IB_OK.
 */
DLL_PUBLIC ib_status_t ib_parsed_tx_notify_resp_body(
    ib_parsed_tx_t *transaction,
    ib_parsed_data_t *data);

/**
 * Handle a chunk of body data.
 * @param[in] transaction The transaction the data belongs to.
 * @param[in] data The data being sent.
 * @return IB_OK.
 */
DLL_PUBLIC ib_status_t ib_parsed_tx_notify_req_body(
    ib_parsed_tx_t *transaction,
    ib_parsed_data_t *data);

/**
 * Signal that the response portion of the transaction has begun.
 *
 * @param[in] transaction The transaction.
 *
 * @returns IB_OK.
 */
DLL_PUBLIC ib_status_t ib_parsed_tx_notify_resp_end(
    ib_parsed_tx_t *transaction);

/**
 * The trailer version of ib_parsed_tx_res_header.
 *
 * @see ib_parsed_tx_res_header.
 */
DLL_PUBLIC ib_status_t ib_parsed_tx_notify_req_trailer(
    ib_parsed_tx_t *transaction,
    ib_parsed_trailer_t *trailers);

/**
 * The trailer version of ib_parsed_tx_resp_header.
 *
 * @see ib_parsed_tx_resp_header.
 */
DLL_PUBLIC ib_status_t ib_parsed_tx_notify_resp_trailer(
    ib_parsed_tx_t *transaction,
    ib_parsed_trailer_t *trailers);

/**
 * Destroy a transaction, releasing any held resources.
 *
 * @param[in,out] transaction The transaction that will no longer 
 *                be valid after this returns.
 * @returns IB_OK.
 */
DLL_PUBLIC ib_status_t ib_parsed_tx_destroy(const ib_parsed_tx_t *transaction);

/**
 * Construct a headers (or trailers) object.
 *
 * This calloc's @a **headers from @a mp. 
 * Then @a mp is stored in @a **headers so that all future list elements
 * are allocated from the same memory pool and all released when the pool
 * is released.
 *
 * @param[out] headers The headers object that will be constructed.
 * @param[in,out] mp The memory pool that will allcoate the headers object.
 * @returns IB_OK or IB_EALLOC if mp could not allocate memory.
 */
DLL_PUBLIC ib_status_t ib_parsed_header_create(ib_parsed_header_t **headers,
                                               ib_mpool_t *mp);

/**
 * Link the arguments to a new list element and append it to this list.
 *
 * It is important to note that the arguments are linked to the list, 
 * not copied. If you have a mutable buffer you must copy the values,
 * peferably out of @a mp. If this is done, then all related memory
 * will be released when the list elements allocated out of @a mp are
 * released.
 * 
 * @param[out] headers The list the the header object will be stored in.
 * @param[in] name The char* that will be stored as the start of the name.
 * @param[in] name_len The length of the string starting at @a name.
 * @param[in] value The char* that will be stored as the start of the value.
 * @param[in] value_len The length of the string starting at @a value.
 * @returns IB_OK on success. IB_EALLOC if the list element could not be
 *          allocated.
 */
DLL_PUBLIC ib_status_t ib_parsed_header_add(ib_parsed_header_t *headers,
                                            const char *name,
                                            size_t name_len,
                                            const char *value,
                                            size_t value_len);

/**
 * Return the size of the headers (or trailers) list.
 * @param[in] headers The list to extract the current size of.
 */
DLL_PUBLIC size_t ib_parsed_header_list_size(const ib_parsed_header_t *headers);

/**
 * Apply @a callback to each name-value header pair in @a headers.
 *
 * This function may also be used for ib_parsed_trailer_t* objects as 
 * they are typedefs of the same struct.
 *
 * This function will forward the @a user_data value to the callback for
 * use by the user's code.
 *
 * This function will prematurly terminate iteration if @a callback does not
 * return IB_OK. The last return code from @a callback is the return code
 * of this function.
 *
 * @param[in] headers The list to be iterated through.
 * @param[in] callback The function supplied by the caller to process the
 *            name-value pairs.
 * @param[in] user_data A pointer that is forwarded to the callback so
 *            the user can pass some context around.
 * @returns The last return code of @a callback. If @a callback returns
 *          a value that is not IB_OK iteration is prematurely terminated
 *          and that return code is returned.
 */
DLL_PUBLIC ib_status_t ib_parsed_tx_each_header(
    ib_parsed_header_t *headers,
    ib_parsed_tx_each_header_callback callback,
    void* user_data);

/**
 * @} IronBeeParsedContent
 * @} IronBeeEngine
 * @} IronBee
 */

#ifdef __cplusplus
} // extern "C"
#endif
#endif // _IB_PARSED_CONTENT_H_
