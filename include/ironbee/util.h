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

#ifndef _IB_UTIL_H_
#define _IB_UTIL_H_

/**
 * @file
 * @brief IronBee &mdash; Utility Functions
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/mpool.h>
#include <ironbee/types.h>

#include <stdarg.h>
#include <stdbool.h>
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
 * @param file Optional source filename (or NULL)
 * @param line Optional source line number (or 0)
 * @param fmt Formatting string
 *
 * @internal
 * Implemented in: util/util.c
 * Tested in:
 */
typedef void (*ib_util_fn_logger_t)(void *cbdata, int level,
                                    const char *file, int line,
                                    const char *fmt, va_list ap)
                                    VPRINTF_ATTRIBUTE(5);

/**
 * Utility log at passed in level
 *
 * @param lvl Log level
 *
 * @internal
 * Implemented in: (self)
 * Tested in:
 */
#define ib_util_log(lvl, ...) \
  ib_util_log_ex((lvl), __FILE__, __LINE__, __VA_ARGS__)

/**
 * Utility log at error level
 *
 * @internal
 * Implemented in: (self)
 * Tested in:
 */
#define ib_util_log_error(...) \
  ib_util_log_ex(3, __FILE__, __LINE__, __VA_ARGS__)


/**
 * Utility log at debug level
 *
 * @internal
 * Implemented in: (self)
 * Tested in:
 */
#define ib_util_log_debug(...) \
  ib_util_log_ex(7, __FILE__, __LINE__, __VA_ARGS__)


/**
 * Set the logger level.
 *
 * @param level Log level
 *
 * @returns Status code
 *
 * @internal
 * Implemented in: util/util.c
 * Tested in:
 */
ib_status_t DLL_PUBLIC ib_util_log_level(int level);

/**
 * Get the logger level.
 *
 * @returns Logger level.
 *
 * @internal
 * Implemented in: util/util.c
 * Tested in:
 */
int DLL_PUBLIC ib_util_get_log_level(void);

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
 *
 * @internal
 * Implemented in: util/util.c
 * Tested in:
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
 *
 * @internal
 * Implemented in: util/util.c
 * Tested in:
 */
void DLL_PUBLIC ib_util_log_ex(int level,
                               const char *file, int line,
                               const char *fmt, ...)
                               PRINTF_ATTRIBUTE(4, 5);

/**
 * Copy a buffer before it's written to.
 *
 * If @a data_out is not NULL, this function does nothing.  Otherwise, a new
 * buffer of size @a size is allocated, @a data_out is pointed at it, input
 * data from @a data_in up to @a cur_in is copied into it, and a pointer into
 * the output @a data_out at the same offset is returned.  See code in
 * util/modsec_compat.c for example usage.
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
 *
 * @internal
 * Implemented in: util/util.c
 * Tested in:
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
 *
 * @internal
 * Implemented in: util/util.c
 * Tested in:
 */
void *ib_util_memdup(ib_mpool_t *mp,
                     const void *in,
                     size_t len,
                     bool nul);

/**
 * Initialize the IB lib.
 *
 * @returns Status code
 *
 * @internal
 * Implemented in: util/util.c
 * Tested in:
 */
ib_status_t DLL_PUBLIC ib_initialize(void);

/**
 * Shutdown the IB lib.
 *
 * @internal
 * Implemented in: util/util.c
 * Tested in:
 */
void DLL_PUBLIC ib_shutdown(void);

/**
 * Test if any of a set of flags is set
 *
 * @param[in] flags Flags to test
 * @param[in] check Flag bits to test check in @a flags
 *
 * @returns boolean value
 *
 * @internal
 * Implemented in: (self)
 * Tested in:
 */
bool ib_flags_any(ib_flags_t flags, ib_flags_t check);
#define ib_flags_any(flags, check) \
    ( ( ((flags) & (check)) != 0) ? true : false)

/**
 * Test if all of a set of flags is set
 *
 * @param[in] flags Flags to test
 * @param[in] check Flag bits to test check in @a flags
 *
 * @returns boolean value
 *
 * @internal
 * Implemented in: (self)
 * Tested in:
 */
bool ib_flags_all(ib_flags_t flags, ib_flags_t check);
#define ib_flags_all(flags, check) \
    ( ( ((flags) & (check)) == (check)) ? true : false)

/**
 * Set flag bits
 *
 * @param[in] flags Flags to modify
 * @param[in] flags_set Flag bits to set in @a flags
 *
 * @returns updated flags
 *
 * @internal
 * Implemented in: (self)
 * Tested in:
 */
bool ib_flags_set(ib_flags_t flags, ib_flags_t flags_set);
#define ib_flags_set(flags, flags_set) \
    ( (flags) |= (flags_set) )

/**
 * Clear flag bits
 *
 * @param[in] flags Flags to modify
 * @param[in] flags_clear Flag bits to clear in @a flags
 *
 * @returns updated flags
 *
 * @internal
 * Implemented in: (self)
 * Tested in:
 */
bool ib_flags_clear(ib_flags_t flags, ib_flags_t flags_clear);
#define ib_flags_clear(flags, flags_clear) \
    ( (flags) &= (~(flags_clear)) )


/**
 * @} IronBeeUtil
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_UTIL_H_ */
