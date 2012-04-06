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

#include <stdarg.h>
#include <stdlib.h>
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
 * @param prefix Optional prefix to the log
 * @param file Optional source filename (or NULL)
 * @param line Optional source line number (or 0)
 * @param fmt Formatting string
 */
typedef void (*ib_util_fn_logger_t)(void *cbdata, int level,
                                    const char *prefix,
                                    const char *file, int line,
                                    const char *fmt, va_list ap)
                                    VPRINTF_ATTRIBUTE(6);

/** Normal Logger. */
#define ib_util_log(lvl,...) \
  ib_util_log_ex((lvl),"IronBee: ",NULL,0,__VA_ARGS__)

/** Error Logger. */
#define ib_util_log_error(...) \
  ib_util_log_ex(3,"IronBeeUtil ERROR: ",NULL,0,__VA_ARGS__)

/** Debug Logger. */
#define ib_util_log_debug(...) \
  ib_util_log_ex(7,"IronBeeUtil DEBUG: ",__FILE__,__LINE__,__VA_ARGS__)


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
 * @param prefix String to prefix log header data (or NULL)
 * @param file Filename (or NULL)
 * @param line Line number (or 0)
 * @param fmt Printf-like format string
 */
void DLL_PUBLIC ib_util_log_ex(int level, const char *prefix,
                               const char *file, int line,
                               const char *fmt, ...)
                               PRINTF_ATTRIBUTE(5, 0);

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
char *ib_util_relative_file(ib_mpool_t *mp,
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
char *ib_util_path_join(ib_mpool_t *mp,
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
 * Convert a ib_bool_t to ib_tristate_t
 *
 * @param[in] boolean Boolean value to convert
 *
 * @returns tri-state value
 */
ib_bool_t ib_bool_to_tristate(ib_bool_t boolean);
#define ib_bool_to_tristate(boolean) \
    (((boolean) == IB_TRUE) ? IB_TRI_TRUE : IB_TRI_FALSE)

/**
 * Convert a ib_tristate_t to a ib_bool_t
 *
 * @param[in] tristate Tristate value to convert
 * @param[in] defbool Boolean value to return if @a tristate is UNSET
 *
 * @returns boolean value
 */
ib_bool_t ib_tristate_to_bool(ib_tristate_t tristate, ib_bool_t defbool);
#define ib_tristate_to_bool(tristate,defbool) \
    ( (tristate) == IB_TRI_TRUE ? IB_TRUE : \
      (tristate) == IB_TRI_FALSE ? IB_FALSE : (defbool) )

/**
 * Test if any of a set of flags is set
 *
 * @param[in] flags Flags to test
 * @param[in] check Flag bits to test check in @a flags
 *
 * @returns boolean value
 */
ib_bool_t ib_flags_any(ib_flags_t flags, ib_flags_t check);
#define ib_flags_any(flags,check) \
    ( ( ((flags) & (check)) != 0) ? IB_TRUE : IB_FALSE)

/**
 * Test if all of a set of flags is set
 *
 * @param[in] flags Flags to test
 * @param[in] check Flag bits to test check in @a flags
 *
 * @returns boolean value
 */
ib_bool_t ib_flags_all(ib_flags_t flags, ib_flags_t check);
#define ib_flags_all(flags,check) \
    ( ( ((flags) & (check)) == (check)) ? IB_TRUE : IB_FALSE)

/**
 * Set flag bits
 *
 * @param[in] flags Flags to modify
 * @param[in] flags_set Flag bits to set in @a flags
 *
 * @returns updated flags
 */
ib_bool_t ib_flags_set(ib_flags_t flags, ib_flags_t flags_set);
#define ib_flags_set(flags,flags_set) \
    ( (flags) | (flags_set) )

/**
 * Clear flag bits
 *
 * @param[in] flags Flags to modify
 * @param[in] flags_clear Flag bits to clear in @a flags
 *
 * @returns updated flags
 */
ib_bool_t ib_flags_clear(ib_flags_t flags, ib_flags_t flags_clear);
#define ib_flags_clear(flags,flags_clear) \
    ( (flags) & (~(flags_clear)) )


/**
 * @} IronBeeUtil
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_UTIL_H_ */
