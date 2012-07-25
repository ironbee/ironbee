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
 * @brief IronBee &mdash; Utility Functions
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
 * @defgroup IronBeeClock Clock and Timing
 * @ingroup IronBeeUtil
 *
 * Functions to access clocks.
 *
 * @{
 */

/** Fixed size version of "struct timeval" type. */
typedef struct {
    // TODO: Research other system sizes - we may want to use uint64_t
    uint32_t tv_sec;      /**< Seconds since epoch */
    uint32_t tv_usec;     /**< Fractional value in microseconds */
} ib_timeval_t;

/** Microsecond time as 64-bit integer. */
typedef uint64_t ib_time_t;

/** Clock Types */
typedef enum ib_clock_type_t {
    IB_CLOCK_TYPE_UNKNOWN,
    IB_CLOCK_TYPE_NONMONOTONIC,
    IB_CLOCK_TYPE_MONOTONIC,
    IB_CLOCK_TYPE_MONOTONIC_RAW
} ib_clock_type_t;

/**
 * Convert microseconds (usec) to milliseconds (msec).
 *
 * @param[in] usec Microseconds
 *
 * @returns Milliseconds from microseconds
 */
#define IB_CLOCK_USEC_TO_MSEC(usec) ((usec) / 1000)

/**
 * Convert an ib_timeval_t to ib_time_t.
 *
 * @param[in]  tv IronBee timeval structure (ib_timeval_t)
 *
 * @returns Status code
 */
#define IB_CLOCK_TIMEVAL_TIME(tv) ((ib_time_t)((((ib_time_t)(tv).tv_sec) * 1000000) + (tv).tv_usec))

/**
 * Convert an ib_time_t to timeval structure.
 *
 * @param[out] tv Timeval structure (ib_timeval_t or struct timeval)
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
 * Convert an ib_time_t to ib_timediff_t structure.
 *
 * @param[out] td Time diff structure (ib_timediff_t)
 * @param[in]  time IronBee time structure (ib_time_t)
 *
 * @returns Status code
 */
#define IB_CLOCK_TIMEDIFF(td, time) \
    do { \
        (td).tv_sec = (time)/1000000 ;\
        (td).tv_usec = (time) - ((td).tv_sec * 1000000); \
    } while (0)

/**
 * Assign values between two timeval structures.
 *
 * This is meant to convert between struct timeval and
 * ib_timeval_t. Either types are supported in dest.
 *
 * @param[out] dest Destination timeval structure
 * @param[in]  src Source timeval structure
 *
 * @returns Status code
 */
#define IB_CLOCK_ASSIGN_TIMEVAL(dest, src) \
    do { \
        (dest).tv_sec = (src).tv_sec; \
        (dest).tv_usec = (src).tv_usec; \
    } while (0)

/**
 * Adjust a timeval structure by a value in microseconds.
 *
 * This is meant to convert between struct timeval and
 * ib_timeval_t. Either types are supported in dest/src.
 *
 * @param[out] dest Destination timeval structure
 * @param[in]  usec Time in microseconds to adjust dest
 *
 * @returns Status code
 */
#define IB_CLOCK_ADJUST_TIMEVAL(dest, usec) \
    do { \
        ib_time_t t = IB_CLOCK_TIMEVAL_TIME((dest)); \
        t += (usec); \
        IB_CLOCK_TIMEVAL((dest),t); \
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
 * Get the clock time.
 *
 * This is to be used for time deltas.
 *
 * @note This is not monotonic nor wall time on all platforms.
 *
 * @returns Microsecond time value
 *
 */
ib_time_t DLL_PUBLIC ib_clock_get_time(void);

/**
 * IronBee types version of @c gettimeofday() called with
 * NULL timezone parameter.
 *
 * This is essentially @c gettimeofday(ib_timeval_t *tp, @c NULL).
 *
 * @param[out] tp Address which ib_timeval_t is written
 */
void ib_clock_gettimeofday(ib_timeval_t *tp);

/**
 * Generate a string timestamp.
 *
 * If ptv is NULL, then the current time is used.
 *
 * Format: YYYY-MM-DDTHH:MM:SS.ssss+/-ZZZZ
 * Example: 2010-11-04T12:42:36.3874-0800
 *
 * @param[out] buf Buffer at least 30 bytes in length
 * @param[in] ptv Address of the ib_timeval_t structure or NULL
 */
void ib_clock_timestamp(char *buf, const ib_timeval_t *ptv);

/**
 * Generate a string timestamp from a timeval structure and offset.
 *
 * If ptv is NULL, then the current time is used.
 *
 * Format: YYYY-MM-DDTHH:MM:SS.ssss+/-ZZZZ
 * Example: 2010-11-04T12:42:36.3874-0800
 *
 * @param[out] buf Buffer at least 30 bytes in length
 * @param[in] ptv Address of the ib_timeval_t structure or NULL
 * @param[in] offset Time offset in microseconds
 */
void ib_clock_relative_timestamp(char *buf, const ib_timeval_t *ptv, ib_time_t offset);

/** @} IronBeeUtilClock */

#ifdef __cplusplus
}
#endif

#endif /* _IB_CLOCK_H_ */
