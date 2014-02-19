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

#ifndef _IB_ARRAY_H_
#define _IB_ARRAY_H_

/**
 * @file
 * @brief IronBee --- Array Utility Functions
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/mm.h>
#include <ironbee/types.h>

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeUtilArray Dynamic Array
 * @ingroup IronBeeUtil
 *
 * Dynamic array routines.
 *
 * @{
 */

/**
 * A dynamic array.
 */
typedef struct ib_array_t ib_array_t;

/**
 * Create an array.
 *
 * The array will be extended by "ninit" elements when more room is required.
 * Up to "nextents" extents will be performed.  If more than this number of
 * extents is required, then "nextents" will be doubled and the array will
 * be reorganized.
 *
 * @param parr Address which new array is written
 * @param mm Memory manager to use
 * @param ninit Initial number of elements
 * @param nextents Initial number of extents
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_array_create(ib_array_t **parr, ib_mm_t mm,
                                       size_t ninit, size_t nextents);

/**
 * Get an element from an array at a given index.
 *
 * If the array is not big enough to hold the index, then IB_EINVAL is
 * returned and pval will be NULL.
 *
 * @param arr Array
 * @param idx Index
 * @param pval Address which element is written
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_array_get(ib_array_t *arr, size_t idx, void *pval);

/**
 * Set an element from an array at a given index.
 *
 * If the array is not big enough to hold the index, then it will be extended
 * by ninit until it is at least this value before setting the value.
 *
 * @note The element is added without copying.
 *
 * @param arr Array
 * @param idx Index
 * @param val Value
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_array_setn(ib_array_t *arr, size_t idx, void *val);

/**
 * Append an element to the end of the array.
 *
 * If the array is not big enough to hold the index, then it will be extended
 * by ninit first.
 *
 * @note The element is added without copying.
 *
 * @param arr Array
 * @param val Value
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_array_appendn(ib_array_t *arr, void *val);

/**
 * Number of elements in an array.
 *
 * @param arr Array
 *
 * @returns Number of elements
 */
size_t DLL_PUBLIC ib_array_elements(ib_array_t *arr);

/**
 * Allocated space in the array.
 *
 * @param arr Array
 *
 * @returns Max number of elements before needing to extend.
 */
size_t DLL_PUBLIC ib_array_size(ib_array_t *arr);


/**
 * Dynamic array loop.
 *
 * This just loops over the indexes in a dynamic array.
 *
 * @code
 * // Where data stored in "arr" is "int *", this will print all int values.
 * size_t ne;
 * size_t idx;
 * int *val;
 * IB_ARRAY_LOOP(arr, ne, idx, val) {
 *     printf("%4d: item[%p]=%d\n", i++, val, *val);
 * }
 * @endcode
 *
 * @param arr Array
 * @param ne Symbol holding the number of elements
 * @param idx Symbol holding the index, set for each iteration
 * @param val Symbol holding the value at the index, set for each iteration
 */
#define IB_ARRAY_LOOP(arr, ne, idx, val) \
    for ((ne) = ib_array_elements(arr), (idx) = 0; \
         ib_array_get((arr), (idx), (void *)&(val)) \
             == IB_OK && (idx) < (ne); \
         ++(idx))

/**
 * Dynamic array loop in reverse.
 *
 * This just loops over the indexes in a dynamic array in reverse order.
 *
 * @code
 * // Where data stored in "arr" is "int *", this will print all int values.
 * size_t ne;
 * size_t idx;
 * int *val;
 * IB_ARRAY_LOOP_REVERSE(arr, ne, idx, val) {
 *     printf("%4d: item[%p]=%d\n", i++, val, *val);
 * }
 * @endcode
 *
 * @param arr Array
 * @param ne Symbol holding the number of elements
 * @param idx Symbol holding the index, set for each iteration
 * @param val Symbol holding the value at the index, set for each iteration
 */
#define IB_ARRAY_LOOP_REVERSE(arr, ne, idx, val) \
    for ((ne) = ib_array_elements(arr), (idx) = (ne) > 0 ? (ne)-1 : 0; \
         ib_array_get((arr), (idx), (void *)&(val)) == IB_OK && (idx) > 0; \
         --(idx))

/** @} IronBeeUtilArray */


/**
 * @} IronBeeUtil
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_ARRAY_H_ */
