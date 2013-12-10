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

#ifndef _IB_STRVAL_H_
#define _IB_STRVAL_H_

/**
 * @file
 * @brief IronBee --- String / value Functions
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include <ironbee/list.h>
#include <ironbee/mpool.h>
#include <ironbee/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeUtilStringValue String/Value mapping functions
 * @ingroup IronBeeUtil
 *
 * Code related to parsing and interpreting string / value pairs
 *
 * @{
 */

/** String key/numeric value pair */
struct ib_strval_t {
    const char        *str;         /**< String "key" */
    uint64_t           val;         /**< Numeric value */
};
typedef struct ib_strval_t ib_strval_t;

#define IB_STRVAL_MAP(name) ib_strval_t name[]
#define IB_STRVAL_PAIR(str, val) { str, val }
#define IB_STRVAL_PAIR_LAST { NULL, 0 }

/** String key/pointer value pair */
struct ib_strval_ptr_t {
    const char        *str;         /**< String "key" */
    const void        *val;         /**< Pointer to some value */
};
typedef struct ib_strval_ptr_t ib_strval_ptr_t;

#define IB_STRVAL_PTR_MAP(name) ib_strval_ptr_t name[]
#define IB_STRVAL_PTR_PAIR(str, ptr) { str, ptr }
#define IB_STRVAL_PTR_PAIR_LAST { NULL, NULL }

/** String key/data pair */
struct ib_strval_data_t {
    const char        *str;         /**< String "key" */
    const char         data[0];     /**< Data portion */
};
typedef struct ib_strval_data_t ib_strval_data_t;

#define IB_STRVAL_DATA_MAP(type, name) type name[]
#define IB_STRVAL_DATA_PAIR(str, ...) { str, {__VA_ARGS__} }
#define IB_STRVAL_DATA_PAIR_LAST(...) { NULL, {__VA_ARGS__} }

/**
 * Lookup a name/value pair mapping
 *
 * @param[in] map String / value mapping
 * @param[in] str String to lookup in @a map
 * @param[out] pval Matching value
 *
 * @returns Status code:
 *  - IB_OK: All OK,
 *  - IB_ENOENT: @a str not found in @a map
 *  - IB_EINVAL: One or more of the pointer parameters is NULL
 */
ib_status_t DLL_PUBLIC ib_strval_lookup(
    const ib_strval_t  *map,
    const char         *str,
    uint64_t           *pval);

/**
 * Loop through all elements in a strval map
 *
 * @param[in] map strval map
 * @param[in] rec strval record
 */
#define IB_STRVAL_LOOP(map, rec)                        \
    for ((rec) = (map); (rec)->str != NULL; ++(rec))

/**
 * Lookup a name/pointer pair mapping
 *
 * @param[in] map String / pointer mapping
 * @param[in] str String to lookup in @a map
 * @param[out] pptr Matching pointer
 *
 * @returns Status code:
 *  - IB_OK: All OK,
 *  - IB_ENOENT: @a str not found in @a map
 *  - IB_EINVAL: One or more of the pointer parameters is NULL
 */
ib_status_t DLL_PUBLIC ib_strval_ptr_lookup(
    const ib_strval_ptr_t *map,
    const char            *str,
    const void            *pptr);

/**
 * Lookup a name/data pair mapping
 *
 * @param[in] map String / data mapping
 * @param[in] rec_size Size of @a map records.
 * @param[in] str String to lookup in @a map
 * @param[out] pptr Pointer to data portion
 *
 * @returns Status code:
 *  - IB_OK: All OK,
 *  - IB_ENOENT: @a str not found in @a map
 *  - IB_EINVAL: One or more of the pointer parameters is NULL
 */
ib_status_t DLL_PUBLIC ib_strval_data_lookup(
    const ib_strval_data_t *map,
    size_t                  rec_size,
    const char             *str,
    const void             *pptr);

/**
 * Lookup a name/data pair mapping -- macro version
 *
 * @param[in] map String / data mapping (cast to const ib_strval_data_t *)
 * @param[in] type Type of the @a map records
 * @param[in] str String to lookup in @a map
 * @param[out] pptr Pointer to data portion
 *
 * @returns Status code:
 *  - IB_OK: All OK,
 *  - IB_ENOENT: @a str not found in @a map
 *  - IB_EINVAL: One or more of the pointer parameters is NULL
 */
#define IB_STRVAL_DATA_LOOKUP(map, type, str, pptr)       \
    ib_strval_data_lookup(                                \
        (const ib_strval_data_t *)map,                    \
        sizeof(type),                                     \
        str,                                              \
        pptr)

/**
 * @} IronBeeUtilStringValue
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_STRVAL_H_ */
