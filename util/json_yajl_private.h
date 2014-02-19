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

#ifndef _IB_JSON_YAJL_PRIVATE_H_
#define _IB_JSON_YAJL_PRIVATE_H_

/**
 * @file
 * @brief IronBee --- Private Utility Declarations for JSON / YAJL
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include <ironbee/mm.h>
#include <ironbee/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** JSON/YAJL Common memory allocation context */
typedef struct {
    ib_mm_t         mm;     /**< Memory manager to use */
    ib_status_t     status; /**< IB status for returning errors */
} json_yajl_alloc_context_t;

/** malloc()-ish clone for YAJL / JSON
 *
 * @param[in] ctx Allocation context
 * @param[in] size Size of allocation
 *
 * @returns Pointer to allocated block
 */
void *json_yajl_alloc(
    void   *ctx,
    size_t  size);

/** realloc()-ish clone for YAJL / JSON
 *
 * @param[in] ctx Allocation context
 * @param[in] ptr Pointer to memory to re-allocate
 * @param[in] size Size of allocation
 *
 * @returns Pointer to (perhaps newly) allocated block
 */
void *json_yajl_realloc(
    void   *ctx,
    void   *ptr,
    size_t  size);

/** free()-ish clone for YAJL / JSON
 *
 * @param[in] ctx Allocation context
 * @param[in] ptr Pointer to memory to free
 */
void json_yajl_free(
    void *ctx,
    void *ptr);


#ifdef __cplusplus
}
#endif

#endif /* IB_JSON_YAJL_PRIVATE_H_ */
