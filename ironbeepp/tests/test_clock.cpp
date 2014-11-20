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

/**
 * @file
 * @brief IronBee++ Internals --- Clock Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/clock.hpp>

#include "gtest/gtest.h"

using namespace boost::posix_time;
using namespace IronBee;

TEST(Clock, Basic)
{
    ib_timeval_t tv_ib = { 0, 0 };
    ib_time_t t_ib = 0;
    ptime p = from_time_t(0);

    EXPECT_EQ(p, ib_to_ptime(tv_ib));
    EXPECT_EQ(t_ib, ptime_to_ib(p));

    tv_ib.tv_sec = 17;
    tv_ib.tv_usec = 492;
    t_ib = 17000492;
    p += seconds(17) + microseconds(492);

    EXPECT_EQ(p, ib_to_ptime(tv_ib));
    EXPECT_EQ(t_ib, ptime_to_ib(p));

    tv_ib.tv_sec = 1340857461;
    tv_ib.tv_usec = 492;
    t_ib = 1340857461000492 + 4;
    p = from_time_t(0);
    p += seconds(1340857461) + microseconds(492) + microseconds(4);

    EXPECT_EQ(p, ib_to_ptime(tv_ib, 4));
    EXPECT_EQ(t_ib, ptime_to_ib(p));

    ptime now = microsec_clock::universal_time();
    ptime now_plus_4 = now + microseconds(4);
    t_ib = ptime_to_ib(now);
    IB_CLOCK_TIMEVAL(tv_ib, t_ib);
    EXPECT_EQ(now_plus_4, ib_to_ptime(tv_ib, 4));
}

TEST(Clock, Parsing) {
    /* Failure results in 0. */
    EXPECT_EQ(0, parse_ib_time("foo"));
    /* RFC-1123. */
    EXPECT_EQ(1416358923000000, parse_ib_time("Wed, 19 Nov 2014 01:02:03 GMT"));
    /* RFC-850. */
    EXPECT_EQ(1416358923000000, parse_ib_time("Wednesday, 19-Nov-14 01:02:03 GMT"));
    /* ASC Time. */
    EXPECT_EQ(1416358923000000, parse_ib_time("Wed Nov 19 01:02:03 2014"));

}