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

#ifndef _IB_PARSED_CONTENT_H_
#define _IB_PARSED_CONTENT_H_

/**
 * @file
 * @brief IronBee --- Interface for handling parsed content.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include <ironbee/bytestr.h>
#include <ironbee/field.h>
#include <ironbee/mpool.h>
#include <ironbee/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeParsedContent Parsed Content
 * @ingroup IronBeeEngine
 *
 * API For passing parsed or partially parsed content to the IronBee Engine.
 *
 * @{
 */

/**
 * A link list element representing the HTTP header.
 */
typedef struct ib_parsed_header_t {
    ib_bytestr_t *name;  /**< Name. */
    ib_bytestr_t *value; /**< Value the name describes. */
    struct ib_parsed_header_t *next; /**< Next element. */
} ib_parsed_header_t;

/**
 * A list wrapper of @ref ib_parsed_header_t.
 *
 * This is used to quickly build a new list. Afterwards the resultant list
 * can be treated as a simple linked list terminated with next==NULL.
 */
typedef struct ib_parsed_headers_t {
    ib_mpool_t *mpool;                      /**< Pool to allocate elements. */
    ib_parsed_header_t *head; /**< Head of the list. */
    ib_parsed_header_t *tail; /**< Tail of the list. */
    size_t size;                            /**< Size of the list. */
} ib_parsed_headers_t;

/**
 * A structure representing the parsed HTTP request line.
 */
typedef struct ib_parsed_req_line_t {
    ib_bytestr_t *raw;      /**< Raw HTTP request line */
    ib_bytestr_t *method;   /**< HTTP method */
    ib_bytestr_t *uri;      /**< HTTP URI */
    ib_bytestr_t *protocol; /**< HTTP protocol/version */
} ib_parsed_req_line_t;

/**
 * A structure representing the parsed HTTP response line.
 */
typedef struct ib_parsed_resp_line_t {
    ib_bytestr_t *raw;      /**< Raw HTTP response line */
    ib_bytestr_t *protocol; /**< HTTP protocol/version */
    ib_bytestr_t *status;   /**< HTTP status code */
    ib_bytestr_t *msg;      /**< HTTP status message */
} ib_parsed_resp_line_t;

/**
 * Construct a HTTP header (or trailer) object.
 *
 * This calloc's @a **header from @a mp.
 * Then @a mp is stored in @a **header so that all future list elements
 * are allocated from the same memory pool and all released when the pool
 * is released.
 *
 * @param[out] header The header object that will be constructed.
 * @param[in] mp Memory pool to allocate from.
 * @returns IB_OK or IB_EALLOC if mp could not allocate memory.
 */
ib_status_t DLL_PUBLIC ib_parsed_headers_create(
    ib_parsed_headers_t **header,
    ib_mpool_t *mp
);

/**
 * Link the arguments to a new list element and append it to this list.
 *
 * It is important to note that the arguments are linked to the list,
 * not copied. If you have a mutable buffer you must copy the values,
 * preferably out of @a mp. If this is done, then all related memory
 * will be released when the list elements allocated out of @a mp are
 * released.
 *
 * @param[out] header The list the the header object will be stored in.
 * @param[in] name The char* that will be stored as the start of the name.
 * @param[in] name_len The length of the string starting at @a name.
 * @param[in] value The char* that will be stored as the start of the value.
 * @param[in] value_len The length of the string starting at @a value.
 * @returns IB_OK on success. IB_EALLOC if the list element could not be
 *          allocated.
 */
ib_status_t DLL_PUBLIC ib_parsed_headers_add(
    ib_parsed_headers_t *header,
    const char *name,
    size_t name_len,
    const char *value,
    size_t value_len);


/**
 * Append the @a tail list to the @a head list.
 *
 * This modifies the @a head list but leaves the @a tail list untouched.
 * However, the @a tail list should not be used as it may be indirectly
 * appended to by calls to ib_parsed_headers_append or
 * ib_parsed_headers_add on @a head.
 */
ib_status_t DLL_PUBLIC ib_parsed_headers_append(
    ib_parsed_headers_t *head,
    const ib_parsed_headers_t *tail);

/**
 * Create a struct to link the response line components.
 *
 * Notice that this creates a struct that links the input char*
 * components. Be sure to call the relevant *_notify(...) function to
 * send this data to the IronBee Engine before the buffer in which the
 * arguments reside is invalidated.
 *
 * @note The @a raw response line can be NULL if it is not available. If
 * available, the @a status and @a msg parameters should be offsets
 * into the @a raw data if possible.
 *
 * @note The @a msg parameter may be NULL if no message is specified.
 *
 * @param[out] line The resultant object will be stored here.
 * @param[in] mp Memory pool to allocate from.
 * @param[in] raw The raw HTTP response line (NULL if not available)
 * @param[in] raw_len The length of @a raw.
 * @param[in] protocol The HTTP protocol.
 * @param[in] protocol_len The length of @a protocol.
 * @param[in] status The HTTP status code.
 * @param[in] status_len The length of @a status.
 * @param[in] msg The message describing @a status code.
 * @param[in] msg_len The length of @a msg.
 * @returns IB_OK or IB_EALLOC.
 */
ib_status_t DLL_PUBLIC ib_parsed_resp_line_create(
    ib_parsed_resp_line_t **line,
    ib_mpool_t *mp,
    const char *raw,
    size_t raw_len,
    const char *protocol,
    size_t protocol_len,
    const char *status,
    size_t status_len,
    const char *msg,
    size_t msg_len);

/**
 * Create a struct to link the request line components.
 *
 * Notice that this creates a struct that links the input char*
 * components. Be sure to call the relevant *_notify(...) function to
 * send this data to the IronBee Engine before the buffer in which the
 * arguments reside is invalidated.
 *
 * @note The @a raw request line can be NULL if it is not available. If
 * available, the @a method, @a uri and @a protocol parameters should be
 * offsets into the @a raw data if possible.
 *
 * @note The @a protocol parameter should be NULL for HTTP/0.9 requests.
 *
 * @param[out] line The resultant object is placed here.
 * @param[in] mp Memory pool to allocate from.
 * @param[in] raw The raw HTTP response line (NULL if not available)
 * @param[in] raw_len The length of @a raw.
 * @param[in] method The method.
 * @param[in] method_len The length of @a method.
 * @param[in] uri The uri component of the request.
 * @param[in] uri_len The length of @a uri.
 * @param[in] protocol The HTTP protocol/version.
 * @param[in] protocol_len The length of @a protocol.
 * @returns IB_OK or IB_EALLOC.
 */
ib_status_t DLL_PUBLIC ib_parsed_req_line_create(
    ib_parsed_req_line_t **line,
    ib_mpool_t *mp,
    const char *raw,
    size_t raw_len,
    const char *method,
    size_t method_len,
    const char *uri,
    size_t uri_len,
    const char *protocol,
    size_t protocol_len);

/**
 * @} IronBeeParsedContent
 */

#ifdef __cplusplus
} // extern "C"
#endif
#endif // _IB_PARSED_CONTENT_H_
