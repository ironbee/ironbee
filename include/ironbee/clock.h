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

#ifndef _IB_CLOCK_H_
#define _IB_CLOCK_H_

/**
 * @file
 * @brief IronBee --- Utility Functions
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
    IB_CLOCK_TYPE_MONOTONIC_RAW,
    IB_CLOCK_TYPE_MONOTONIC_COARSE
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
 * Number of bytes to format a timestamp as text not including the null
 * terminating character.
 */
#define IB_CLOCK_FMT_WIDTH 30

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
 * Convert an ib_time_t to ib_timeval_t structure.
 *
 * @param[out] td Time diff structure (ib_timeval_t)
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
 * Compare two ib_timeval_t values
 *
 * @param[in] t1 First time
 * @param[in] t2 Second time
 *
 * @returns
 *   - == 0 if t1 == t2
 *   -  > 0 if t2 > t1
 *   -  < 0 if t2 < t1
 */
int DLL_PUBLIC ib_clock_timeval_cmp(const ib_timeval_t *t1,
                                    const ib_timeval_t *t2);

/**
 * Convert an ib_time_t (microseconds) to seconds
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
 * This is to be used for time deltas, and the value may or may not be related
 * to the value returned by time(3) (i.e. seconds since epoch).
 *
 * @note This is not monotonic nor wall time on all platforms.
 *
 * @returns Microsecond time value
 *
 */
ib_time_t DLL_PUBLIC ib_clock_get_time(void);

/**
 * Get the clock time, preferring more precise clocks.
 *
 * This is to be used for time deltas, and the value may or may not be related
 * to the value returned by time(3) (i.e. seconds since epoch).
 *
 * @note This is not monotonic nor wall time on all platforms.
 *
 * @returns Microsecond time value
 */
ib_time_t DLL_PUBLIC ib_clock_precise_get_time(void);

/**
 * IronBee types version of @c gettimeofday() called with
 * NULL timezone parameter.  The returned time is relative to epoch.
 *
 * This is essentially @c gettimeofday(ib_timeval_t *tp, @c NULL).
 *
 * @param[out] tp Address which ib_timeval_t is written
 */
void ib_clock_gettimeofday(ib_timeval_t *tp);

/**
 * Add a time from two timeval structures.  This is written in such a way that
 * @a result may be an alias for @a tv1 and/or @a tv2.
 *
 * @param[in] tv1 First time value structure
 * @param[in] tv2 Second time value structure
 * @param[out] result Result time value structure
 */
void ib_clock_timeval_add(const ib_timeval_t *tv1,
                          const ib_timeval_t *tv2,
                          ib_timeval_t *result);

/**
 * Generate a string timestamp.
 *
 * If ptv is NULL, then the current time is used.
 *
 * Format: YYYY-MM-DDTHH:MM:SS.ssss+/-ZZZZ
 * Example: 2010-11-04T12:42:36.3874-0800
 *
 * @param[out] buf Buffer at least IB_CLOCK_FMT_WIDTH (30) bytes in length
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
 * @param[out] buf Buffer at least IB_CLOCK_FMT_WIDTH (30) bytes in length
 * @param[in] ptv Address of the ib_timeval_t structure or NULL
 * @param[in] offset Time offset in microseconds
 */
void ib_clock_relative_timestamp(char *buf, const ib_timeval_t *ptv, ib_time_t offset);

/** @} IronBeeUtilClock */

#ifdef __cplusplus
}
#endif

#endif /* _IB_CLOCK_H_ */
