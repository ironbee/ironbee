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

#ifndef _IB_RELEASE_H_
#define _IB_RELEASE_H_

/**
 * @file
 * @brief IronBee &mdash; Release Data
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeRelease Release
 * @ingroup IronBee
 *
 * Release information, e.g., version.
 *
 * @{
 */

/** Macro expand, then turn a literal into a string constant */
#define IB_XSTRINGIFY(s)   IB_STRINGIFY(s)

/** Turn a literal into a string constant */
#define IB_STRINGIFY(s)   #s

/* Name */
#define IB_PRODUCT_NAME IB_STRINGIFY(IronBee)

/* Version definitions */
#define IB_VERSION_MAJOR        0 /**< Major version */
#define IB_VERSION_MINOR        4 /**< Minor version */
#define IB_VERSION_MAINT        0 /**< Maintenance version */
#define IB_VERSION_BUILD_EXTRA  0 /**< Extra build number (normally 0) */

/* Handle tacking on a build number */
#if IB_VERSION_BUILD_EXTRA
/** Build version string */
#define IB_VERSION_EXTRA  "-" IB_VERSION_BUILD_EXTRA
#else
/** Extra version string */
#define IB_VERSION_EXTRA  ""
#endif

/** Version string */
#define IB_VERSION \
    IB_XSTRINGIFY(IB_VERSION_MAJOR) "." \
    IB_XSTRINGIFY(IB_VERSION_MINOR) "." \
    IB_XSTRINGIFY(IB_VERSION_MAINT) IB_VERSION_EXTRA

/** Package email address */
#define IB_EMAIL "ironbee-devel@lists.sourceforge.net"

/** Product version name string */
#define IB_PRODUCT_VERSION_NAME IB_PRODUCT_NAME "/" IB_VERSION

/**
 * IronBee version number.
 *
 * This records what version of the engine a module/plugin
 * was built against.
 *
 * Each version component represents 3 decimal digits in a 9 digit
 * number. For example, Version 1.2.3 would have a 1002003 decimal
 * value (001 . 002 . 003).
 */
#define IB_VERNUM    (uint32_t)(((IB_VERSION_MAJOR) * 1000000) + \
                      ((IB_VERSION_MAJOR) * 1000) + \
                      (IB_VERSION_MAINT))

/**
 * ABI version number.
 *
 * This number must be incremented each time the ABI changes. If a
 * module/plugin tries to load that was built with a number greater than
 * this, then it is not compatible.
 *
 * Format is a decimal number based on date ABI changed: YYYYMMDDn
 *   YYYY: 4-digit year
 *     MM: 2-digit month
 *     DD: 2-digit day
 *      n: Revision number if changes more than once in a day (default=0)
 */
#define IB_ABINUM    2012053001

/**
 * @} IronBeeRelease
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_RELEASE_H_ */
