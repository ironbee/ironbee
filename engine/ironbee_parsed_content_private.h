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

#ifndef _IB_PARSED_CONTENT_PRIVATE_H_
#define _IB_PARSED_CONTENT_PRIVATE_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * @brief IronBee interface for handling parsed content.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee_config_auto.h>
#include <ironbee/bytestr.h>
#include <ironbee/field.h>
#include <ironbee/types.h>
#include <ironbee/mpool.h>

/* Pull in partially defined headers we will more fully define in this .h. */
#include <ironbee/parsed_content.h>

/**
 * An opaque link list element representing HTTP headers.
 */
typedef struct ib_parsed_name_value_pair_list_element_t {
    const char *name;
    size_t name_len;
    const char *value;
    size_t value_len;
    struct ib_parsed_name_value_pair_list_element_t *next; /**< The next header. */
} ib_parsed_name_value_pair_list_element_t;

/**
 * An opaque link list element representing HTTP headers.
 *
 * This is typedef'ed to useful types in parsed_content.h.
 */
struct ib_parsed_name_value_pair_list_t {
    ib_parsed_name_value_pair_list_element_t *head;
    ib_parsed_name_value_pair_list_element_t *tail;
    ib_unum_t size;
    ib_mpool_t *mp; /**< The memory pool to allocate all elements from. */
};

/**
 * The first line in an HTTP request.
 *
 * This is typedef'ed to useful types in parsed_content.h.
 */
struct ib_parsed_req_line_t {
    const char *method;  /**< HTTP Method. */
    size_t method_len;   /**< Length of method. */
    const char *path;    /**< Path request method is against. */
    size_t path_len;     /**< Length of path. */
    const char *version; /**< HTTP Version. */
    size_t version_len;  /**< Length of version. */
};

/**
 * The first line returned to a user from the server.
 *
 * This is typedef'ed to useful types in parsed_content.h.
 */
struct ib_parsed_resp_line_t {
    const char *code; /**< The status code. */
    size_t code_len;  /**< Length of code. */
    const char *msg;  /**< The message to the user. */
    size_t msg_len;   /**< Length of the msg. */
};

/**
 * A pointer into an existing buffer of data.
 */
struct ib_parsed_data_t {
    const char *buffer;
    size_t start;
    size_t offset;
};

#ifdef __cplusplus
} // extern "C"
#endif
#endif // _IB_PARSED_CONTENT_PRIVATE_H_
