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

#ifndef _IB_TYPES_H_
#define _IB_TYPES_H_

/**
 * @file
 * @brief IronBee - Type Definitions
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

#include <sys/types.h>
#include <unistd.h>

#include <netinet/in.h>
#include <arpa/inet.h>


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeUtilTypes Type Definitions
 * @ingroup IronBeeUtil
 * @{
 */

/* Type definitions. */
typedef struct ib_mpool_t ib_mpool_t;
typedef struct ib_mpool_buffer_t ib_mpool_buffer_t;

typedef struct ib_dso_t ib_dso_t;
typedef void ib_dso_sym_t;
typedef struct ib_hash_t ib_hash_t;
typedef uint32_t ib_ftype_t;
typedef uint32_t ib_flags_t;
typedef uint64_t ib_flags64_t;
typedef intmax_t ib_num_t;
typedef uintmax_t ib_unum_t;
typedef struct ib_cfgmap_t ib_cfgmap_t;
typedef struct ib_cfgmap_init_t ib_cfgmap_init_t;
typedef struct ib_field_t ib_field_t;
typedef struct ib_field_val_t ib_field_val_t;
typedef struct ib_bytestr_t ib_bytestr_t;
typedef struct ib_stream_t ib_stream_t;
typedef struct ib_sdata_t ib_sdata_t;

/** Generic function pointer type. */
typedef void (*ib_void_fn_t)(void);


/** Status code. */
typedef enum ib_status_t {
    IB_OK,                      /**<  0: No error */
    IB_DECLINED,                /**<  1: Declined execution */
    IB_EUNKNOWN,                /**<  2: Unknown error */
    IB_ENOTIMPL,                /**<  3: Not implemented (yet?) */
    IB_EINCOMPAT,               /**<  4: Incompatible with ABI version */
    IB_EALLOC,                  /**<  5: Could not allocate resources */
    IB_EINVAL,                  /**<  6: Invalid argument */
    IB_ENOENT,                  /**<  7: Entity does not exist */
    IB_ETRUNC,                  /**<  8: Buffer truncated, size limit reached */
    IB_ETIMEDOUT,               /**<  9: Operation timed out */
    IB_EAGAIN,                  /**< 10: Not ready, try again later */
} ib_status_t;

/**
 * @} IronBeeUtil
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_TYPES_H_ */
