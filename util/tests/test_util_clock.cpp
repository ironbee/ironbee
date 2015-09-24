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
/// @brief IronBee --- Clock utility tests
///
/// @author Nick LeRoy <nleroy@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include <ironbee/hash.h>

#include "ironbee_config_auto.h"

#include "gtest/gtest.h"

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

double GetTime()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return TvToSecs(tv);
}

bool CheckUsecDiff(int64_t t1, int64_t t2, unsigned int usecs)
{
    int64_t diff = (int64_t)llabs(t2 - t1);
    int64_t mean = (int64_t)llabs( (t2 + t1) / 2);
    int64_t limit = mean ? mean / 5 : 100000;

    return (diff >= 0) && (llabs(diff - usecs) < limit);
}
bool CheckSecDiff(double t1, double t2, double secs, double limit=-1.0)
{
    double diff = fabs(t2 - t1);
    double mean = fabs( (t1 + t2) * 0.5);

    if (limit < 0.0) {
        limit = (mean > 0.0001) ? mean * 0.2 : 1e-5;
    }
    return (diff >= 0.0) && (fabs(diff - secs) < limit);
}

bool Compare(const struct timeval &tv,
             const ib_timeval_t &itv,
             double limit=-1.0)
{
    return CheckSecDiff(TvToSecs(tv), TvToSecs(itv), 0, limit);
}

bool CheckDelta(const ib_timeval_t &tv1,
                const ib_timeval_t &tv2,
                ib_time_t expected_usecs)
{
    return CheckSecDiff(TvToSecs(tv1), TvToSecs(tv2), expected_usecs * 1e-6);
}

bool CheckDelta(const ib_timeval_t &tv1,
                const ib_timeval_t &tv2,
                double expected_secs)
{
    return CheckSecDiff(TvToSecs(tv1), TvToSecs(tv2), expected_secs);
}

bool CheckDelta(ib_time_t t1,
                ib_time_t t2,
                ib_time_t expected_usecs)
{
    return CheckUsecDiff(t1, t2, expected_usecs);
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
    double       t1;
    double       t2;

    usecs = 100000;
    ib_clock_gettimeofday(&tv1);
    t1 = GetTime( );
    usleep(usecs);
    ib_clock_gettimeofday(&tv2);
    t2 = GetTime( );
    rv = CheckDelta(tv1, tv2, t2 - t1);
    ASSERT_TRUE(rv);

    usecs = 500000;
    ib_clock_gettimeofday(&tv1);
    t1 = GetTime( );
    usleep(usecs);
    ib_clock_gettimeofday(&tv2);
    t2 = GetTime( );
    rv = CheckDelta(tv1, tv2, t2 - t1);
    ASSERT_TRUE(rv);

    usecs = 1000000;
    ib_clock_gettimeofday(&tv1);
    t1 = GetTime( );
    usleep(usecs);
    ib_clock_gettimeofday(&tv2);
    t2 = GetTime( );
    rv = CheckDelta(tv1, tv2, t2 - t1);
    ASSERT_TRUE(rv);
}

const size_t bufsize = 32;

void TestTimestamp(bool relative, int seconds)
{
    struct timeval tv;
    struct tm      tm;
    ib_timeval_t   itv;
    char           buf[bufsize + 1];
    time_t         t;
    bool           rv;

    ib_clock_gettimeofday(&itv);
    if (relative) {
        ib_time_t offset = static_cast<ib_time_t>(seconds) * 1000000;
        ib_clock_relative_timestamp(buf, &itv, offset);
    }
    else {
        seconds = 0;
        ib_clock_timestamp(buf, &itv);
    }

    /* Initialize tm using the time from above and localtime().  This ensures
     * that all of the tm structure is initialized properly, in particular
     * timezone info. */
    t = (time_t)(itv.tv_sec + seconds);
    localtime_r(&t, &tm);

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
        SCOPED_TRACE("test_timestamp");
        TestTimestamp(false, 0);
    }
}

TEST(TestClock, test_relative_timestamp)
{
    {
        SCOPED_TRACE("test_relative_timestamp: 0s");
        TestTimestamp(true, 0);
    }
    {
        SCOPED_TRACE("test_relative_timestamp: 1s");
        TestTimestamp(true, 1);
    }
    {
        SCOPED_TRACE("test_relative_timestamp: -1s");
        TestTimestamp(true, -1);
    }
    {
        SCOPED_TRACE("test_relative_timestamp: 60s");
        TestTimestamp(true, 60);
    }
    {
        SCOPED_TRACE("test_relative_timestamp: -60s");
        TestTimestamp(true, -60);
    }

}

/* Test ib_clock_relative_timestamp() using self-referential results.
 *
 * That is, ib_clock_relative_timestamp() produces the expected
 * result as well as the test result through different means.
 *
 * We first show that when the offset is 0 that the same timevalue is returned.
 * We show that when the offset is non-zero that a different timevalue is
 * returned.
 *
 * We then use this to justify comparing the outputs of shifed and
 * unshifted uses of ib_clock_relative_timestamp().
 */
TEST(TestClock, test_relative_timestamp2) {
    char buf[100];
    char expected[100];
    ib_timeval_t t;

    /* Prove the unshifted time produces the same output. */
    t.tv_sec = 10;
    t.tv_usec = 0;
    ib_clock_relative_timestamp(buf, &t, 0);
    ib_clock_relative_timestamp(expected, &t, 0);
    ASSERT_STREQ(expected, buf);

    /* Prove that the shifted time produces different output. */
    t.tv_sec = 10;
    t.tv_usec = 0;
    ib_clock_relative_timestamp(buf, &t, 1000000);
    ib_clock_relative_timestamp(expected, &t, 0);
    ASSERT_STRNE(expected, buf);

    /* Prove that negative shifting prodcues different output. */
    t.tv_sec = 10;
    t.tv_usec = 0;
    ib_clock_relative_timestamp(buf, &t, -1000000);
    ib_clock_relative_timestamp(expected, &t, 0);
    ASSERT_STRNE(expected, buf);

    /* Now show that 10s shifted -1s == unshifted 11s. */
    t.tv_sec = 10;
    t.tv_usec = 0;
    ib_clock_relative_timestamp(buf, &t, -1000000);
    t.tv_sec -= 1;
    ib_clock_relative_timestamp(expected, &t, 0);
    ASSERT_STREQ(expected, buf);

    /* Now show that 10s shifted -11s undeflows to 0. */
    t.tv_sec = 10;
    t.tv_usec = 0;
    ib_clock_relative_timestamp(buf, &t, -11000000);
    t.tv_sec = 0;
    ib_clock_relative_timestamp(expected, &t, 0);
    ASSERT_STREQ(expected, buf);

    /* Now show that 10s shifted +1s == unshifted 11s. */
    t.tv_sec = 10;
    t.tv_usec = 0;
    ib_clock_relative_timestamp(buf, &t, 1000000);
    t.tv_sec += 1;
    ib_clock_relative_timestamp(expected, &t, 0);
    ASSERT_STREQ(expected, buf);
}

TEST(TestClock, test_timeval_cmp)
{
    ib_timeval_t tv1;
    ib_timeval_t tv2;

    /* tv1 == tv1 */
    tv1.tv_sec  = 10;
    tv1.tv_usec = 0;
    ASSERT_TRUE(ib_clock_timeval_cmp(&tv1, &tv1) == 0);

    /* tv2 > tv1 */
    tv2.tv_sec  = 10;
    tv2.tv_usec = 1;
    ASSERT_TRUE(ib_clock_timeval_cmp(&tv1, &tv2) < 0);
    ASSERT_TRUE(ib_clock_timeval_cmp(&tv2, &tv1) > 0);

    /* tv2 < tv1 */
    tv2.tv_sec  = 9;
    tv2.tv_usec = 999999;
    ASSERT_TRUE(ib_clock_timeval_cmp(&tv1, &tv2) > 0);
    ASSERT_TRUE(ib_clock_timeval_cmp(&tv2, &tv1) < 0);

    /* tv2 < tv1 */
    tv1.tv_sec  = 10;
    tv1.tv_usec = 10;
    tv2.tv_sec  = 10;
    tv2.tv_usec = 9;
    ASSERT_TRUE(ib_clock_timeval_cmp(&tv1, &tv2) > 0);
    ASSERT_TRUE(ib_clock_timeval_cmp(&tv2, &tv1) < 0);

    /* tv2 > tv1 */
    tv2.tv_usec = 11;
    ASSERT_TRUE(ib_clock_timeval_cmp(&tv1, &tv2) < 0);
    ASSERT_TRUE(ib_clock_timeval_cmp(&tv2, &tv1) > 0);
}

TEST(TestClock, test_timeval_add)
{
    ib_timeval_t tv1;
    ib_timeval_t tv2;
    ib_timeval_t out;
    ib_timeval_t exp;
    const uint32_t sec_usec  = 1000000;
    const uint32_t max_usec  = (sec_usec - 1);
    const uint32_t half_usec = (sec_usec / 2);

    tv1.tv_sec  = 10;
    tv1.tv_usec = 0;
    tv2.tv_sec  = 10;
    tv2.tv_usec = 0;
    exp.tv_sec  = 20;
    exp.tv_usec = 0;
    ib_clock_timeval_add(&tv1, &tv2, &out);
    ASSERT_EQ(0, ib_clock_timeval_cmp(&out, &exp));

    tv1.tv_sec  = 10;
    tv1.tv_usec = max_usec;
    tv2.tv_sec  = 10;
    tv2.tv_usec = 1;
    exp.tv_sec  = 21;
    exp.tv_usec = 0;
    ib_clock_timeval_add(&tv1, &tv2, &out);
    ASSERT_EQ(0, ib_clock_timeval_cmp(&out, &exp));

    tv1.tv_sec  = 10;
    tv1.tv_usec = half_usec;
    tv2.tv_sec  = 10;
    tv2.tv_usec = half_usec;
    exp.tv_sec  = 21;
    exp.tv_usec = 0;
    ib_clock_timeval_add(&tv1, &tv2, &out);
    ASSERT_EQ(0, ib_clock_timeval_cmp(&out, &exp));

    tv1.tv_sec  = 10;
    tv1.tv_usec = half_usec - 1;
    tv2.tv_sec  = 10;
    tv2.tv_usec = half_usec;
    exp.tv_sec  = 20;
    exp.tv_usec = max_usec;
    ib_clock_timeval_add(&tv1, &tv2, &out);
    ASSERT_EQ(0, ib_clock_timeval_cmp(&out, &exp));

    tv1.tv_sec  = 10;
    tv1.tv_usec = half_usec - 1;
    tv2.tv_sec  = 10;
    tv2.tv_usec = half_usec - 1;
    exp.tv_sec  = 20;
    exp.tv_usec = sec_usec - 2;
    ib_clock_timeval_add(&tv1, &tv2, &out);
    ASSERT_EQ(0, ib_clock_timeval_cmp(&out, &exp));

    tv1.tv_sec  = 10;
    tv1.tv_usec = half_usec + 1;
    tv2.tv_sec  = 10;
    tv2.tv_usec = half_usec - 1;
    exp.tv_sec  = 21;
    exp.tv_usec = 0;
    ib_clock_timeval_add(&tv1, &tv2, &out);
    ASSERT_EQ(0, ib_clock_timeval_cmp(&out, &exp));

    tv1.tv_sec  = 10;
    tv1.tv_usec = half_usec + 1;
    tv2.tv_sec  = 10;
    tv2.tv_usec = half_usec;
    exp.tv_sec  = 21;
    exp.tv_usec = 1;
    ib_clock_timeval_add(&tv1, &tv2, &out);
    ASSERT_EQ(0, ib_clock_timeval_cmp(&out, &exp));

    /* Add to tv1 */
    tv1.tv_sec  = 10;
    tv1.tv_usec = 1;
    tv2.tv_sec  = 10;
    tv2.tv_usec = 1;
    exp.tv_sec  = 20;
    exp.tv_usec = 2;
    ib_clock_timeval_add(&tv1, &tv2, &tv1);
    ASSERT_EQ(0, ib_clock_timeval_cmp(&tv1, &exp));

    /* Add to tv2 */
    tv1.tv_sec  = 10;
    tv1.tv_usec = 1;
    tv2.tv_sec  = 10;
    tv2.tv_usec = 1;
    exp.tv_sec  = 20;
    exp.tv_usec = 2;
    ib_clock_timeval_add(&tv1, &tv2, &tv2);
    ASSERT_EQ(0, ib_clock_timeval_cmp(&tv2, &exp));
}

