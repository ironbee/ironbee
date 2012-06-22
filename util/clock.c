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

/**
 * @file
 * @brief IronBee &mdash; Utility Functions
 *
 * @author William Metcalf <wmetcalf@qualys.com>
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/clock.h>

#include <time.h>
#include <sys/time.h>

#if 0
// FIXME: Just use gettimeofday for now until issues between wall/clock time are fixed.
#ifdef CLOCK_MONOTONIC_RAW
#define IB_CLOCK                  CLOCK_MONOTONIC_RAW
#else
#ifdef CLOCK_MONOTONIC
#define IB_CLOCK                  CLOCK_MONOTONIC
#endif /* CLOCK_MONOTONIC */
#endif /* CLOCK_MONOTONIC_RAW */
#endif

ib_clock_type_t ib_clock_type(void)
{
#ifdef IB_CLOCK
#ifdef CLOCK_MONOTONIC_RAW
    return IB_CLOCK_TYPE_MONOTONIC_RAW;
#else
    return IB_CLOCK_TYPE_MONOTONIC;
#endif /* CLOCK_MONOTONIC_RAW */
#else
    return IB_CLOCK_TYPE_NONMONOTONIC;
#endif /* IB_CLOCK */
}

ib_time_t ib_clock_get_time(void) {
    uint64_t us;

#ifdef IB_CLOCK
    struct timespec ts;

    /* Ticks seem to be an undesirable due for many reasons.
     * IB_CLOCK is set to CLOCK_MONOTONIC which is vulnerable to slew or
     * if available set to CLOCK_MONOTONIC_RAW which does not suffer from slew.
     *
     * timespec provides sec and nsec resolution so we have to convert to msec.
     */
    clock_gettime(IB_CLOCK, &ts);

    /* There are 1 million microsecs in a sec.
     * There are 1000 nanosecs in a microsec
     */
    us = (ts.tv_sec * 1000000) + (ts.tv_nsec / 1000);
#else
    struct timeval tv;

    gettimeofday(&tv, NULL);
    us = ((tv.tv_sec * 1000000) + tv.tv_usec);
#endif
    return us;
}
