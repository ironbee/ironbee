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

#ifndef _IB_JSON_H_
#define _IB_JSON_H_

/**
 * @file
 * @brief IronBee --- JSON Utility Functions
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/list.h>
#include <ironbee/mm.h>
#include <ironbee/types.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>

#include <sys/types.h>

#include <yajl/yajl_gen.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeUtilJson Utility JSON Functions
 * @ingroup IronBeeUtil
 *
 * Code to encode and decode JSON
 *
 * @{
 */


/**
 * Decode a JSON encoded buffer into a list of IronBee fields (ex version)
 *
 * @param[in] mm Memory manager to use for allocations
 * @param[in] data_in Input JSON buffer
 * @param[in] dlen_in Length of data in @a data_in
 * @param[out] list_out List of IronBee fields
 * @param[out] error Pointer to error string (or NULL)
 *
 * @returns Status code:
 *  - IB_OK - All OK
 *  - IB_EINVAL - Decoding errors
 */
ib_status_t DLL_PUBLIC ib_json_decode_ex(
    ib_mm_t         mm,
    const uint8_t  *data_in,
    size_t          dlen_in,
    ib_list_t      *list_out,
    const char    **error
)
NONNULL_ATTRIBUTE(2, 4, 5);

/**
 * Decode a JSON encoded buffer into a list of IronBee fields
 *
 * @param[in] mm Memory manager to use for allocations
 * @param[in] in Input JSON buffer (NUL terminated string)
 * @param[out] list_out List of IronBee fields
 * @param[out] error Pointer to error string (or NULL)
 *
 * @returns Status code:
 *  - IB_OK - All OK
 *  - IB_EINVAL - Decoding errors
 */
ib_status_t DLL_PUBLIC ib_json_decode(
    ib_mm_t      mm,
    const char  *in,
    ib_list_t   *list_out,
    const char **error
)
NONNULL_ATTRIBUTE(2, 3, 4);

/**
 * Encode an IronBee list into a JSON buffer
 *
 * @param[in] mm Memory manager to use for allocations
 * @param[in] list List of IB fields to encode
 * @param[in] pretty Enable "pretty" JSON (if supported by library)
 * @param[in] obuf Output buffer (as a nul-terminated string)
 * @param[in] olen Length of @a obuf
 *
 * @returns IronBee status code
 */
ib_status_t DLL_PUBLIC ib_json_encode(
    ib_mm_t           mm,
    const ib_list_t  *list,
    bool              pretty,
    char            **obuf,
    size_t           *olen
)
NONNULL_ATTRIBUTE(2, 4, 5);

/**
 * Allocate a YAJL allocation functio struct and populate it using @a mm.
 *
 * Using this to create a yajl_alloc_funcs structure will cause
 * subsequently created yajl handles to be allocated out of, and perform
 * their allocations out of, @a mm. Thus, any generated JSON will have the
 * lifetime of mm.
 *
 * @param[out] funcs Function structure.
 * @param[in] mm The memory manager to use for all allocations.
 */
ib_status_t DLL_PUBLIC ib_json_yajl_alloc_create(
    yajl_alloc_funcs **funcs,
    ib_mm_t            mm
)
NONNULL_ATTRIBUTE(1);

/**
 * Allocate a yajl generation handle that is uses a memory manager.
 *
 * This is very similar to ib_json_yajl_alloc_create() but also
 * registers yajl_gen_free() with the memory manager.
 *
 * @param[out] handle The handle created out of the memory manager.
 * @param[in] mm Memory manager to use for JSON generation.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC On an allocation error.
 *
 * @sa ib_json_yajl_alloc_create()
 */
ib_status_t DLL_PUBLIC ib_json_yajl_gen_create(
    yajl_gen *handle,
    ib_mm_t   mm
)
NONNULL_ATTRIBUTE(1);


/**
 * @} IronBeeUtilJson
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_JSON_H_ */
