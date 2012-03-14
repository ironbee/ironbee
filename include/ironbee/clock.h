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

#ifndef _IB_CLOCK_H_
#define _IB_CLOCK_H_

/**
 * @file
 * @brief IronBee - Utility Functions
 *
 * @author William Metcalf <wmetcalf@qualys.com>
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeClock Clock and Timing Functions
 * @ingroup IronBeeUtil
 * @{
 */

/** Microsecond Time */
typedef uint64_t ib_time_t;

/** Clock Types */
typedef enum ib_clock_type_t {
    IB_CLOCK_TYPE_UNKNOWN,
    IB_CLOCK_TYPE_NONMONOTONIC,
    IB_CLOCK_TYPE_MONOTONIC,
    IB_CLOCK_TYPE_MONOTONIC_RAW
} ib_clock_type_t;

/**
 * Convert an ib_time_t to struct timeval.
 *
 * @param[out] tv Timeval structure (struct timeval)
 * @param[in]  time IronBee time structure (ib_time_t)
 *
 * @returns Status code
 */
#define IB_CLOCK_TIMEVAL(tv, time) \
    do { \
        (tv).tv_sec = (time)/1000000 ;\
        (tv).tv_usec = (time) - ((tv).tv_sec * 1000000); \
    } while (0)

/**
 * Convert an ib_time_t to seconds (epoch)
 *
 * @param[in]  time IronBee time structure (ib_time_t)
 *
 * @returns Status code
 */
#define IB_CLOCK_SECS(time) ((time)/1000000)


/**
 * Get the clock type.
 *
 * @returns Clock type
 */
ib_clock_type_t DLL_PUBLIC ib_clock_type(void);

/**
 * Get a timestamp.
 *
 * @note This is not monotonic on all platforms.
 *
 * @returns Timestamp
 *
 */
ib_time_t DLL_PUBLIC ib_clock_get_time(void);

/** @} IronBeeUtilClock */

#ifdef __cplusplus
}
#endif

#endif /* _IB_CLOCK_H_ */
