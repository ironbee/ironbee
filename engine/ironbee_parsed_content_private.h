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
 * Empty forward declaration to let us use the typedef as a member of the
 * struct ib_parsed_name_value_pair_list_element_t.
 */
typedef struct ib_parsed_name_value_pair_list_element_t
    ib_parsed_name_value_pair_list_element_t;

/**
 * An opaque link list element representing HTTP headers.
 */
typedef struct ib_parsed_name_value_pair_list_element_t {
    const char *name;
    size_t name_len;
    const char *value;
    size_t value_len;
    ib_parsed_name_value_pair_list_element_t *next; /**< The next header. */
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
    ib_bytestr_t *method;  /**< HTTP Method. */
    ib_bytestr_t *path;    /**< Path request method is against. */
    ib_bytestr_t *version; /**< HTTP Version. */
};

/**
 * The first line returned to a user from the server.
 *
 * This is typedef'ed to useful types inparsed_content.h.
 */
struct ib_parsed_resp_line_t {
    ib_bytestr_t *code; /**< The status code. */
    ib_bytestr_t *msg;  /**< The message to the user. */
};

/**
 * A pointer into an existing buffer of data.
 */
struct ib_parsed_data_t {
    const char *buffer;
    ib_unum_t start;
    ib_unum_t offset;
};

#ifdef __cplusplus
} // extern "C"
#endif
#endif // _IB_PARSED_CONTENT_PRIVATE_H_
