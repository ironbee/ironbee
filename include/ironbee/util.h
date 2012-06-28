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

#ifndef _IB_UTIL_H_
#define _IB_UTIL_H_

/**
 * @file
 * @brief IronBee &mdash; Utility Functions
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/types.h>
#include <ironbee/mpool.h>

#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeUtil Utility Functions
 * @ingroup IronBee
 *
 * This module contains a wide variety of useful code not necessarily
 * specific to IronBee.
 *
 * @{
 */


/**
 * Logger callback.
 *
 * @param cbdata Callback data
 * @param level Log level
 * @param file Optional source filename (or NULL)
 * @param line Optional source line number (or 0)
 * @param fmt Formatting string
 */
typedef void (*ib_util_fn_logger_t)(void *cbdata, int level,
                                    const char *file, int line,
                                    const char *fmt, va_list ap)
                                    VPRINTF_ATTRIBUTE(5);

/** Normal Logger. */
#define ib_util_log(lvl,...) \
  ib_util_log_ex((lvl),__FILE__,__LINE__,__VA_ARGS__)

/** Error Logger. */
#define ib_util_log_error(...) \
  ib_util_log_ex(3,__FILE__,__LINE__,__VA_ARGS__)

/** Debug Logger. */
#define ib_util_log_debug(...) \
  ib_util_log_ex(7,__FILE__,__LINE__,__VA_ARGS__)


/**
 * When passed to @ref ib_util_unescape_string an escaped null character will
 * results in the string not being parsed and IB_EINVAL being returned.
 */
#define IB_UTIL_UNESCAPE_NONULL    (1U << 0)
#define IB_UTIL_UNESCAPE_NULTERMINATE (1U << 1)

/**
 * Set the logger level.
 *
 * @param level Log level
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_util_log_level(int level);

/**
 * Set the logger.
 *
 * Sets a callback which will be called to perform the logging. Any value
 * set in @a cbdata is passed as a parameter to the callback function.
 *
 * @param callback Logger callback
 * @param cbdata Data passed to callback
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_util_log_logger(ib_util_fn_logger_t callback,
                                          void *cbdata);

/**
 * Write a log entry via the logger callback.
 *
 * @param level Log level (0-9)
 * @param file Filename (or NULL)
 * @param line Line number (or 0)
 * @param fmt Printf-like format string
 */
void DLL_PUBLIC ib_util_log_ex(int level,
                               const char *file, int line,
                               const char *fmt, ...)
                               PRINTF_ATTRIBUTE(4, 5);

/**
 * Create a directory path recursively.
 *
 * @param path Path to create
 * @param mode Mode to create directories with
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_util_mkpath(const char *path, mode_t mode);


/**
 * Create a file path relative to a reference file.
 *
 * If @a file_path looks like an absolute path (it starts with a '/'),
 * a copy of @a file_path is returned.  If not, the directory portion
 * of @a ref_file is joined with @a file_path using ib_util_path_join,
 * and the resulting path is returned.
 *
 * @param[in] mp Memory pool to use for allocations
 * @param[in] ref_file Reference file path
 * @param[in] file_path New file's path
 *
 * @return Pointer to new path, or NULL if unable to allocate memory
 */
char DLL_PUBLIC *ib_util_relative_file(ib_mpool_t *mp,
                            const char *ref_file,
                            const char *file_path);


/**
 * Join two path components (similar to os.path.join() in Python)
 *
 * @param[in] mp Memory pool to use for allocations
 * @param[in] parent Parent portion of path
 * @param[in] file_path Child portion of path
 *
 * @return Pointer to new path, or NULL if unable to allocate memory
 */
char DLL_PUBLIC *ib_util_path_join(ib_mpool_t *mp,
                        const char *parent,
                        const char *file_path);

/**
 * Unescape a Javascript-escaped string into the @a dst string buffer.
 *
 * Decode the contents of @a str into @a dst. Then terminate @a dst with \0
 * if @a flags includes IB_UTIL_UNESCAPE_NULTERMINATE. In this case,
 * @a dst must be @a src_len+1 in size.
 *
 * Because @a src may be a segment in a larger character buffer,
 * @a src is not treated as a \0 terminated string, but is
 * processed using the given @a src_len.
 *
 * The resultant buffer @a dst should also not be treated as a typical string
 * because a \0 character could appear in the middle of the buffer unless
 * IB_UTIL_UNESCAPE_NONULL is set in @a flags.
 *
 * If IB_OK is not returned then @a dst and @a dst_len are left in an
 * inconsistent state.
 *
 * @param[out] dst string buffer that should be at least as long as
 *             @a src_len or @a src_len+1 if IB_UTIL_UNESCAPE_NULTERMINATE
 *             is set.
 * @param[out] dst_len the length of the decoded byte array. This will be
 *             equal to or shorter than @a src_len. Note that srclen(dst)
 *             could result in a smaller value than @a dst_len because of
 *             a \0 character showing up in the middle of the array.
 * @param[in] src source string that is encoded.
 * @param[in] src_len the length of @a src.
 * @param[in] flags Flags that affect how the string is processed.
 *
 * @returns IB_OK if successful. IB_EINVAL if the string cannot be unescaped
 *          because of short escape codes or non-hex values being passed
 *          to escape codes.
 *
 *          IB_EBADVAL is returned if a flag is set and the string cannot
 *          be decoded because of the flag settings.
 *
 *          On a failure @a dst_len are left in an inconsistent state.
 */
ib_status_t DLL_PUBLIC ib_util_unescape_string(char *dst,
                                               size_t *dst_len,
                                               const char *src,
                                               size_t src_len,
                                               uint32_t flags);

/**
 * Copy a buffer before it's written to.
 *
 * If @a data_out is not NULL, this function does nothing.  Otherwise, a new
 * buffer of size @a size is allocated, @a data_out is pointed at it, input
 * data from @a data_in up to @a cur_in is copied into it, and a pointer into
 * the output @a data_out at the same offset is returned.  See code in
 * util/decode.c for example usage.
 *
 * @param[in] mp Memory pool to use for allocations
 * @param[in] data_in Input data
 * @param[in] end_in End of data to copy from @a data_in
 * @param[in] size Size of buffer to allocate
 * @param[in] cur_out Current output pointer
 * @param[in,out] data_out Output buffer (possibly newly allocated)
 * @param[in,out] end_out End of output buffer (ignored if NULL)
 *
 * @return New output position in @a data_out,
 *         or NULL if unable to allocate memory
 */
uint8_t DLL_PUBLIC *ib_util_copy_on_write(ib_mpool_t *mp,
                                          const uint8_t *data_in,
                                          const uint8_t *end_in,
                                          size_t size,
                                          uint8_t *cur_out,
                                          uint8_t **data_out,
                                          const uint8_t **end_out);

/**
 * Duplicate memory using ib_mpool_alloc() or malloc(), optionally add a nul
 *
 * @param[in] mp Memory pool to use for allocation, or NULL to use malloc()
 * @param[in] in Input data
 * @param[in] len Length of input
 * @param[in] nul Add nul byte?
 *
 * @returns Pointer to new buffer or NULL
 */
void *ib_util_memdup(ib_mpool_t *mp,
                     const void *in,
                     size_t len,
                     bool nul);

/**
 * In-place decode a URL (NUL-string version)
 *
 * @param[in,out] data Buffer to operate on
 * @param[out] result Result flags (IB_STRFLAG_xxx)
 *
 * @returns Status code:
 * - IB_OK: Success
 */
ib_status_t DLL_PUBLIC ib_util_decode_url(char *data,
                                          ib_flags_t *result);

/**
 * In-place decode a URL (ex version)
 *
 * @param[in,out] data_in Buffer to operate on
 * @param[in] dlen_in Length of @a data_in
 * @param[out] dlen_out Output length
 * @param[out] result Result flags (IB_STRFLAG_xxx)
 *
 * @returns Status code:
 * - IB_OK: Success
 */
ib_status_t DLL_PUBLIC ib_util_decode_url_ex(uint8_t *data_in,
                                             size_t dlen_in,
                                             size_t *dlen_out,
                                             ib_flags_t *result);

/**
 * Copy-on-write decode a URL (NUL-string version)
 *
 * @param[in] mp Memory pool for allocations
 * @param[in] data_in Buffer to operate on
 * @param[out] data_out Output data
 * @param[out] result Result flags (IB_STRFLAG_xxx)
 *
 * @returns Status code:
 * - IB_OK: Success
 * - IB_EALLOC: allocation error
 */
ib_status_t DLL_PUBLIC ib_util_decode_url_cow(ib_mpool_t *mp,
                                              const char *data_in,
                                              char **data_out,
                                              ib_flags_t *result);

/**
 * Copy-on-write decode a URL (ex version)
 *
 * @param[in] mp Memory pool for allocations
 * @param[in] data_in Buffer to operate on
 * @param[in] dlen_in Length of @a data_in
 * @param[out] data_out Output data
 * @param[out] dlen_out Length of @a data_out
 * @param[out] result Result flags (IB_STRFLAG_xxx)
 *
 * @returns Status code:
 * - IB_OK: Success
 * - IB_EALLOC: allocation error
 */
ib_status_t DLL_PUBLIC ib_util_decode_url_cow_ex(ib_mpool_t *mp,
                                                 const uint8_t *data_in,
                                                 size_t dlen_in,
                                                 uint8_t **data_out,
                                                 size_t *dlen_out,
                                                 ib_flags_t *result);

/**
 * In-place decode HTML entities (NUL-string version)
 *
 * @param[in,out] data Buffer to operate on
 * @param[out] result Result flags
 *
 * @returns Status code:
 * - IB_OK: Success
 */
ib_status_t DLL_PUBLIC ib_util_decode_html_entity(
    char *data,
    ib_flags_t *result);

/**
 * In-place decode HTML entities (ex version)
 *
 * @param[in,out] data Buffer to operate on
 * @param[in] dlen_in Length of @a data
 * @param[out] dlen_out Output length
 * @param[out] result Result flags (IB_STRFLAG_xxx)
 *
 * @returns Status (IB_OK)
 */
ib_status_t DLL_PUBLIC ib_util_decode_html_entity_ex(
    uint8_t *data,
    size_t dlen_in,
    size_t *dlen_out,
    ib_flags_t *result);

/**
 * Copy-on-write decode HTML entity (NUL-string version)
 *
 * @param[in] mp Memory pool for allocations
 * @param[in] data_in Buffer to operate on
 * @param[out] data_out Output data
 * @param[out] result Result flags (IB_STRFLAG_xxx)
 *
 * @returns Status: IB_OK
 *                  IB_EALLOC for allocation errors
 */
ib_status_t DLL_PUBLIC ib_util_decode_html_entity_cow(
    ib_mpool_t *mp,
    const char *data_in,
    char **data_out,
    ib_flags_t *result);

/**
 * Copy-on-write decode HTML entity (ex version)
 *
 * @param[in] mp Memory pool for allocations
 * @param[in] data_in Buffer to operate on
 * @param[in] dlen_in Length of @a buf
 * @param[out] data_out Output data
 * @param[out] dlen_out Length of @a data_out
 * @param[out] result Result flags
 *
 * @returns Status code
 * - IB_OK: Success
 * - IB_EALLOC: Allocation error
 */
ib_status_t DLL_PUBLIC ib_util_decode_html_entity_cow_ex(
    ib_mpool_t *mp,
    const uint8_t *data_in,
    size_t dlen_in,
    uint8_t **data_out,
    size_t *dlen_out,
    ib_flags_t *result);


/**
 * Normalize a path (in-place / NUL string version)
 *
 * @param[in,out] data Buffer to operate on
 * @param[in] win Handle Windows style '\' as well as '/'
 * @param[out] result Result flags (IB_STRFLAG_xxx)
 *
 * @returns Status code
 * - IB_OK: Success
 * - IB_EALLOC: Allocation error
 */
ib_status_t DLL_PUBLIC ib_util_normalize_path(
    char *data,
    bool win,
    ib_flags_t *result);

/**
 * Normalize a path (in-place / ex version)
 *
 * @param[in] data Buffer to operate on
 * @param[in] dlen_in Length of @a buf
 * @param[in] win Handle Windows style '\' as well as '/'
 * @param[out] dlen_out Length of @a data after normalization
 * @param[out] result Result flags (IB_STRFLAG_xxx)
 *
 * @returns Status code
 * - IB_OK: Success
 * - IB_EALLOC: Allocation error
 */
ib_status_t DLL_PUBLIC ib_util_normalize_path_ex(
    uint8_t *data,
    size_t dlen_in,
    bool win,
    size_t *dlen_out,
    ib_flags_t *result);

/**
 * Normalize a path (copy-on-write / NUL string version)
 *
 * @param[in] mp Memory pool for allocations
 * @param[in] data_in Buffer to operate on
 * @param[in] win Handle Windows style '\' as well as '/'
 * @param[out] data_out Output data
 * @param[out] result Result flags (IB_STRFLAG_xxx)
 *
 * @returns Status code
 * - IB_OK: Success
 * - IB_EALLOC: Allocation error
 */
ib_status_t DLL_PUBLIC ib_util_normalize_path_cow(
    ib_mpool_t *mp,
    const char *data_in,
    bool win,
    char **data_out,
    ib_flags_t *result);

/**
 * Normalize a path (copy-on-write / ex version)
 *
 * @param[in] mp Memory pool for allocations
 * @param[in] data_in Buffer to operate on
 * @param[in] dlen_in Length of @a data_in
 * @param[in] win Handle Windows style '\' as well as '/'
 * @param[out] data_out Output data
 * @param[out] dlen_out Length of @a data_out
 * @param[out] result Result flags (IB_STRFLAG_xxx)
 *
 * @returns Status code
 * - IB_OK: Success
 * - IB_EALLOC: Allocation error
 */
ib_status_t DLL_PUBLIC ib_util_normalize_path_cow_ex(
    ib_mpool_t *mp,
    const uint8_t *data_in,
    size_t dlen_in,
    bool win,
    uint8_t **data_out,
    size_t *dlen_out,
    ib_flags_t *result);

/**
 * Malloc a @c char* and escape @a src into it and return that @c char*.
 *
 * Escaping is done by finding ASCII non-printable characters
 * and replacing them with @c 0xhh where @c hh is the hexadecimal value
 * of the character.
 *
 * This utility is intended to assist in logging otherwise unprintable
 * strings for information purposes. There is no way to distinguish
 * between the string "hi0x00" and "hi" where the last byte is a zero once
 * the two strings have passed through this function.
 *
 * The returned string must be free'ed.
 *
 * @param[in] src The source string.
 * @param[in] src_len The length of @a src not including the final NUL.
 *
 * @returns
 * - NULL on error;
 * - a NUL-terminated string that must be free'ed on success.
 */
char DLL_PUBLIC * ib_util_hex_escape(const char *src, size_t src_len);

/**
 * Initialize the IB lib.
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_initialize(void);

/**
 * Shutdown the IB lib.
 */
void DLL_PUBLIC ib_shutdown(void);

/**
 * Test if any of a set of flags is set
 *
 * @param[in] flags Flags to test
 * @param[in] check Flag bits to test check in @a flags
 *
 * @returns boolean value
 */
bool ib_flags_any(ib_flags_t flags, ib_flags_t check);
#define ib_flags_any(flags,check) \
    ( ( ((flags) & (check)) != 0) ? true : false)

/**
 * Test if all of a set of flags is set
 *
 * @param[in] flags Flags to test
 * @param[in] check Flag bits to test check in @a flags
 *
 * @returns boolean value
 */
bool ib_flags_all(ib_flags_t flags, ib_flags_t check);
#define ib_flags_all(flags,check) \
    ( ( ((flags) & (check)) == (check)) ? true : false)

/**
 * Set flag bits
 *
 * @param[in] flags Flags to modify
 * @param[in] flags_set Flag bits to set in @a flags
 *
 * @returns updated flags
 */
bool ib_flags_set(ib_flags_t flags, ib_flags_t flags_set);
#define ib_flags_set(flags,flags_set) \
    ( (flags) |= (flags_set) )

/**
 * Clear flag bits
 *
 * @param[in] flags Flags to modify
 * @param[in] flags_clear Flag bits to clear in @a flags
 *
 * @returns updated flags
 */
bool ib_flags_clear(ib_flags_t flags, ib_flags_t flags_clear);
#define ib_flags_clear(flags,flags_clear) \
    ( (flags) &= (~(flags_clear)) )


/**
 * @} IronBeeUtil
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_UTIL_H_ */
