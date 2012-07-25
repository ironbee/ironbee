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
/// @brief IronBee &mdash; IP Set tests
///
/// @author Christopher Alfeld <calfeld@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include <iostream>

#include "ironbee_config_auto.h"
#include "gtest/gtest.h"

#include <ironbee/ip.h>

using namespace std;

namespace {

ib_ip4_t ip4(int a, int b, int c, int d)
{
    return (a << 24) + (b << 16) + (c << 8) + d;
}

ib_ip4_network_t net4(int a, int b, int c, int d, size_t s)
{
    ib_ip4_network_t net;
    net.ip = ip4(a, b, c, d);
    net.size = s;

    return net;
}

}

static bool operator==(
    const ib_ip4_network_t& a,
    const ib_ip4_network_t& b
)
{
    return (a.ip == b.ip) && (a.size == b.size);
}

static ostream& operator<<(ostream& o, const ib_ip4_network_t& net)
{
    o << (net.ip >> 24)          << "."
      << ((net.ip >> 16) & 0xff) << "."
      << ((net.ip >> 8)  & 0xff) << "."
      << (net.ip         & 0xff)
      << "/" << int(net.size);
    return o;
}

TEST(TestIP, ip4_str_to_ip)
{
    ib_ip4_t ip;
    ib_status_t rc;

    rc = ib_ip4_str_to_ip("1.2.3.4", &ip);
    EXPECT_EQ(IB_OK, rc);
    EXPECT_EQ(ip, ip4(1, 2, 3, 4));
    rc = ib_ip4_str_to_ip("1.2.3.4", NULL);
    EXPECT_EQ(IB_OK, rc);
    rc = ib_ip4_str_to_ip("0.0.0.0", &ip);
    EXPECT_EQ(IB_OK, rc);
    EXPECT_EQ(ip, ip4(0, 0, 0, 0));
    rc = ib_ip4_str_to_ip("255.255.255.255", &ip);
    EXPECT_EQ(IB_OK, rc);
    EXPECT_EQ(ip, ip4(255, 255, 255, 255));

    rc = ib_ip4_str_to_ip("", &ip);
    EXPECT_EQ(IB_EINVAL, rc);
    rc = ib_ip4_str_to_ip(NULL, &ip);
    EXPECT_EQ(IB_EINVAL, rc);
    rc = ib_ip4_str_to_ip("foobar", NULL);
    EXPECT_EQ(IB_EINVAL, rc);
    rc = ib_ip4_str_to_ip("-5.2.3.4", NULL);
    EXPECT_EQ(IB_EINVAL, rc);
    rc = ib_ip4_str_to_ip("256.2.3.4", NULL);
    EXPECT_EQ(IB_EINVAL, rc);
    rc = ib_ip4_str_to_ip("1.2.3.4hello", NULL);
    EXPECT_EQ(IB_EINVAL, rc);
}

TEST(TestIP, ip4_str_to_net)
{
    ib_ip4_network_t net;
    ib_status_t rc;

    rc = ib_ip4_str_to_net("1.2.3.4/16", &net);
    EXPECT_EQ(IB_OK, rc);
    EXPECT_EQ(net, net4(1, 2, 3, 4, 16));
    rc = ib_ip4_str_to_net("1.2.3.4/16", NULL);
    EXPECT_EQ(IB_OK, rc);
    rc = ib_ip4_str_to_net("1.2.3.4/0", &net);
    EXPECT_EQ(IB_OK, rc);
    EXPECT_EQ(net, net4(1, 2, 3, 4, 0));
    rc = ib_ip4_str_to_net("1.2.3.4/32", &net);
    EXPECT_EQ(IB_OK, rc);
    EXPECT_EQ(net, net4(1, 2, 3, 4, 32));

    rc = ib_ip4_str_to_net("", &net);
    EXPECT_EQ(IB_EINVAL, rc);
    rc = ib_ip4_str_to_net(NULL, &net);
    EXPECT_EQ(IB_EINVAL, rc);
    rc = ib_ip4_str_to_net("foobar", NULL);
    EXPECT_EQ(IB_EINVAL, rc);
    rc = ib_ip4_str_to_net("1.2.3.4", NULL);
    EXPECT_EQ(IB_EINVAL, rc);
    rc = ib_ip4_str_to_net("-5.2.3.4/16", NULL);
    EXPECT_EQ(IB_EINVAL, rc);
    rc = ib_ip4_str_to_net("1.2.3.4/-16", NULL);
    EXPECT_EQ(IB_EINVAL, rc);
    rc = ib_ip4_str_to_net("1.2.3.4/33", NULL);
    EXPECT_EQ(IB_EINVAL, rc);
    rc = ib_ip4_str_to_net("1.2.3.4/16hello", NULL);
    EXPECT_EQ(IB_EINVAL, rc);
}
