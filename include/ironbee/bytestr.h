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

#ifndef _IB_BYTESTR_H_
#define _IB_BYTESTR_H_

/**
 * @file
 * @brief IronBee - Byte String Utility Functions
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


#include <ironbee/build.h>
#include <ironbee/release.h>
#include <ironbee/types.h>
#include <ironbee/array.h>
#include <ironbee/list.h>
#include <ironbee/field.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @defgroup IronBeeUtilByteStr Byte String
 * @ingroup IronBeeUtil
 * @{
 */

#define IB_BYTESTR_FREADONLY           (1<<0)

#define IB_BYTESTR_CHECK_FREADONLY(f)  ((f) & IB_BYTESTR_FREADONLY)

/**
 * Create a byte string.
 *
 * @param pdst Address which new bytestring is written
 * @param pool Memory pool
 * @param size Size allocated for byte string
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_bytestr_create(ib_bytestr_t **pdst,
                                         ib_mpool_t *pool,
                                         size_t size);

/**
 * Create a byte string as a copy of another byte string.
 *
 * @param pdst Address which new bytestring is written
 * @param pool Memory pool
 * @param src Byte string to duplicate
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_bytestr_dup(ib_bytestr_t **pdst,
                                      ib_mpool_t *pool,
                                      const ib_bytestr_t *src);

/**
 * Create a byte string as a copy of a memory address and length.
 *
 * @param pdst Address which new bytestring is written
 * @param pool Memory pool
 * @param data Memory address which contains the data
 * @param dlen Length of data
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_bytestr_dup_mem(ib_bytestr_t **pdst,
                                          ib_mpool_t *pool,
                                          const uint8_t *data,
                                          size_t dlen);

/**
 * Create a byte string as a copy of a NUL terminated string.
 *
 * @param pdst Address which new bytestring is written
 * @param pool Memory pool
 * @param data String to duplicate
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_bytestr_dup_nulstr(ib_bytestr_t **pdst,
                                             ib_mpool_t *pool,
                                             const char *data);

/**
 * Create a byte string that is an alias (contains a reference) to the
 * data in another byte string.
 *
 * If either bytestring is modified, both will see the change as they
 * will both reference the same address.
 *
 * @param pdst Address which new bytestring is written
 * @param pool Memory pool
 * @param src Byte string to alias
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_bytestr_alias(ib_bytestr_t **pdst,
                                        ib_mpool_t *pool,
                                        const ib_bytestr_t *src);

/**
 * Create a byte string that is an alias (contains a reference) to the
 * data at a given memory location with a given length.
 *
 * If either the bytestring or memory is modified, both will see the change
 * as they will both reference the same address.
 *
 * @param pdst Address which new bytestring is written
 * @param pool Memory pool
 * @param data Memory address which contains the data
 * @param dlen Length of data
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_bytestr_alias_mem(ib_bytestr_t **pdst,
                                            ib_mpool_t *pool,
                                            const uint8_t *data,
                                            size_t dlen);

/**
 * Create a byte string that is an alias (contains a reference) to the
 * data in a NUL terminated string.
 *
 * If either the bytestring or string is modified, both will see the change
 * as they will both reference the same address.
 *
 * @param pdst Address which new bytestring is written
 * @param pool Memory pool
 * @param data String to alias
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_bytestr_alias_nulstr(ib_bytestr_t **pdst,
                                               ib_mpool_t *pool,
                                               const char *data);

/**
 * Set the value of the bytestring.
 *
 * @param dst Bytestring which will have data appended
 * @param data New data
 * @param dlen New data length
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_bytestr_setv(ib_bytestr_t *dst,
                                       const uint8_t *data,
                                       size_t dlen);

/**
 * Extend a bytestring by appending the data from another bytestring.
 *
 * @param dst Bytestring which will have data appended
 * @param src Byte string which data is copied
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_bytestr_append(ib_bytestr_t *dst,
                                         const ib_bytestr_t *src);

/**
 * Extend a bytestring by appending the data from a memory address with
 * a given length.
 *
 * @param dst Bytestring which will have data appended
 * @param data Memory address containing the data
 * @param dlen Length of data
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_bytestr_append_mem(ib_bytestr_t *dst,
                                             const uint8_t *data,
                                             size_t dlen);

/**
 * Extend a bytestring by appending the data from a NUL terminated string.
 *
 * @note The NUL is not copied.
 *
 * @param dst Bytestring which will have data appended
 * @param data String containing data to be appended
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_bytestr_append_nulstr(ib_bytestr_t *dst,
                                                const char *data);

/**
 * Length of the data in a byte string.
 *
 * @param bs Byte string
 *
 * @returns Length of data in bytestring.
 */
size_t DLL_PUBLIC ib_bytestr_length(ib_bytestr_t *bs);

/**
 * Allocated size of the data in a byte string.
 *
 * @param bs Byte string
 *
 * @returns Allocated size of data in bytestring.
 */
size_t DLL_PUBLIC ib_bytestr_size(ib_bytestr_t *bs);

/**
 * Raw buffer containing data in a byte string.
 *
 * @param bs Byte string
 *
 * @returns Address of byte string buffer
 */
uint8_t DLL_PUBLIC *ib_bytestr_ptr(ib_bytestr_t *bs);

/**
 * Search for a c string in a byte string.
 *
 * @param[in] haystack Byte string to search.
 * @param[in] needle String to search for.
 *
 * @returns position of the match, or -1 if there is no match
 */
int DLL_PUBLIC ib_bytestr_index_of_c(ib_bytestr_t *haystack, char *needle);


/** @} IronBeeUtilByteStr */


#ifdef __cplusplus
}
#endif

#endif /* _IB_BYTESTR_H_ */
