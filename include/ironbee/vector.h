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

#ifndef _IB_VECTOR_H_
#define _IB_VECTOR_H_

/**
 * @file
 * @brief IronBee --- Vector Functions
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/mm.h>
#include <ironbee/mpool_lite.h>
#include <ironbee/types.h>

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeUtilVector Vector
 * @ingroup IronBeeUtil
 *
 * Vector routines.
 *
 * When using vectors, be aware that any change in the
 * length of the contents of a vector may result in the pointer
 * ib_vector_t::data changing.
 *
 * It is safer to store offsets into the buffer than to store the
 * absolute address of a string in ib_vector_t::data.
 *
 * @{
 */

/**
 * A flag, if set, tells the vector to never shrink the buffer size.
 *
 * This is an optimization to avoid shrinking a very large buffer
 * that the user knows will soon grow again to its previous size.
 */
#define IB_VECTOR_NEVER_SHRINK (0x1 << 0)

/**
 * A vector datastructure.
 */
struct ib_vector_t {
    size_t           size;  /**< The size of data. */
    size_t           len;   /**< The length used in the data segment. */
    ib_flags_t       flags; /**< Flags that affect vector operations. */
    ib_mpool_lite_t *mp;    /**< Where data came from. */
    void            *data;  /**< The data segment that holds the data. */
};
typedef struct ib_vector_t ib_vector_t;

/**
 * Create a vector.
 *
 * There is no destroy function as the memory pool @a mm will
 * handle everything. If it is required to release most of the
 * memory held by the @ref ib_vector_t, call ib_vector_truncate()
 * with a length of 0.
 *
 * @param[out] vector The out pointer.
 * @param[in] mm Memory manager this vector is allocated out of.
 * @param[in] flags Flags the affect vector operations.
 *
 * @returns
 * - IB_OK on success.
 * - IB_EALLOC on memory errors.
 * - IB_EUNKNOWN on memory pool lock failures.
 */
ib_status_t DLL_PUBLIC ib_vector_create(
    ib_vector_t **vector,
    ib_mm_t       mm,
    ib_flags_t    flags
)
NONNULL_ATTRIBUTE(1);

/**
 * Set the size of the vector.
 *
 * If the length of the data in the vector is longer than
 * @a size, then the data segment will be truncated to the new
 * size. Any append operation (except append 0 data) will cause the
 * data segment to grow.
 *
 * @param[in] vector The vector.
 * @param[in] size The size to set ib_vector_t::data to.
 *
 * @returns
 * - IB_OK on success.
 * - IB_EALLOC on memory errors.
 * - IB_EUNKNOWN on memory pool locking failures.
 */
ib_status_t DLL_PUBLIC ib_vector_resize(
    ib_vector_t *vector,
    size_t       size
);

/**
 * Truncate the vector unless IB_VECTOR_NEVER_SHRINK is set.
 *
 * If the new length of the vector is less than, or equal to 1/2 the current
 * buffer size, and the IB_VECTOR_NEVER_SHRINK option is not set,
 * the ib_vector_t::data 's size is reduced by 1/2.
 *
 * @param[in] vector The vector.
 * @param[in] len The new length of the data.
 *
 * @returns
 * - IB_OK on success or if IB_VECTOR_NEVER_SHRINK is set.
 * - IB_EINVAL If length is greater than size.
 */
ib_status_t DLL_PUBLIC ib_vector_truncate(
    ib_vector_t *vector,
    size_t       len
);

/**
 * Append data to the end of the memory pool.
 *
 * The pool is doubled in size if the append operation would exceed the
 * end of the current allocation. While this is order O(n) (where n is
 * the number of elements in the entire list) for a
 * particular append operation, all appends amortize to O(1) for each
 * element inserted.
 *
 * @param[in] vector The vector to manipulate.
 * @param[in] data The data to append to the end of the vector.
 * @param[in] data_length The length of @a data to be copied in.
 *
 * @returns
 * - IB_OK on success.
 * - IB_EALLOC on memory errors.
 * - IB_EINVAL @a data_length would result in a size that is too large to fit
 *             in a @c size_t.
 * - IB_EUNKNOWN on memory pool locking failures.
 */
ib_status_t DLL_PUBLIC ib_vector_append(
    ib_vector_t *vector,
    const void  *data,
    size_t       data_length
);

/**
 * @}
 */
#ifdef __cplusplus
}
#endif

#endif /* _IB_VECTOR_H_ */
