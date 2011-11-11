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

#ifndef _IB_STRING_H_
#define _IB_STRING_H_

/**
 * @file
 * @brief IronBee - String Utility Functions
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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeUtilString String Utilities
 * @ingroup IronBeeUtil
 * @{
 */

/**
 * Convert string const to string and length parameters.
 *
 * Allows using a NUL terminated string in place of two parameters
 * (char*, len) by calling strlen().
 *
 * @param s String
 */
#define IB_S2SL(s)  (s), strlen(s)

/**
 * Convert string const to unsigned string and length parameters.
 *
 * Allows using a NUL terminated string in place of two parameters
 * (uint8_t*, len) by calling strlen().
 *
 * @param s String
 */
#define IB_S2USL(s)  ((uint8_t *)(s)), strlen(s)

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
#define IB_BYTESTRSL_FMT_PARAM(s,l)  (int)(l), (const char *)(s)

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
  (int)ib_bytestr_length(bs), (const char *)ib_bytestr_ptr(bs)

/** Printf style format string for bytestr. */
#define IB_BYTESTR_FMT         ".*s"


/**
 * @} IronBeeUtil
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_STRING_H_ */
