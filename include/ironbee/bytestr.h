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

#ifndef _IB_BYTESTR_H_
#define _IB_BYTESTR_H_

/**
 * @file
 * @brief IronBee --- Byte String Utility Functions
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <ironbee/types.h>
#include <ironbee/build.h>
#include <ironbee/mm.h>

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @defgroup IronBeeUtilByteStr Byte String
 * @ingroup IronBeeUtil
 *
 * Mpool managed strings with support for embedded NULs.
 *
 * @{
 */

typedef struct ib_bytestr_t ib_bytestr_t;

#define IB_BYTESTR_FREADONLY           (1<<0)

#define IB_BYTESTR_CHECK_FREADONLY(f)  ((f) & IB_BYTESTR_FREADONLY)

/**
 * Parameter for string/length formatter.
 *
 * Allow using a string and length for %.*s style formatters.
 *
 * @todo Fix with escaping for at least NULs
 *
 * @param s String
 * @param l Length
 */
#define IB_BYTESTRSL_FMT_PARAM(s, l)  (int)(l), (const char *)(s)

/**
 * Parameter for byte string formatter.
 *
 * Allows using a ib_bytestr_t with %.*s style formatters.
 *
 * @todo Fix for ib_bytestr_t type with escaping for at least NULs
 *
 * @param bs Bytestring
 */
#define IB_BYTESTR_FMT_PARAM(bs) \
  (int)ib_bytestr_length(bs), (const char *)ib_bytestr_const_ptr(bs)

/** Printf style format string for bytestr. */
#define IB_BYTESTR_FMT         ".*s"

/**
 * Create a byte string.
 *
 * @param pdst Address which new bytestring is written
 * @param mm Memory manager
 * @param size Size allocated for byte string
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_bytestr_create(ib_bytestr_t **pdst,
                                         ib_mm_t mm,
                                         size_t size);

/**
 * Create a byte string as a copy of another byte string.
 *
 * @param pdst Address which new bytestring is written
 * @param mm Memory manager
 * @param src Byte string to duplicate
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_bytestr_dup(ib_bytestr_t **pdst,
                                      ib_mm_t mm,
                                      const ib_bytestr_t *src);

/**
 * Create a byte string as a copy of a memory address and length.
 *
 * @param pdst Address which new bytestring is written
 * @param mm Memory manager
 * @param data Memory address which contains the data
 * @param dlen Length of data
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_bytestr_dup_mem(ib_bytestr_t **pdst,
                                          ib_mm_t mm,
                                          const uint8_t *data,
                                          size_t dlen);

/**
 * Create a byte string as a copy of a NUL terminated string.
 *
 * @param pdst Address which new bytestring is written
 * @param mm Memory manager
 * @param data String to duplicate
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_bytestr_dup_nulstr(ib_bytestr_t **pdst,
                                             ib_mm_t mm,
                                             const char *data);

/**
 * Create a byte string that is an alias (contains a reference) to the
 * data in another byte string.
 *
 * If either bytestring is modified, both will see the change as they
 * will both reference the same address.
 *
 * @param pdst Address which new bytestring is written
 * @param mm Memory manager
 * @param src Byte string to alias
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_bytestr_alias(ib_bytestr_t **pdst,
                                        ib_mm_t mm,
                                        const ib_bytestr_t *src);

/**
 * Create a byte string that is an alias (contains a reference) to the
 * data at a given memory location with a given length.
 *
 * If either the bytestring or memory is modified, both will see the change
 * as they will both reference the same address.
 *
 * @param pdst Address which new bytestring is written
 * @param mm Memory manager
 * @param data Memory address which contains the data
 * @param dlen Length of data
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_bytestr_alias_mem(ib_bytestr_t **pdst,
                                            ib_mm_t mm,
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
 * @param mm Memory manager
 * @param data String to alias
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_bytestr_alias_nulstr(ib_bytestr_t **pdst,
                                               ib_mm_t mm,
                                               const char *data);

/**
 * Set the value of the bytestring.
 *
 * @param dst Bytestring which will have data set
 * @param data New data
 * @param dlen New data length
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_bytestr_setv(ib_bytestr_t *dst,
                                       uint8_t *data,
                                       size_t dlen);

/**
 * Set the value of the bytestring, const version.
 *
 * @param dst Bytestring which will have data set
 * @param data New data
 * @param dlen New data length
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_bytestr_setv_const(ib_bytestr_t *dst,
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
size_t DLL_PUBLIC ib_bytestr_length(const ib_bytestr_t *bs);

/**
 * Allocated size of the data in a byte string.
 *
 * @param bs Byte string
 *
 * @returns Allocated size of data in bytestring.
 */
size_t DLL_PUBLIC ib_bytestr_size(const ib_bytestr_t *bs);

/**
 * Memory manager associated with byte string.
 *
 * @param bs Byte string.
 *
 * @returns Memory manager associated with @a bs.
 */
ib_mm_t DLL_PUBLIC ib_bytestr_mm(const ib_bytestr_t *bs);

/**
 * Raw buffer containing data in a byte string.
 *
 * @param bs Byte string
 *
 * @returns Address of byte string buffer or NULL if bs is read only.
 */
uint8_t DLL_PUBLIC *ib_bytestr_ptr(ib_bytestr_t *bs);

/**
 * Is byte string read only?
 *
 * @param bs Byte string.
 *
 * @returns 1 if @a bs is read-only and 0 otherwise.
 **/
int ib_bytestr_read_only( const ib_bytestr_t *bs );

/**
 * Make byte string read only.
 *
 * Does nothing if @a bs is already read only.
 *
 * @param bs Byte string.
 **/
void ib_bytestr_make_read_only( ib_bytestr_t *bs );

/**
 * Const raw buffer containing data in a byte string.
 *
 * @param bs Byte string
 *
 * @returns Address of byte string buffer
 */
const uint8_t DLL_PUBLIC *ib_bytestr_const_ptr(const ib_bytestr_t *bs);

/**
 * Compare a ib_bytestr_t to the other argument.
 *
 * @param[in] bs The byte string to compare.
 * @param[in] mem The argument to compare @a bs to.
 * @parma[in] len The length of mem.
 *
 * @returns
 * - Less than 0 if bs is smaller than @a mem.
 * - Greater than 0 if bs is greater than @a mem.
 * - Zero if the two are equal.
 */
int DLL_PUBLIC ib_bytestr_memcmp(
  const ib_bytestr_t *bs,
  const void         *mem,
  size_t              len
)
NONNULL_ATTRIBUTE(1,2);

/**
 * Compare a ib_bytestr_t to the other argument.
 *
 * @param[in] bs The byte string to compare.
 * @param[in] str The argument to compare @a bs to.
 *
 * @returns
 * - Less than 0 if bs is smaller than @a str.
 * - Greater than 0 if bs is greater than @a str.
 * - Zero if the two are equal.
 */
int DLL_PUBLIC ib_bytestr_strcmp(
  const ib_bytestr_t *bs,
  const char         *str
)
NONNULL_ATTRIBUTE(1,2);

/**
 * Compare a ib_bytestr_t to the other argument.
 *
 * @param[in] bs The byte string to compare.
 * @param[in] that The argument to compare @a bs to.
 *
 * @returns
 * - Less than 0 if bs is smaller than @a that.
 * - Greater than 0 if bs is greater than @a that.
 * - Zero if the two are equal.
 */
int DLL_PUBLIC ib_bytestr_bscmp(
  const ib_bytestr_t *bs,
  const ib_bytestr_t *that
)
NONNULL_ATTRIBUTE(1,2);

/** @} IronBeeUtilByteStr */


#ifdef __cplusplus
}
#endif

#endif /* _IB_BYTESTR_H_ */
