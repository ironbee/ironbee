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
 * @brief IronBee++ Internals &mdash; Clock Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/clock.hpp>

#include "gtest/gtest.h"

using namespace boost::posix_time;
using namespace IronBee;

TEST(Clock, Basic)
{
    ib_time_t ib = 0;
    ptime p = from_time_t(0);

    EXPECT_EQ(p, ib_to_ptime(ib));
    EXPECT_EQ(ib, ptime_to_ib(p));
    EXPECT_EQ(p, ib_to_ptime(ptime_to_ib(p)));
    EXPECT_EQ(ib, ptime_to_ib(ib_to_ptime(ib)));

    ib = 17;
    p += microseconds(17);

    EXPECT_EQ(p, ib_to_ptime(ib));
    EXPECT_EQ(ib, ptime_to_ib(p));
    EXPECT_EQ(p, ib_to_ptime(ptime_to_ib(p)));
    EXPECT_EQ(ib, ptime_to_ib(ib_to_ptime(ib)));

    ptime now = microsec_clock::universal_time();
    EXPECT_EQ(now, ib_to_ptime(ptime_to_ib(now)));
}
