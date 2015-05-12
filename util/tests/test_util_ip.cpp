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
/// @brief IronBee --- IP Set tests
///
/// @author Christopher Alfeld <calfeld@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include <iostream>

#include "ironbee_config_auto.h"
#include "gtest/gtest.h"

#include <ironbee/ip.h>

#ifdef __clang__
#pragma clang diagnostic push
#if __has_warning("-Wunused-local-typedef")
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
#endif
#include <boost/format.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

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

ib_ip6_t ip6(
    uint16_t a1, uint16_t a2,
    uint16_t b1, uint16_t b2,
    uint16_t c1, uint16_t c2,
    uint16_t d1, uint16_t d2
)
{
    ib_ip6_t ip;

    ip.ip[0] = (a1 << 16) | a2;
    ip.ip[1] = (b1 << 16) | b2;
    ip.ip[2] = (c1 << 16) | c2;
    ip.ip[3] = (d1 << 16) | d2;

    return ip;
}

ib_ip6_network_t net6(
    uint16_t a1, uint16_t a2,
    uint16_t b1, uint16_t b2,
    uint16_t c1, uint16_t c2,
    uint16_t d1, uint16_t d2,
    size_t s
)
{
    ib_ip6_network_t net;

    net.ip = ip6(a1, a2, b1, b2, c1, c2, d1, d2);
    net.size = s;

    return net;
}

}

static
bool operator==(
    const ib_ip4_network_t& a,
    const ib_ip4_network_t& b
)
{
    return (a.ip == b.ip) && (a.size == b.size);
}

static
bool operator==(
    const ib_ip6_t& a,
    const ib_ip6_t& b
)
{
    return
        a.ip[0] == b.ip[0] &&
        a.ip[1] == b.ip[1] &&
        a.ip[2] == b.ip[2] &&
        a.ip[3] == b.ip[3];
}

static
bool operator==(
    const ib_ip6_network_t& a,
    const ib_ip6_network_t& b
)
{
    return (a.ip == b.ip) && (a.size == b.size);
}

static
ostream& operator<<(ostream& o, const ib_ip4_network_t& net)
{
    o << (net.ip >> 24)          << "."
      << ((net.ip >> 16) & 0xff) << "."
      << ((net.ip >> 8)  & 0xff) << "."
      << (net.ip         & 0xff)
      << "/" << int(net.size);
    return o;
}

static
ostream& operator<<(ostream& o, const ib_ip6_t& ip)
{
    for (int i = 0; i < 4; ++i) {
        o << boost::format("%x:%x") % (ip.ip[i] >> 16) % (ip.ip[i] & 0xffff);
        if (i < 3) {
            o << ":";
        }
    }

    return o;
}

static
ostream& operator<<(ostream& o, const ib_ip6_network_t& net)
{
    o << net.ip << "/" << int(net.size);
    return o;
}


TEST(TestIP, ip4_str_to_ip)
{
    ib_ip4_t ip;
    ib_status_t rc;

    rc = ib_ip4_str_to_ip("1.2.3.4", &ip);
    EXPECT_EQ(IB_OK, rc);
    EXPECT_EQ(ip4(1, 2, 3, 4), ip);
    rc = ib_ip4_str_to_ip("1.2.3.4", NULL);
    EXPECT_EQ(IB_OK, rc);
    rc = ib_ip4_str_to_ip("0.0.0.0", &ip);
    EXPECT_EQ(IB_OK, rc);
    EXPECT_EQ(ip4(0, 0, 0, 0), ip);
    rc = ib_ip4_str_to_ip("255.255.255.255", &ip);
    EXPECT_EQ(IB_OK, rc);
    EXPECT_EQ(ip4(255, 255, 255, 255), ip);

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
    EXPECT_EQ(net4(1, 2, 3, 4, 16), net);
    rc = ib_ip4_str_to_net("1.2.3.4/16", NULL);
    EXPECT_EQ(IB_OK, rc);
    rc = ib_ip4_str_to_net("1.2.3.4/0", &net);
    EXPECT_EQ(IB_OK, rc);
    EXPECT_EQ(net4(1, 2, 3, 4, 0), net);
    rc = ib_ip4_str_to_net("1.2.3.4/32", &net);
    EXPECT_EQ(IB_OK, rc);
    EXPECT_EQ(net4(1, 2, 3, 4, 32), net);

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

TEST(TestIP, ip6_str_to_ip)
{
    ib_ip6_t ip;
    ib_status_t rc;

    rc = ib_ip6_str_to_ip("::1", &ip);
    EXPECT_EQ(IB_OK, rc);
    EXPECT_EQ(ip6(0, 0, 0, 0, 0, 0, 0, 1), ip);

    rc = ib_ip6_str_to_ip("1::", &ip);
    EXPECT_EQ(IB_OK, rc);
    EXPECT_EQ(ip6(1, 0, 0, 0, 0, 0, 0, 0), ip);

    rc = ib_ip6_str_to_ip("1:2:3:4:5:6:7:8", &ip);
    EXPECT_EQ(IB_OK, rc);
    EXPECT_EQ(ip6(1, 2, 3, 4, 5, 6, 7, 8), ip);
    rc = ib_ip6_str_to_ip("1:2:3:4::6:7:8", &ip);
    EXPECT_EQ(IB_OK, rc);
    EXPECT_EQ(ip6(1, 2, 3, 4, 0, 6, 7, 8), ip);
    rc = ib_ip6_str_to_ip("1:2:3::6:7:8", &ip);
    EXPECT_EQ(IB_OK, rc);
    EXPECT_EQ(ip6(1, 2, 3, 0, 0, 6, 7, 8), ip);
    rc = ib_ip6_str_to_ip("1:2::6:7:8", &ip);
    EXPECT_EQ(IB_OK, rc);
    EXPECT_EQ(ip6(1, 2, 0, 0, 0, 6, 7, 8), ip);
    rc = ib_ip6_str_to_ip("1:2::7:8", &ip);
    EXPECT_EQ(IB_OK, rc);
    EXPECT_EQ(ip6(1, 2, 0, 0, 0, 0, 7, 8), ip);
    rc = ib_ip6_str_to_ip("1:2::8", &ip);
    EXPECT_EQ(IB_OK, rc);
    EXPECT_EQ(ip6(1, 2, 0, 0, 0, 0, 0, 8), ip);
    rc = ib_ip6_str_to_ip("1::8", &ip);
    EXPECT_EQ(IB_OK, rc);
    EXPECT_EQ(ip6(1, 0, 0, 0, 0, 0, 0, 8), ip);
    rc = ib_ip6_str_to_ip("aaaa:bbbb:cccc:AbAb:DDDD:abCd:0:dF", &ip);
    EXPECT_EQ(IB_OK, rc);
    EXPECT_EQ(ip6(0xaaaa, 0xbbbb, 0xcccc, 0xabab, 0xdddd, 0xabcd, 0, 0x00df), ip);
}

TEST(TestIP, ip6_str_to_net)
{
    ib_ip6_network_t net;
    ib_status_t rc;

    rc = ib_ip6_str_to_net("::1/128", &net);
    EXPECT_EQ(IB_OK, rc);
    EXPECT_EQ(net6(0, 0, 0, 0, 0, 0, 0, 1, 128), net);
    rc = ib_ip6_str_to_net("1:2:3:4:5:6:7:8/64", &net);
    EXPECT_EQ(IB_OK, rc);
    EXPECT_EQ(net6(1, 2, 3, 4, 5, 6, 7, 8, 64), net);
    rc = ib_ip6_str_to_net("1:2:3:4:5:6:7:8/64hello", NULL);
    EXPECT_EQ(IB_EINVAL, rc);
    rc = ib_ip6_str_to_net("1:2:3:4:5:6:7:8/129", NULL);
    EXPECT_EQ(IB_EINVAL, rc);
    rc = ib_ip6_str_to_net("1:2:3:4:5:6:7:8/-5", NULL);
    EXPECT_EQ(IB_EINVAL, rc);
}

TEST(TestIP, ip_validate)
{
    EXPECT_EQ(IB_OK, ib_ip_validate("1.2.3.4"));
    EXPECT_EQ(IB_OK, ib_ip_validate("::1"));
    EXPECT_EQ(IB_OK, ib_ip_validate("1:2:3:4:5:6:7:8"));
    EXPECT_EQ(IB_OK, ib_ip_validate("::ffff:1.2.3.4"));

    EXPECT_EQ(IB_EINVAL, ib_ip_validate("foobar"));
    EXPECT_EQ(IB_EINVAL, ib_ip_validate("1.2.3.4foobar"));
    EXPECT_EQ(IB_EINVAL, ib_ip_validate("1.2.3.4:ffff::"));
}
