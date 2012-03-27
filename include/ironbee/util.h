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
 * @brief IronBee - Utility Functions
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
#define ib_util_log_error(lvl,...) \
  ib_util_log_ex((lvl),"IronBeeUtil ERROR: ",NULL,0,__VA_ARGS__)

/** Abort Logger. */
#define ib_util_log_abort(...) \
  do { ib_util_log_ex(0,"IronBeeUtil ABORT: ",__FILE__,__LINE__,__VA_ARGS__); \
    abort(); } while(0)

/** Debug Logger. */
#define ib_util_log_debug(lvl,...) \
  ib_util_log_ex((lvl),"IronBeeUtil DBG: ",__FILE__,__LINE__,__VA_ARGS__)


/**
 * When passed to @ref ib_util_unescape_string an escaped null character will
 * results in the string not being parsed and IB_EINVAL being returned.
 */
#define IB_UTIL_UNESCAPE_NONULL 0x0001

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
 * Unescape a Javascript-escaped string into the @a dst string buffer.
 *
 * Decode the contents of @a str into @a dst. Then terminate @dst with \0.
 * This means @a dst must be @a src_len+1 in size.
 *
 * Because @a src may be a segment in a larger character buffer,
 * @a src is not treated as a \0 terminated string, but is
 * processed using the given @a src_len.
 *
 * The resultant buffer @a dst should also not be treated as a typical string
 * because a \0 character could appear in the middle of the buffer.
 *
 * If IB_OK is not returned then @a dst and @a dst_len are left in an
 * inconsistent state.
 *
 * @param[out] dst string buffer that should be at least as long as
 *             @a src_len+1.
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
 *          On a failure @dst_len are left in an inconsistent state.
 */
ib_status_t DLL_PUBLIC ib_util_unescape_string(char *dst,
                                               size_t *dst_len,
                                               const char *src,
                                               size_t src_len,
                                               uint32_t flags);

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
 * @} IronBeeUtil
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_UTIL_H_ */
