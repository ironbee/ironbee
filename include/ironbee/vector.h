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
#include <ironbee/mpool.h>
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
 * @c data changing.
 *
 * It is much safer to store offsets into the buffer than to store the 
 * absolute address of a string in @ref ib_vector_t @c -> @c data.
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
    size_t      size;  /**< The size of data. */
    size_t      len;   /**< The length used in the data segment. */
    ib_flags_t  flags; /**< Flags that affect vector operations. */
    ib_mpool_t *mp;    /**< Where data came from. This is a child pool. */
    void       *data;  /**< The data segment that holds the data. */
};
typedef struct ib_vector_t ib_vector_t;

/**
 * Create a vector.
 *
 * This will create a child memory pool of @a mp for allocations.
 * When vector is resized another child will be created, allocate the
 * new vector data, and the original child will be released to the parent.
 *
 * There is no destroy function as the memory pool @a mp will
 * handle everything. If it is required to release most of the
 * memory heald by the ib_vector_t, call ib_vector_truncate()
 * with a length of 0.
 *
 * @param[out] vector The out pointer.
 * @param[in] mp The parent memory pool for the vector.
 * @param[in] flags Flags the affect vector operations.
 *
 * @returns
 * - IB_OK on success.
 * - IB_EALLOC on memory errors.
 * - IB_EUNKNOWN on locking failures.
 */
ib_status_t DLL_PUBLIC ib_vector_create(
    ib_vector_t **vector,
    ib_mpool_t   *mp,
    ib_flags_t    flags
);

/**
 * Set the size of the vector.
 *
 * If the length of the data in the vector is longer than
 * @a size, then it will be truncated to the new
 * size. Any append operation (except append 0 data) will cause the
 * buffer to grow.
 *
 * @param[in] vector The vector.
 * @param[in] size The size to set the buffer to.
 *
 * @returns
 * - IB_OK on success.
 * - IB_EALLOC on memory errors.
 * - IB_EUNKNOWN on locking failures.
 */
ib_status_t DLL_PUBLIC ib_vector_resize(
    ib_vector_t *vector,
    size_t       size
);

/**
 * Truncate the vector.
 *
 * If the new length of the vector is less than, or equal to 1/4 the current
 * buffer size, the buffer is reduced by 1/2.
 *
 * @param[in] vector The vector.
 * @param[in] len The new length of the data.
 *
 * @returns
 * - IB_OK on success.
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
 * end of the current allocation. While this is order O(n) for a
 * particular append operation, amortized appends are O(1).
 *
 * @param[in] vector The vector to manipulate.
 * @param[in] data The data to append to the end of the vector.
 * @param[in] data_length The length of @a data to be copied in.
 *
 * @returns
 * - IB_OK on success.
 * - IB_EALLOC on memory errors.
 * - IB_EINVAL if the resulting length of the vector would undeflow.
 * - IB_EINVAL if we cannot store the size of a buffer a power of 2 greater
 *             then @a data_length.
 * - IB_EUNKNOWN on locking failures.
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
