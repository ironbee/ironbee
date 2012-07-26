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

#ifndef _IB_RADIX_PRIVATE_H_
#define _IB_RADIX_PRIVATE_H_

/**
 * @file
 * @brief IronBee &mdash; Private Radix Tree Declarations
 *
 * This file only exists to allow radix tree unit tests to access internal
 * data.
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Pablo Rincon <pablo.rincon.crespo@gmail.com>
 */

#include <ironbee/radix.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Prefix for radix nodes
 */
struct ib_radix_prefix_t {
    uint8_t *rawbits;
    uint8_t prefixlen;
};

/**
 * Radix node structure
 */
struct ib_radix_node_t {
    ib_radix_prefix_t *prefix;

    struct ib_radix_node_t *zero;
    struct ib_radix_node_t *one;

    void *data;
};

/**
 * Radix tree structure
 */
struct ib_radix_t {
    ib_radix_node_t *start;

    ib_radix_update_fn_t update_data;
    ib_radix_print_fn_t  print_data;
    ib_radix_free_fn_t   free_data;

    size_t data_cnt;

    ib_mpool_t *mp;
};

/**
 * Matching functions type helper
 */
enum {
    IB_RADIX_PREFIX,
    IB_RADIX_CLOSEST,
};


#ifdef __cplusplus
}
#endif

#endif /* IB_RADIX_PRIVATE_H_ */

