//////////////////////////////////////////////////////////////////////////////
// Licensed to Qualys, Inc. (QUALYS) under one or more
// contributor license agreements.  See the NOTICE file distributed with
// this work for additional information regarding copyright ownership.
// QUALYS licenses this file to You under the Apache License, Version 2.0
// (the "License"); you may not use this file except in compliance with
// the License.  You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
/// @file
/// @brief IronBee &cfgmap; Configuration Mapping Test
///
/// @author Nick LeRoy <nleroy@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include <ironbee/hash.h>

#include "ironbee_config_auto.h"

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

#include <ironbee/types.h>
#include <ironbee/clock.h>

#include <stdexcept>
#include <sys/time.h>
#include <math.h>

double TvToSecs(const struct timeval &tv)
{
    return tv.tv_sec + (tv.tv_usec * 1e-6);
}
double TvToSecs(const ib_timeval_t &tv)
{
    return tv.tv_sec + (tv.tv_usec * 1e-6);
}

bool CheckUsecDiff(int64_t diff, unsigned int usecs)
{
    int64_t limit = usecs ? usecs / 5 : 100000;
    return (diff >= 0) && (abs(diff - usecs) < limit);
}
bool CheckSecDiff(double diff, double secs, double limit=-1.0)
{
    if (limit < 0.0) {
        limit = (secs > 0.0001) ? secs * 0.2 : 1e-5;
    }
    return (diff >= 0.0) && (fabs(diff - secs) < limit);
}

bool Compare(const struct timeval &tv,
             const ib_timeval_t &itv,
             double limit=-1.0)
{
    double secs = fabs(TvToSecs(tv) - TvToSecs(itv));
    return CheckSecDiff(secs, 0, limit);
}

bool CheckDelta(const ib_timeval_t &tv1,
                const ib_timeval_t &tv2,
                ib_time_t expected_usecs)
{
    double secs = TvToSecs(tv2) - TvToSecs(tv1);
    return CheckSecDiff(secs, expected_usecs * 1e-6);
}

bool CheckDelta(ib_time_t t1,
                ib_time_t t2,
                ib_time_t expected_usecs)
{
    int64_t diff = (int64_t)t2 - (int64_t)t1;
    return CheckUsecDiff(diff, expected_usecs);
}

TEST(TestClock, test_get_time)
{
    ib_time_t time1;
    ib_time_t time2;
    unsigned int usecs;
    bool rv;

    usecs = 1000;
    time1 = ib_clock_get_time( );
    usleep(usecs);
    time2 = ib_clock_get_time( );
    rv = CheckDelta(time1, time2, usecs);
    ASSERT_TRUE(rv);

    usecs = 10000;
    time1 = ib_clock_get_time( );
    usleep(usecs);
    time2 = ib_clock_get_time( );
    rv = CheckDelta(time1, time2, usecs);
    ASSERT_TRUE(rv);

    usecs = 100000;
    time1 = ib_clock_get_time( );
    usleep(usecs);
    time2 = ib_clock_get_time( );
    rv = CheckDelta(time1, time2, usecs);
    ASSERT_TRUE(rv);

    usecs = 1000000;
    time1 = ib_clock_get_time( );
    usleep(usecs);
    time2 = ib_clock_get_time( );
    rv = CheckDelta(time1, time2, usecs);
    ASSERT_TRUE(rv);
}

TEST(TestClock, test_gettimeofday)
{
    struct timeval tv;
    ib_timeval_t   itv;
    bool           rv;

    ASSERT_EQ(0, gettimeofday(&tv, NULL) );
    ib_clock_gettimeofday(&itv);
    rv = Compare(tv, itv);
    ASSERT_TRUE(rv);
}

TEST(TestClock, test_gettimeofday_diffs)
{
    ib_timeval_t tv1;
    ib_timeval_t tv2;
    unsigned int usecs;
    bool         rv;

    usecs = 100000;
    ib_clock_gettimeofday(&tv1);
    usleep(usecs);
    ib_clock_gettimeofday(&tv2);
    rv = CheckDelta(tv1, tv2, usecs);
    ASSERT_TRUE(rv);

    usecs = 500000;
    ib_clock_gettimeofday(&tv1);
    usleep(usecs);
    ib_clock_gettimeofday(&tv2);
    rv = CheckDelta(tv1, tv2, usecs);
    ASSERT_TRUE(rv);

    usecs = 1000000;
    ib_clock_gettimeofday(&tv1);
    usleep(usecs);
    ib_clock_gettimeofday(&tv2);
    rv = CheckDelta(tv1, tv2, usecs);
    ASSERT_TRUE(rv);
}

const size_t bufsize = 32;

void TestTimestamp(bool relative, int seconds)
{
    struct timeval tv;
    struct tm      tm;
    ib_timeval_t   itv;
    char           buf[bufsize + 1];
    bool           rv;

    ib_clock_gettimeofday(&itv);
    if (relative) {
        ib_time_t offset = seconds * 1000000;
        ib_clock_relative_timestamp(buf, &itv, offset);
    }
    else {
        seconds = 0;
        ib_clock_timestamp(buf, &itv);
    }

    ASSERT_STRNE(NULL, strptime(buf, "%Y-%m-%dT%H:%M:%S", &tm));
    tv.tv_sec = mktime(&tm) - seconds;
    ASSERT_EQ( (uint32_t)itv.tv_sec, (uint32_t)tv.tv_sec);
    tv.tv_usec = atoi(buf+20) * 100;
    rv = Compare(tv, itv, 0.001);
    ASSERT_TRUE(rv);
}

TEST(TestClock, test_timestamp)
{
    {
        SCOPED_TRACE("Timestamp");
        TestTimestamp(false, 0);
    }
}

TEST(TestClock, test_relative_timestamp)
{
    {
        SCOPED_TRACE("RelativeTimestamp 0s");
        TestTimestamp(true, 0);
    }
    {
        SCOPED_TRACE("RelativeTimestamp 1s");
        TestTimestamp(true, 1);
    }
    {
        SCOPED_TRACE("RelativeTimestamp -1s");
        TestTimestamp(true, -1);
    }
    {
        SCOPED_TRACE("RelativeTimestamp 60s");
        TestTimestamp(true, 60);
    }
    {
        SCOPED_TRACE("RelativeTimestamp -60s");
        TestTimestamp(true, -60);
    }
}
