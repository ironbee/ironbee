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

#ifndef _IB_RADIX_H_
#define _IB_RADIX_H_

/**
 * @file
 * @brief IronBee &mdash; Radix Utility Functions
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/types.h>
#include <ironbee/list.h>
#include <ironbee/field.h>     /* For ib_num_t */

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeUtilRadix Radix
 * @ingroup IronBeeUtil
 *
 * Datastructure to that maps bit string prefixes to arbitrary data.
 *
 * @{
 */

typedef struct ib_radix_t ib_radix_t;
typedef struct ib_radix_prefix_t ib_radix_prefix_t;
typedef struct ib_radix_node_t ib_radix_node_t;

typedef void (*ib_radix_update_fn_t)(ib_radix_node_t*, void*);
typedef void (*ib_radix_print_fn_t)(void *);
typedef void (*ib_radix_free_fn_t)(void *);

/**
 * Creates a new prefix instance
 *
 * @param prefix reference to a pointer that will link to the allocated prefix
 * @param pool memory pool that the allocation should use
 *
 * @returns Status code
 */
ib_status_t ib_radix_prefix_new(ib_radix_prefix_t **prefix,
                                ib_mpool_t *pool);

/**
 * Creates a new prefix instance
 *
 * @param prefix reference to a pointer that will link to the allocated prefix
 * @param rawbits sequence of bytes representing the key prefix
 * @param prefixlen size in bits with the len of the prefix
 * @param pool memory pool that the allocation should use
 *
 * @returns Status code
 */
ib_status_t ib_radix_prefix_create(ib_radix_prefix_t **prefix,
                                   uint8_t *rawbits,
                                   uint8_t prefixlen,
                                   ib_mpool_t *pool);

/**
 * creates a clone of the prefix instance
 *
 * @param orig pointer to the original prefix
 * @param new_prefix reference to a pointer to the allocated prefix
 * @param mp memory pool that the allocation should use
 *
 * @returns Status code
 */
ib_status_t ib_radix_clone_prefix(ib_radix_prefix_t *orig,
                                  ib_radix_prefix_t **new_prefix,
                                  ib_mpool_t *mp);

/**
 * destroy a prefix
 *
 * @param prefix to destroy
 * @param pool memory of the prefix
 *
 * @returns Status code
 */
ib_status_t ib_radix_prefix_destroy(ib_radix_prefix_t **prefix,
                                    ib_mpool_t *pool);


/**
 * Creates a new node instance
 *
 * @param node reference to a pointer that will link to the allocated node
 * @param pool memory pool that the allocation should use
 *
 * @returns Status code
 */
ib_status_t ib_radix_node_new(ib_radix_node_t **node,
                              ib_mpool_t *pool);

/**
 * creates a clone of the node instance
 *
 * @param orig pointer to the original node
 * @param new_node reference to a pointer that will link to the allocated node
 * @param mp memory pool that the allocation should use
 *
 * @returns Status code
 */
ib_status_t ib_radix_clone_node(ib_radix_node_t *orig,
                                ib_radix_node_t **new_node,
                                ib_mpool_t *mp);

/**
 * Destroy a node and its children (this includes prefix and userdata)
 *
 * @param radix the radix of the node
 * @param node the node to destroy
 * @param pool memory pool of the node
 *
 * @returns Status code
 */
ib_status_t ib_radix_node_destroy(ib_radix_t *radix,
                                  ib_radix_node_t **node,
                                  ib_mpool_t *pool);

/**
 * Creates a new radix tree registering functions to update, free and print
 * associated to each prefix it also register the memory pool it should use
 * for new allocations
 *
 * @param radix pointer to link the instance of the new radix
 * @param free_data pointer to the function that will be used to
 * free the userdata entries
 * @param update_data pointer to the function that knows how to update a node
 * with new user data
 * @param print_data pointer to a helper function that print_datas a userdata
 * @param pool memory pool that the allocation should use
 *
 * @returns Status code
 */
ib_status_t ib_radix_new(ib_radix_t **radix,
                         ib_radix_free_fn_t free_data,
                         ib_radix_print_fn_t print_data,
                         ib_radix_update_fn_t update_data,
                         ib_mpool_t *pool);

/*
 * Inserts a new user data associated to the prefix passed. The prefix is not
 * used, so developers are responsible to free that prefix
 * Keys can be of "any size" but this will be probably used for
 * CIDR data prefixes only (from 0 to 32 ~ 128 depending on IPv4
 * or IPv6 respectively)
 *
 * @param radix the radix of the node
 * @param prefix the prefix to use as index
 * @param prefix_data, the data to store under that prefix
 *
 * @returns Status code
 */
ib_status_t ib_radix_insert_data(ib_radix_t *radix,
                                 ib_radix_prefix_t *prefix,
                                 void *prefix_data);

/*
 * creates a clone of the tree, allocating memory from mp
 *
 * @param orig pointer to the original pool
 * @param new_pool reference to a pointer that will link to the allocated pool
 * @param mp memory pool that the allocation should use
 *
 * @returns Status code
 */
ib_status_t ib_radix_clone_radix(ib_radix_t *orig,
                                 ib_radix_t **new_radix,
                                 ib_mpool_t *mp);

/**
 * return the number of datas linked from the radix
 *
 * @param radix the radix of the node
 *
 * @returns Status code
 */
size_t ib_radix_elements(ib_radix_t *radix);

/*
 * Destroy the memory pool of a radix (warning: this usually includes itself)
 *
 * @param radix the radix to destroy
 *
 * @returns Status code
 */
ib_status_t ib_radix_destroy(ib_radix_t **radix);

/*
 * Function that return the data allocated to an exact prefix
 *
 * @param radix The radix tree
 * @param prefix the prefix we are searching
 * @param result reference to the pointer that will be linked to the data if any
 *
 * @returns Status code
 */
ib_status_t ib_radix_match_exact(ib_radix_t *radix,
                                 ib_radix_prefix_t *prefix,
                                 void *result);

/*
 * Function that return the data linked to an exact prefix if any. Otherwise
 * it will start falling backwards until it reach a immediate shorter prefix with
 * any data returning it. If no data is found on it's path it will return null.
 *
 * Example: insert data in 192.168.1.0/24
 * search with this function the data of 192.168.1.27
 * it will not have an exact match ending with .27, but walking backwards the
 * recursion, it will find data associated to 192.168.1.0/24
 *
 * @param radix The radix tree
 * @param prefix the prefix we are searching
 * @param result reference to the pointer that will be linked to the data if any
 *
 * @returns Status code
 */
ib_status_t ib_radix_match_closest(ib_radix_t *radix,
                                   ib_radix_prefix_t *prefix,
                                   void *result);

/*
 * Function that return a list of all the datas with a prefix that start like
 * the provided prefix arg
 *
 * Example: insert data in 192.168.1.27, as well as 192.168.1.28,
 * as well as 192.168.10.0/24 and 10.0.0.0/8 and then search 192.168.0.0/16
 * it should return a list containing all the datas except the associated to
 * 10.0.0.0/8
 *
 * @param radix The radix tree
 * @param prefix the prefix we are searching
 * @param rlist reference to the pointer that will be linked to the list, if any
 * @param mp pool where we should allocate the list
 *
 * @returns Status code
 */
ib_status_t ib_radix_match_all_data(ib_radix_t *radix,
                                    ib_radix_prefix_t *prefix,
                                    ib_list_t **rlist,
                                    ib_mpool_t *mp);

/*
 * Create a prefix of type ib_radix_prefix_t given the cidr ascii representation
 * Valid for ipv4 and ipv6.
 * warning:
 *  the criteria to determine if ipv6 or ipv4 is the presence of ':' (ipv6)
 *  so the functions using this API should implement their own checks for valid
 *  formats, with regex, or functions, thought
 *
 * @param[in] cidr ascii representation
 * @param[out] prefix reference to link the new prefix
 * @param[in] mp pool where we should allocate the prefix
 *
 * @returns Status code
 */
ib_status_t ib_radix_ip_to_prefix(const char *cidr,
                                  ib_radix_prefix_t **prefix,
                                  ib_mpool_t *mp);

/*
 * Create a prefix of type ib_radix_prefix_t given the cidr ASCII
 * representation Valid for ipv4 and ipv6.
 *
 * warning:
 *  the criteria to determine if ipv6 or ipv4 is the presence of ':' (ipv6)
 *  so the functions using this API should implement their own checks for valid
 *  formats, with regex, or functions, thought
 *
 * @param[in] cidr ASCII representation
 * @param[in] len ASCII string length
 * @param[out] prefix reference to link the new prefix
 * @param[in] mp pool where we should allocate the list
 *
 * @returns Status code
 */
ib_status_t ib_radix_ip_to_prefix_ex(const char *cidr,
                                     size_t len,
                                     ib_radix_prefix_t **prefix,
                                     ib_mpool_t *mp);

/** @} IronBeeUtilRadix */


#ifdef __cplusplus
}
#endif

#endif /* _IB_RADIX_H_ */
