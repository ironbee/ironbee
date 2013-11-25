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
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

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
 * API for passing parsed or partially parsed content to the IronBee Engine.
 *
 * @{
 **/

/**
 * An HTTP header.
 **/
typedef struct ib_parsed_header_t {
    ib_bytestr_t              *name;  /**< Name. **/
    ib_bytestr_t              *value; /**< Value. **/
    struct ib_parsed_header_t *next;  /**< Next header. **/
} ib_parsed_header_t;

/**
 * A list of @ref ib_parsed_header_t.
 **/
typedef struct ib_parsed_headers_t {
    ib_mpool_t         *mpool; /**< Pool to allocate elements. **/
    ib_parsed_header_t *head;  /**< Head of the list. **/
    ib_parsed_header_t *tail;  /**< Tail of the list. **/
    size_t              size;  /**< Size of the list. **/
} ib_parsed_headers_t;

/**
 * HTTP Request Line.
 **/
typedef struct ib_parsed_req_line_t {
    ib_bytestr_t *raw;      /**< Raw HTTP request line **/
    ib_bytestr_t *method;   /**< HTTP method **/
    ib_bytestr_t *uri;      /**< HTTP URI **/
    ib_bytestr_t *protocol; /**< HTTP protocol/version **/
} ib_parsed_req_line_t;

/**
 * HTTP Response line.
 **/
typedef struct ib_parsed_resp_line_t {
    ib_bytestr_t *raw;      /**< Raw HTTP response line **/
    ib_bytestr_t *protocol; /**< HTTP protocol/version **/
    ib_bytestr_t *status;   /**< HTTP status code **/
    ib_bytestr_t *msg;      /**< HTTP status message **/
} ib_parsed_resp_line_t;

/**
 * Construct a @ref ib_parsed_headers_t.
 *
 * Constructs a header list.  Lifetime of list and elements will be equal to
 * that of @a mp.
 *
 * @param[out] headers The header list to create.
 * @param[in]  mp      Memory pool to allocate from.
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 **/
ib_status_t DLL_PUBLIC ib_parsed_headers_create(
    ib_parsed_headers_t **headers,
    ib_mpool_t           *mp
);

/**
 * Add a headers to @a headers.
 *
 * Currently the buffers @a name and @a value are copied.  Future versions
 * will likely change this to avoids copies.
 *
 * @param[in] headers   Headers to add to.
 * @param[in] name      Name of header.
 * @param[in] name_len  Length of @a name.
 * @param[in] value     Value of header.
 * @param[in] value_len Length of @ avalue.
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 **/
ib_status_t DLL_PUBLIC ib_parsed_headers_add(
    ib_parsed_headers_t *headers,
    const char          *name,
    size_t               name_len,
    const char          *value,
    size_t               value_len
);


/**
 * Append the @a tail list to the @a head list.
 *
 * This function does not directly modify @a tail.  However, future appends or
 * adds to @a head will also modify @a tail.  It is recommended that @a tail
 * not be used after calling this.
 *
 * @param[in] head Headers to append to.
 * @param[in] tail Headers to append.
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 **/
ib_status_t DLL_PUBLIC ib_parsed_headers_append(
    ib_parsed_headers_t       *head,
    const ib_parsed_headers_t *tail
);

/**
 * Create a parsed request line.
 *
 * Currently @a raw, @a method, @a uri, and @a protocol are copied.  Future
 * versions will likely change this to avoid copies.
 *
 * @note The @a protocol parameter should be NULL for HTTP/0.9 requests.
 *
 * @param[out] line         Constructed line.
 * @param[in]  mp           Memory pool to allocate from.
 * @param[in]  raw          Raw response line.  If NULL, will be constructed
 *                          from other arguments.
 * @param[in]  raw_len      Length of @a raw.
 * @param[in]  method       Method.  If NULL, empty string will be used.
 * @param[in]  method_len   Length of @a method.
 * @param[in]  uri The      URI.  If NULL, empty string will be used.
 * @param[in]  uri_len      Length of @a uri.
 * @param[in]  protocol     Protocol/version.  If NULL, empty string will be
 *                          used.
 * @param[in]  protocol_len Length of @a protocol.
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 **/
ib_status_t DLL_PUBLIC ib_parsed_req_line_create(
    ib_parsed_req_line_t **line,
    ib_mpool_t            *mp,
    const char            *raw,
    size_t                 raw_len,
    const char            *method,
    size_t                 method_len,
    const char            *uri,
    size_t                 uri_len,
    const char            *protocol,
    size_t                 protocol_len
);

/**
 * Create a parsed response line.
 *
 * Currently @a raw, @a protocol, @a status, and @a msg are copied.  Future
 * versions will likely change this to avoid copies.
 *
 * @param[out] line         Constructred line.
 * @param[in]  mp           Memory pool to allocate from.
 * @param[in]  raw          Raw response line.  If NULL, will be constructred
 *                          from other arguments.
 * @param[in]  raw_len      Length of @a raw.
 * @param[in]  protocol     Protocol.  If NULL, empty string will be used.
 * @param[in]  protocol_len Length of @a protocol.
 * @param[in]  status       Status code.  If NULL, empty string will be used.
 * @param[in]  status_len   Length of @a status.
 * @param[in]  msg          Message.  If NULL, empty string will be used.
 * @param[in]  msg_len      Length of @a msg.
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 **/
ib_status_t DLL_PUBLIC ib_parsed_resp_line_create(
    ib_parsed_resp_line_t **line,
    ib_mpool_t             *mp,
    const char             *raw,
    size_t                  raw_len,
    const char             *protocol,
    size_t                  protocol_len,
    const char             *status,
    size_t                  status_len,
    const char             *msg,
    size_t                  msg_len
);

/**
 * @} IronBeeParsedContent
 **/

#ifdef __cplusplus
} // extern "C"
#endif

#endif // _IB_PARSED_CONTENT_H_