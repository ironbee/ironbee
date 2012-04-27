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
 * @brief IronBee &mdash; Interface for handling parsed content.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

/**
 * @defgroup IronBeeParsedContent Parsed Content
 * @ingroup IronBeeEngine
 *
 * API For passing parsed or partially parsed content to the IronBee Engine.
 *
 * @{
 */

#include <ironbee/bytestr.h>
#include <ironbee/field.h>
#include <ironbee/types.h>
#include <ironbee/mpool.h>

struct ib_tx_t;

/**
 * A link list element representing HTTP headers.
 */
typedef struct ib_parsed_name_value_pair_list_t {
    ib_bytestr_t *name;  /**< Name. */
    ib_bytestr_t *value; /**< Value the name describes. */
    struct ib_parsed_name_value_pair_list_t *next; /**< Next element. */
} ib_parsed_name_value_pair_list_t;

/**
 * A list wrapper of ib_Parsed_name_value_pair_list_t.
 *
 * This is used to quickly build a new list. Afterwards the resultant list
 * can be treated as a simple linked list terminated with next==NULL.
 */
typedef struct ib_parsed_name_value_pair_list_wrapper_t {
    ib_mpool_t *mpool;                      /**< Pool to allocate elements. */
    ib_parsed_name_value_pair_list_t *head; /**< Head of the list. */
    ib_parsed_name_value_pair_list_t *tail; /**< Tail of the list. */
    size_t size;                            /**< Size of the list. */
} ib_parsed_name_value_pair_list_wrapper_t;


typedef struct ib_parsed_name_value_pair_list_wrapper_t
    ib_parsed_header_wrapper_t;
typedef struct ib_parsed_name_value_pair_list_wrapper_t
    ib_parsed_trailer_wrapper_t;

/**
 * The first line in an HTTP request.
 *
 * This is typedef'ed to useful types in parsed_content.h.
 */
typedef struct ib_parsed_req_line_t {
    ib_bytestr_t *method;  /**< HTTP Method. */
    ib_bytestr_t *path;    /**< Path request method is against. */
    ib_bytestr_t *version; /**< HTTP Version. */
} ib_parsed_req_line_t;

/**
 * The first line returned to a user from the server.
 *
 * This is typedef'ed to useful types in parsed_content.h.
 */
typedef struct ib_parsed_resp_line_t {
    ib_bytestr_t *code; /**< The status code. */
    ib_bytestr_t *msg;  /**< The message to the user. */
} ib_parsed_resp_line_t;

/**
 * An opaque list representation of a header list.
 */
typedef struct ib_parsed_name_value_pair_list_t ib_parsed_header_t;

/**
 * A trailer is structurally identical to an ib_parsed_header_t.
 */
typedef struct ib_parsed_name_value_pair_list ib_parsed_trailer_t;


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
 * Construct a headers (or trailers) object.
 *
 * This calloc's @a **headers from @a mp.
 * Then @a mp is stored in @a **headers so that all future list elements
 * are allocated from the same memory pool and all released when the pool
 * is released.
 *
 * @param[out] headers The headers object that will be constructed.
 * @param[in] tx The transaction that will allocate the headers object.
 * @returns IB_OK or IB_EALLOC if mp could not allocate memory.
 */
DLL_PUBLIC ib_status_t ib_parsed_name_value_pair_list_wrapper_create(
    ib_parsed_name_value_pair_list_wrapper_t **headers,
    struct ib_tx_t *tx);

/**
 * Link the arguments to a new list element and append it to this list.
 *
 * It is important to note that the arguments are linked to the list,
 * not copied. If you have a mutable buffer you must copy the values,
 * preferably out of @a mp. If this is done, then all related memory
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
DLL_PUBLIC ib_status_t ib_parsed_name_value_pair_list_add(
    ib_parsed_name_value_pair_list_wrapper_t *headers,
    const char *name,
    size_t name_len,
    const char *value,
    size_t value_len);

/**
 * Apply @a callback to each name-value header pair in @a headers.
 *
 * This function may also be used for ib_parsed_trailer_t* objects as
 * they are typedefs of the same struct.
 *
 * This function will forward the @a user_data value to the callback for
 * use by the user's code.
 *
 * This function will prematurely terminate iteration if @a callback does not
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
    ib_parsed_name_value_pair_list_wrapper_t *headers,
    ib_parsed_tx_each_header_callback callback,
    void* user_data);

/**
 * Create a struct to link the response line components.
 *
 * Notice that this creates a struct that links the input char*
 * components. Be sure to call the relevant *_notify(...) function to
 * send this data to the IronBee Engine before the buffer in which the
 * arguments reside is invalidated.
 *
 * @param[in] tx The transaction whose memory pool will be used.
 * @param[out] line The resultant object will be stored here.
 * @param[in] code The HTTP status code.
 * @param[in] code_len The length of @a code.
 * @param[in] msg The message describing @a code.
 * @param[in] msg_len The length of @a msg.
 * @returns IB_OK or IB_EALLOC.
 */
DLL_PUBLIC ib_status_t ib_parsed_resp_line_create(
    struct ib_tx_t *tx,
    ib_parsed_resp_line_t **line,
    const char *code,
    size_t code_len,
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
 * @param[in] tx The transaction whose memory pool is used.
 * @param[out] line The resultant object is placed here.
 * @param[in] method The method.
 * @param[in] method_len The length of @a method.
 * @param[in] path The path component of the request.
 * @param[in] path_len The length of @a path.
 * @param[in] version The HTTP version.
 * @param[in] version_len The length of @a version.
 * @returns IB_OK or IB_EALLOC.
 */
DLL_PUBLIC ib_status_t ib_parsed_req_line_create(
    struct ib_tx_t *tx,
    ib_parsed_req_line_t **line,
    const char *method,
    size_t method_len,
    const char *path,
    size_t path_len,
    const char *version,
    size_t version_len);

/**
 * Append the @a tail list to the @a head list.
 *
 * This modifies the @a head list but leaves the @a tail list untouched.
 * However, the @a tail list should not be used as it may be indirectly
 * appended to by calls to ib_parsed_name_value_pair_list_append or
 * ib_parsed_name_value_pair_list_add on @a head.
 */
DLL_PUBLIC ib_status_t ib_parsed_name_value_pair_list_append(
    ib_parsed_name_value_pair_list_wrapper_t *head,
    const ib_parsed_name_value_pair_list_wrapper_t *tail);
/**
 * @} IronBeeParsedContent
 */

#ifdef __cplusplus
} // extern "C"
#endif
#endif // _IB_PARSED_CONTENT_H_
