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
/// @brief IronBee &mdash; IP Address Test Functions
/// 
/// @author Nick LeRoy <nleroy@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "ironbee_config_auto.h"

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

#include <ironbee/util.h>
#include <ironbee/ip_addr.h>
#include <ironbee/mpool.h>
#include <ironbee/debug.h>

/* @test Test util IP Address library - ib_ipaddr_is_ipv4_ex() */
TEST(TestIBUtilIpAddr, test_ipaddr_is_ipv4_ex)
{
    ib_status_t rc;
    bool result;
    const char *ascii1 = "192.168.1.10";
    const char *ascii2 = "AAAA:BBBB::1";

    const char *ascii3 = "192.168.2.0/23";
    const char *ascii4 = "AAAA:BBBB::1/111";

    const char ascii5[] = "192.168.2.0\0/23";
    const char ascii6[] = "AA\0AA:BBBB::1/111";

    
    rc = ib_initialize();
    ASSERT_EQ(IB_OK, rc);

    /* IPV4 prefix */
    rc = ib_ipaddr_is_ipv4_ex(ascii1, strlen(ascii1), false, &result);
    ASSERT_EQ(IB_OK, rc);

    /* Check the result */
    ASSERT_TRUE(result);

    /* IPV6 prefix */
    rc = ib_ipaddr_is_ipv4_ex(ascii2, strlen(ascii2), false, &result);
    ASSERT_EQ(IB_OK, rc);

    /* Check the result */
    ASSERT_FALSE(result);

    /* IPV4 prefix */
    rc = ib_ipaddr_is_ipv4_ex(ascii3, strlen(ascii3), false, &result);
    ASSERT_NE(IB_OK, rc);

    /* IPV4 prefix */
    rc = ib_ipaddr_is_ipv4_ex(ascii3, strlen(ascii3), true, &result);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(result);

    /* IPV6 prefix */
    rc = ib_ipaddr_is_ipv4_ex(ascii4, strlen(ascii4), false, &result);
    ASSERT_NE(IB_OK, rc);

    /* IPV6 prefix */
    rc = ib_ipaddr_is_ipv4_ex(ascii4, strlen(ascii4), true, &result);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_FALSE(result);

    /* Invalid IPV4 prefix */
    rc = ib_ipaddr_is_ipv4_ex(ascii5, sizeof(ascii5), true, &result);
    ASSERT_NE(IB_OK, rc);

    /* IPV6 prefix */
    rc = ib_ipaddr_is_ipv4_ex(ascii6, sizeof(ascii6), true, &result);
    ASSERT_NE(IB_OK, rc);
}

/* @test Test util IP Address library - ib_ipaddr_is_ipv6_ex() */
TEST(TestIBUtilIpAddr, test_ipaddr_is_ipv6_ex)
{
    ib_status_t rc;
    bool result;
    const char *ascii1 = "192.168.1.10";
    const char *ascii2 = "AAAA:BBBB::1";
    const char *ascii3 = "192.168.2.0/23";
    const char *ascii4 = "AAAA:BBBB::1/111";
    const char ascii5[] = "192.168.2.0\0/23";
    const char ascii6[] = "AA\0AA:BBBB::1/111";

    rc = ib_initialize();
    ASSERT_EQ(IB_OK, rc);

    /* IPV4 prefix */
    rc = ib_ipaddr_is_ipv6_ex(ascii1, strlen(ascii1), false, &result);
    ASSERT_EQ(IB_OK, rc);

    /* Check the result */
    //ASSERT_EQ(false, result);
    ASSERT_FALSE(result);

    /* IPV6 prefix */
    rc = ib_ipaddr_is_ipv6_ex(ascii2, strlen(ascii2), false, &result);
    ASSERT_EQ(IB_OK, rc);

    /* Check the result */
    ASSERT_TRUE(result);

    /* IPV4 prefix */
    rc = ib_ipaddr_is_ipv6_ex(ascii3, strlen(ascii3), false, &result);
    ASSERT_NE(IB_OK, rc);

    /* IPV4 prefix */
    rc = ib_ipaddr_is_ipv6_ex(ascii3, strlen(ascii3), true, &result);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_FALSE(result);

    /* IPV6 prefix */
    rc = ib_ipaddr_is_ipv6_ex(ascii4, strlen(ascii4), false, &result);
    ASSERT_NE(IB_OK, rc);

    /* IPV6 prefix */
    rc = ib_ipaddr_is_ipv6_ex(ascii4, strlen(ascii4), true, &result);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(result);

    /* Invalid IPV4 prefix */
    rc = ib_ipaddr_is_ipv6_ex(ascii5, sizeof(ascii5), true, &result);
    ASSERT_NE(IB_OK, rc);

    /* IPV6 prefix */
    rc = ib_ipaddr_is_ipv6_ex(ascii6, sizeof(ascii6), true, &result);
    ASSERT_NE(IB_OK, rc);
}

/* @test Test util IP Address library - ib_ipaddr_is_ip_ex() */
TEST(TestIBUtilIpAddr, test_ipaddr_is_ip_ex)
{
    ib_status_t rc;
    bool ipv4;
    bool ipv6;

    const char *invalid1 = "";
    const char *invalid2 = "192.168.1.1a";
    const char *invalid3 = "192.a.1.2";
    const char *invalid4 = "AAAA:BBBX::1";
    const char *invalid5 = "192.168.2.";
    const char *invalid6 = "www.foo.com";
    const char *invalid7 = "http://www.foo.com";
    const char invalid8[] = "192.168.2.0\0/23";
    const char invalid9[] = "AA\0AA:BBBB::1/111";

    const char *ascii1 = "192.168.1.10";
    const char *ascii2 = "AAAA:BBBB::1";

    const char *ascii3 = "192.168.2.0/23";
    const char *ascii4 = "AAAA:BBBB::1/111";

    
    rc = ib_initialize();
    ASSERT_EQ(IB_OK, rc);

    /* IPV4 prefix */
    rc = ib_ipaddr_is_ip_ex(ascii1, strlen(ascii1), true, &ipv4, &ipv6);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(ipv4);
    ASSERT_FALSE(ipv6);

    /* With NULL for ipv4 */
    rc = ib_ipaddr_is_ip_ex(ascii1, strlen(ascii1), true, &ipv4, NULL);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(ipv4);

    /* With NULL for ipv6 */
    rc = ib_ipaddr_is_ip_ex(ascii1, strlen(ascii1), true, NULL, &ipv6);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_FALSE(ipv6);


    /* IPV6 address */
    rc = ib_ipaddr_is_ip_ex(ascii2, strlen(ascii2), true, &ipv4, &ipv6);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_FALSE(ipv4);
    ASSERT_TRUE(ipv6);

    /* With NULL for ipv4 */
    rc = ib_ipaddr_is_ip_ex(ascii2, strlen(ascii2), true, &ipv4, NULL);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_FALSE(ipv4);

    /* With NULL for ipv6 */
    rc = ib_ipaddr_is_ip_ex(ascii2, strlen(ascii2), true, NULL, &ipv6);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(ipv6);


    /* IPV4 prefix */
    rc = ib_ipaddr_is_ip_ex(ascii3, strlen(ascii3), true, &ipv4, &ipv6);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(ipv4);
    ASSERT_FALSE(ipv6);

    /* IPV6 prefix */
    rc = ib_ipaddr_is_ip_ex(ascii4, strlen(ascii4), true, &ipv4, &ipv6);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_FALSE(ipv4);
    ASSERT_TRUE(ipv6);


    /* Invalid checks */
    rc = ib_ipaddr_is_ip_ex(invalid1, strlen(invalid1), true, &ipv4, &ipv6);
    ASSERT_NE(IB_OK, rc);
    ASSERT_FALSE(ipv4);
    ASSERT_FALSE(ipv6);

    rc = ib_ipaddr_is_ip_ex(invalid2, strlen(invalid2), true, &ipv4, &ipv6);
    ASSERT_NE(IB_OK, rc);
    ASSERT_FALSE(ipv4);
    ASSERT_FALSE(ipv6);

    rc = ib_ipaddr_is_ip_ex(invalid3, strlen(invalid3), true, &ipv4, &ipv6);
    ASSERT_NE(IB_OK, rc);
    ASSERT_FALSE(ipv4);
    ASSERT_FALSE(ipv6);

    rc = ib_ipaddr_is_ip_ex(invalid4, strlen(invalid4), true, &ipv4, &ipv6);
    ASSERT_NE(IB_OK, rc);
    ASSERT_FALSE(ipv4);
    ASSERT_FALSE(ipv6);

    rc = ib_ipaddr_is_ip_ex(invalid5, strlen(invalid5), true, &ipv4, &ipv6);
    ASSERT_NE(IB_OK, rc);
    ASSERT_FALSE(ipv4);
    ASSERT_FALSE(ipv6);

    /* Host name */
    rc = ib_ipaddr_is_ip_ex(invalid6, strlen(invalid6), true, &ipv4, &ipv6);
    ASSERT_NE(IB_OK, rc);
    ASSERT_FALSE(ipv4);
    ASSERT_FALSE(ipv6);

    /* URL */
    rc = ib_ipaddr_is_ip_ex(invalid7, strlen(invalid7), true, &ipv4, &ipv6);
    ASSERT_NE(IB_OK, rc);
    ASSERT_FALSE(ipv4);
    ASSERT_FALSE(ipv6);

    /* Invalid IPV4 prefix */
    rc = ib_ipaddr_is_ip_ex(invalid8, sizeof(invalid8), true, &ipv4, &ipv6);
    ASSERT_NE(IB_OK, rc);
    ASSERT_FALSE(ipv4);
    ASSERT_FALSE(ipv6);

    /* Invalid IPV6 prefix */
    rc = ib_ipaddr_is_ip_ex(invalid9, sizeof(invalid9), true, &ipv4, &ipv6);
    ASSERT_NE(IB_OK, rc);
    ASSERT_FALSE(ipv4);
    ASSERT_FALSE(ipv6);

    /* Verify that it handles NULLs for ipv4 / ipv6 */
    rc = ib_ipaddr_is_ip_ex(invalid1, strlen(invalid1), true, NULL, NULL);
    ASSERT_NE(IB_OK, rc);

    rc = ib_ipaddr_is_ip_ex(invalid1, strlen(invalid1), true, &ipv4, NULL);
    ASSERT_NE(IB_OK, rc);
    ASSERT_FALSE(ipv4);

    rc = ib_ipaddr_is_ip_ex(invalid1, strlen(invalid1), true, NULL, &ipv6);
    ASSERT_NE(IB_OK, rc);
    ASSERT_FALSE(ipv6);

}
