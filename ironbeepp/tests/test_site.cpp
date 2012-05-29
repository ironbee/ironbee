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
 * @brief IronBee++ Internals &mdash; Site Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/site.hpp>
#include "fixture.hpp"

#include "gtest/gtest.h"

#include <ironbee/engine.h>

using namespace std;
using namespace IronBee;

class TestSite : public ::testing::Test, public IBPPTestFixture
{
};

TEST_F(TestSite, Location)
{
    Site s = Site::create(m_engine, "test");
    ASSERT_TRUE(s);

    Location l = s.create_location("foo");
    ASSERT_TRUE(l);
    EXPECT_EQ(s, l.site());
    EXPECT_EQ(string("foo"), l.path());

    l.set_path("bar");
    EXPECT_EQ(string("bar"), l.path());

    l = s.create_default_location();
    EXPECT_EQ(s, l.site());
}

TEST_F(TestSite, Site)
{
    Site s = Site::create(m_engine, "test");
    ASSERT_TRUE(s);

    EXPECT_EQ(string("test"), s.name());
    EXPECT_EQ(m_engine, s.engine());
    EXPECT_TRUE(s.memory_pool());

    Location l = s.create_default_location();
    EXPECT_EQ(l, s.default_location());

    s.add_ip("1.2.3.4");
    EXPECT_EQ(1UL, s.ips().size());
    EXPECT_EQ(string("1.2.3.4"), s.ips().front());

    s.add_host("foo");
    EXPECT_EQ(1UL, s.hosts().size());
    EXPECT_EQ(string("foo"), s.hosts().front());

    l = s.create_location("/foo");
    EXPECT_EQ(1UL, s.locations().size());
    EXPECT_EQ(l, s.locations().front());
}
