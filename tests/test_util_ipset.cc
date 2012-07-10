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

#include <ironbee/ipset.h>

#include <stdexcept>
#include <set>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdelete-non-virtual-dtor"
#endif
#include <boost/random.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

using namespace std;

class TestIPSet : public ::testing::Test
{
protected:
    // Helper routines.
    
    /** Chose a random integer uniformly from [@a min, @a max]. */
    size_t random(size_t min, size_t max)
    {
        static boost::random::mt19937 rng;
        return boost::random::uniform_int_distribution<size_t>(min, max)(rng);
    }
    
    /** Construct v4 IP from 4 chars. */
    ib_ipset4_ip_t ip4(char a, char b, char c, char d)
    {
        return (a << 24) + (b << 16) + (c << 8) + d;
    }
    
    /** Construct v4 network from 4 chars and number of bits. */
    ib_ipset4_network_t net4(char a, char b, char c, char d, size_t bits)
    {
        ib_ipset4_network_t result;
        result.ip = ip4(a, b, c, d);
        result.size = bits;
        
        return result;
    }
    
    /** Construct v4 entry from chars, bits, and data. */
    ib_ipset4_entry_t entry4(
        char a, char b, char c, char d, size_t bits, 
        void* data = NULL
    )
    {
        ib_ipset4_entry_t result;
        result.network = net4(a, b, c, d, bits);
        result.data = data;
        
        return result;
    }
     
    /** Construct a v6 IP from four uint32_t. */   
    ib_ipset6_ip_t ip6(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
    {
        ib_ipset6_ip_t ip;
        ip.ip[0] = a;
        ip.ip[1] = b;
        ip.ip[2] = c;
        ip.ip[3] = d;
        return ip;
    }
    
    /** Construct a v6 network from four uint32_t and the number of bits. */
    ib_ipset6_network_t net6(
        uint32_t a, uint32_t b, uint32_t c, uint32_t d, size_t bits
    )
    {
        ib_ipset6_network_t result;
        result.ip = ip6(a, b, c, d);
        result.size = bits;
        
        return result;
    }
    
    /** Construct a v6 entry from ints, bits, and data. */
    ib_ipset6_entry_t entry6(
        uint32_t a, uint32_t b, uint32_t c, uint32_t d, size_t bits, 
        void* data = NULL
    )
    {
        ib_ipset6_entry_t result;
        result.network = net6(a, b, c, d, bits);
        result.data = data;
        
        return result;
    }
    
    /** Set bit @a bit to @a value in @a ip. */
    void set_bit(ib_ipset4_ip_t& ip, size_t bit, int value = 1)
    {
        ip |= (value << (31 - bit));
    }
    
    /** Overload of above for v6 IPs. */
    void set_bit(ib_ipset6_ip_t& ip, size_t bit, int value = 1)
    {
        set_bit(ip.ip[bit / 32], bit % 32, value);
    }
    
    /** Set @a ip to be @a num_ones 1s followed by zeros. */
    void make_ones(ib_ipset4_ip_t& ip, size_t num_ones)
    {
        ip = 0xffffffff;
        if (num_ones < 32) {
            ip = ~(0xffffffff >> num_ones);
        }
    }
    
    /** Overload of the above for v6 IPs. */
    void make_ones(ib_ipset6_ip_t& ip, size_t num_ones)
    {
        ip.ip[0] = ip.ip[1] = ip.ip[2] = ip.ip[3] = 0;
        for (size_t i = 0; i < num_ones / 32; ++i) {
            ip.ip[i] = 0xffffffff;
        }
        if (num_ones % 32 != 0) {
            make_ones(ip.ip[num_ones / 32], num_ones % 32);
        }
    }
};

/** Equality for v6 IPs. */
bool operator==(const ib_ipset6_ip_t& a, const ib_ipset6_ip_t& b)
{
    return
        a.ip[0] == b.ip[0] &&
        a.ip[1] == b.ip[1] &&
        a.ip[2] == b.ip[2] &&
        a.ip[3] == b.ip[3];
}

/** Lexicographical ordering from v6 IPs. */
bool operator<(const ib_ipset6_ip_t& a, const ib_ipset6_ip_t& b)
{
    for (size_t i = 0; i < 4; ++i) {
        if (a.ip[i] < b.ip[i]) {
            return true;
        }
        if (a.ip[i] > b.ip[i]) {
            return false;
        }
    }
    return false;
}

TEST_F(TestIPSet, TrivialCreation)
{
    ib_status_t rc;
    
    {
        ib_ipset4_t set;

        rc = ib_ipset4_init(&set, NULL, 0, NULL, 0);
        EXPECT_EQ(IB_OK, rc);
    }
    
    {
        ib_ipset6_t set;

        rc = ib_ipset6_init(&set, NULL, 0, NULL, 0);
        EXPECT_EQ(IB_OK, rc);
    }
}

TEST_F(TestIPSet, Simple4)
{
    ib_status_t rc;
    ib_ipset4_t set;
    vector<ib_ipset4_entry_t> positive;
    vector<ib_ipset4_entry_t> negative;
    
    positive.push_back(entry4(1, 0, 0, 0, 8));
    negative.push_back(entry4(1, 2, 3, 0, 24));
    
    rc = ib_ipset4_init(
        &set,
        negative.data(), negative.size(),
        positive.data(), positive.size()
    );
    ASSERT_EQ(IB_OK, rc);
    
    const ib_ipset4_entry_t* result;
    rc = ib_ipset4_query(&set, ip4(1, 2, 100, 20), &result, NULL, NULL);
    ASSERT_EQ(IB_OK, rc);
    EXPECT_EQ(ip4(1, 0, 0, 0), result->network.ip);
    EXPECT_EQ(8, result->network.size);
    rc = ib_ipset4_query(&set, ip4(1, 2, 3,   20), &result, NULL, NULL);
    EXPECT_EQ(IB_ENOENT, rc);
    EXPECT_FALSE(result);
    rc = ib_ipset4_query(&set, ip4(3, 2, 3,   20), &result, NULL, NULL);
    EXPECT_EQ(IB_ENOENT, rc);
    EXPECT_FALSE(result);
}

TEST_F(TestIPSet, Complex4)
{
    ib_status_t rc;
    ib_ipset4_t set;
    vector<ib_ipset4_entry_t> positive;
    vector<ib_ipset4_entry_t> negative;
    
    static int marker_a = 1;
    static int marker_b = 2;
    static int marker_c = 3;
    
    positive.push_back(entry4(2, 1, 0, 0, 16));
    positive.push_back(entry4(2, 5, 0, 0, 16));
    positive.push_back(entry4(2, 4, 0, 0, 16));
    positive.push_back(entry4(2, 6, 1, 0, 24));
    positive.push_back(entry4(1, 0, 0, 0, 8, &marker_a));
    positive.push_back(entry4(2, 0, 0, 0, 8));
    positive.push_back(entry4(2, 3, 0, 0, 16, &marker_b));
    positive.push_back(entry4(2, 3, 1, 0, 24, &marker_c));
    positive.push_back(entry4(2, 2, 0, 0, 16));
    
    negative.push_back(entry4(2, 5, 128, 0, 17));
    negative.push_back(entry4(2, 2, 3,   0, 24));
    negative.push_back(entry4(2, 2, 7,   0, 24));
    negative.push_back(entry4(2, 2, 1,   0, 24));
    negative.push_back(entry4(3, 0, 0,   0, 8));
        
    rc = ib_ipset4_init(
        &set,
        negative.data(), negative.size(),
        positive.data(), positive.size()
    );
    ASSERT_EQ(IB_OK, rc);
    
    const ib_ipset4_entry_t* entry    = NULL;
    const ib_ipset4_entry_t* specific = NULL;
    const ib_ipset4_entry_t* general  = NULL;

    rc = ib_ipset4_query(
        &set, ip4(1, 2, 100, 20), 
        &entry, &specific, &general
    );
    EXPECT_EQ(IB_OK, rc);
    EXPECT_EQ(entry, specific);
    EXPECT_EQ(entry, general);
    EXPECT_EQ(&marker_a, reinterpret_cast<const int*>(entry->data));
    rc = ib_ipset4_query(
        &set, ip4(2, 3, 1,   2),  
        &entry, &specific, &general
    );
    EXPECT_EQ(IB_OK, rc);
    EXPECT_LT(general, specific);
    EXPECT_TRUE(entry == specific || entry == general);
    EXPECT_EQ(&marker_b, reinterpret_cast<const int*>(general->data));
    EXPECT_EQ(&marker_c, reinterpret_cast<const int*>(specific->data));
    rc = ib_ipset4_query(
        &set, ip4(2, 5, 130, 1),  
        &entry, &specific, &general
    );
    EXPECT_FALSE(entry);
    EXPECT_FALSE(specific);
    EXPECT_FALSE(general);
    EXPECT_EQ(IB_ENOENT, rc);
}

TEST_F(TestIPSet, Structured4)
{
    static const size_t c_num_tests = 1e5;
    
    ib_status_t rc;
    ib_ipset4_t set;
    vector<ib_ipset4_entry_t> positive;
    vector<ib_ipset4_entry_t> negative;
    
    // Set includes all ips that begin with a sequence of ones followed by
    // at most one zero.  I.e., positive entries are all possible prefixes of
    // 1s and negative entries are all possible prefixes of 1s followed by two
    // zeros.
    for (size_t i = 1; i < 32; ++i) {
        ib_ipset4_entry_t entry;
        make_ones(entry.network.ip, i);
        entry.network.size = i;
        entry.data = NULL;
        positive.push_back(entry);
        
        if (entry.network.size <= 30) {
            entry.network.size += 2;
            negative.push_back(entry);
        }
    }
        
    rc = ib_ipset4_init(
        &set,
        negative.data(), negative.size(),
        positive.data(), positive.size()
    );
    ASSERT_EQ(IB_OK, rc);
   
    // Use asserts as if something is wrong, output could be huge with
    // expects.
    
    // Test a bunch of positives.
    for (size_t i = 0; i < c_num_tests; ++i) {
        ib_ipset4_ip_t ip;
        size_t num_ones = random(1, 32);
        make_ones(ip, num_ones);
        if (num_ones <= 30) {
            set_bit(ip, num_ones + 1);
            for (size_t j = num_ones + 2; j <= 32; ++j) {
                set_bit(ip, j, random(0,1));
            }
        }
        rc = ib_ipset4_query(&set, ip, NULL, NULL, NULL);
        ASSERT_EQ(IB_OK, rc);
    }
    
    // Test a bunch of negatives.
    for (size_t i = 0; i < c_num_tests; ++i) {
        ib_ipset4_ip_t ip;
        char num_ones = random(1, 30);
        make_ones(ip, num_ones);
        rc = ib_ipset4_query(&set, ip, NULL, NULL, NULL);
        ASSERT_EQ(IB_ENOENT, rc);
    }
}

TEST_F(TestIPSet, PositiveSet4)
{
    static const size_t c_num_tests = 1e5;
    static const size_t c_num_ips   = 1024;
    
    ib_status_t rc;
    ib_ipset4_t set;
    vector<ib_ipset4_entry_t> positive;
    std::set<ib_ipset4_ip_t> ips;
    
    // To limit the search space, the first 20 bits will be 0.
    while (ips.size() < c_num_ips) {
        ib_ipset4_entry_t entry;
        make_ones(entry.network.ip, 20);
        entry.network.ip |= random(0, 4095);
        entry.network.size = 32;
        
        bool is_new = ips.insert(entry.network.ip).second;
        if (is_new) {
            positive.push_back(entry);
        }
    }
        
    rc = ib_ipset4_init(
        &set,
        NULL, 0,
        positive.data(), positive.size()
    );
    ASSERT_EQ(IB_OK, rc);
    
    // Test a bunch of ips.
    for (size_t i = 0; i < c_num_tests; ++i) {
        ib_ipset4_ip_t ip;
        make_ones(ip, 20);
        ip |= random(0, 4095);
        bool in_set = (ips.count(ip) != 0);
        rc = ib_ipset4_query(&set, ip, NULL, NULL, NULL);
        if (in_set) {
            ASSERT_EQ(IB_OK, rc);
        }
        else {
            ASSERT_EQ(IB_ENOENT, rc);
        }
    }
}

TEST_F(TestIPSet, Simple6)
{
    ib_status_t rc;
    ib_ipset6_t set;
    vector<ib_ipset6_entry_t> positive;
    vector<ib_ipset6_entry_t> negative;
    
    positive.push_back(entry6(1, 0, 0, 0, 32));
    negative.push_back(entry6(1, 2, 3, 0, 96));
    
    rc = ib_ipset6_init(
        &set,
        negative.data(), negative.size(),
        positive.data(), positive.size()
    );
    ASSERT_EQ(IB_OK, rc);
    
    const ib_ipset6_entry_t* result;
    rc = ib_ipset6_query(&set, ip6(1, 2, 100, 20), &result, NULL, NULL);
    ASSERT_EQ(IB_OK, rc);
    EXPECT_EQ(ip6(1, 0, 0, 0), result->network.ip);
    EXPECT_EQ(32, result->network.size);
    rc = ib_ipset6_query(&set, ip6(1, 2, 3,   20), &result, NULL, NULL);
    EXPECT_EQ(IB_ENOENT, rc);
    EXPECT_FALSE(result);
    rc = ib_ipset6_query(&set, ip6(3, 2, 3,   20), &result, NULL, NULL);
    EXPECT_EQ(IB_ENOENT, rc);
    EXPECT_FALSE(result);
}

TEST_F(TestIPSet, Complex6)
{
    ib_status_t rc;
    ib_ipset6_t set;
    vector<ib_ipset6_entry_t> positive;
    vector<ib_ipset6_entry_t> negative;
    
    static int marker_a = 1;
    static int marker_b = 2;
    static int marker_c = 3;
    
    positive.push_back(entry6(2, 1, 0, 0, 64));
    positive.push_back(entry6(2, 5, 0, 0, 64));
    positive.push_back(entry6(2, 4, 0, 0, 64));
    positive.push_back(entry6(2, 6, 1, 0, 96));
    positive.push_back(entry6(1, 0, 0, 0, 32, &marker_a));
    positive.push_back(entry6(2, 0, 0, 0, 32));
    positive.push_back(entry6(2, 3, 0, 0, 64, &marker_b));
    positive.push_back(entry6(2, 3, 1, 0, 96, &marker_c));
    positive.push_back(entry6(2, 2, 0, 0, 64));
    
    negative.push_back(entry6(2, 5, 0x10000000, 0, 65));
    negative.push_back(entry6(2, 5, 0x10000000, 0, 33));
    negative.push_back(entry6(2, 2, 3,   0, 96));
    negative.push_back(entry6(2, 2, 7,   0, 96));
    negative.push_back(entry6(2, 2, 1,   0, 96));
    negative.push_back(entry6(3, 0, 0,   0, 32));
        
    rc = ib_ipset6_init(
        &set,
        negative.data(), negative.size(),
        positive.data(), positive.size()
    );
    ASSERT_EQ(IB_OK, rc);
    
    const ib_ipset6_entry_t* entry    = NULL;
    const ib_ipset6_entry_t* specific = NULL;
    const ib_ipset6_entry_t* general  = NULL;

    rc = ib_ipset6_query(
        &set, ip6(1, 2, 100, 20), 
        &entry, &specific, &general
    );
    EXPECT_EQ(IB_OK, rc);
    EXPECT_EQ(entry, specific);
    EXPECT_EQ(entry, general);
    EXPECT_EQ(&marker_a, reinterpret_cast<const int*>(entry->data));
    rc = ib_ipset6_query(
        &set, ip6(2, 3, 1,   2),  
        &entry, &specific, &general
    );
    EXPECT_EQ(IB_OK, rc);
    EXPECT_LT(general, specific);
    EXPECT_TRUE(entry == specific || entry == general);
    EXPECT_EQ(&marker_b, reinterpret_cast<const int*>(general->data));
    EXPECT_EQ(&marker_c, reinterpret_cast<const int*>(specific->data));
    rc = ib_ipset6_query(
        &set, ip6(2, 5, 0x11000000, 1),  
        &entry, &specific, &general
    );
    EXPECT_FALSE(entry);
    EXPECT_FALSE(specific);
    EXPECT_FALSE(general);
    EXPECT_EQ(IB_ENOENT, rc);
}

TEST_F(TestIPSet, Structured6)
{
    static const size_t c_num_tests = 1e5;
    
    ib_status_t rc;
    ib_ipset6_t set;
    vector<ib_ipset6_entry_t> positive;
    vector<ib_ipset6_entry_t> negative;
    
    // Set includes all ips that begin with a sequence of ones followed by
    // at most one zero.  I.e., positive entries are all possible prefixes of
    // 1s and negative entries are all possible prefixes of 1s followed by two
    // zeros.
    for (size_t i = 1; i < 128; ++i) {
        ib_ipset6_entry_t entry;
        make_ones(entry.network.ip, i);
        entry.network.size = i;
        entry.data = NULL;
        positive.push_back(entry);
        
        if (entry.network.size <= 126) {
            entry.network.size += 2;
            negative.push_back(entry);
        }
    }
        
    rc = ib_ipset6_init(
        &set,
        negative.data(), negative.size(),
        positive.data(), positive.size()
    );
    ASSERT_EQ(IB_OK, rc);
   
    // Use asserts as if something is wrong, output could be huge with
    // expects.
    
    // Test a bunch of positives.
    for (size_t i = 0; i < c_num_tests; ++i) {
        ib_ipset6_ip_t ip;
        size_t num_ones = random(1, 128);
        make_ones(ip, num_ones);
        if (num_ones <= 126) {
            set_bit(ip, num_ones + 1);
        }
        for (size_t j = num_ones + 2; j < 128; ++j) {
            set_bit(ip, j, random(0,1));
        }
        rc = ib_ipset6_query(&set, ip, NULL, NULL, NULL);
        ASSERT_EQ(IB_OK, rc);
    }
    
    // Test a bunch of negatives.
    for (size_t i = 0; i < c_num_tests; ++i) {
        ib_ipset6_ip_t ip;
        char num_ones = random(1, 126);
        make_ones(ip, num_ones);
        rc = ib_ipset6_query(&set, ip, NULL, NULL, NULL);
        ASSERT_EQ(IB_ENOENT, rc);
    }
}

TEST_F(TestIPSet, PositiveSet6)
{
    static const size_t c_num_tests = 1e5;
    static const size_t c_num_ips   = 1024;
    
    ib_status_t rc;
    ib_ipset6_t set;
    vector<ib_ipset6_entry_t> positive;
    std::set<ib_ipset6_ip_t> ips;
    
    // To limit the search space, the first 116 bits will be 1.
    while (ips.size() < c_num_ips) {
        ib_ipset6_entry_t entry;
        make_ones(entry.network.ip, 116);
        entry.network.ip.ip[3] |= random(0, 4095);
        entry.network.size = 128;
        
        bool is_new = ips.insert(entry.network.ip).second;
        if (is_new) {
            positive.push_back(entry);
        }
    }
        
    rc = ib_ipset6_init(
        &set,
        NULL, 0,
        positive.data(), positive.size()
    );
    ASSERT_EQ(IB_OK, rc);
    
    // Test a bunch of ips.
    for (size_t i = 0; i < c_num_tests; ++i) {
        ib_ipset6_ip_t ip;
        make_ones(ip, 116);
        ip.ip[3] |= random(0, 4095);
        bool in_set = (ips.count(ip) != 0);
        rc = ib_ipset6_query(&set, ip, NULL, NULL, NULL);
        if (in_set) {
            ASSERT_EQ(IB_OK, rc);
        }
        else {
            ASSERT_EQ(IB_ENOENT, rc);
        }
    }
}

TEST_F(TestIPSet, Inval)
{
    ib_ipset4_t set4;
    ib_ipset6_t set6;

    EXPECT_EQ(IB_EINVAL, ib_ipset4_init(NULL, NULL, 0, NULL, 0));
    EXPECT_EQ(IB_EINVAL, ib_ipset4_init(&set4, NULL, 1, NULL, 0));
    EXPECT_EQ(IB_EINVAL, ib_ipset4_init(&set4, NULL, 0, NULL, 1));
    EXPECT_EQ(IB_EINVAL, ib_ipset4_query(NULL, 0, NULL, NULL, NULL));

    EXPECT_EQ(IB_EINVAL, ib_ipset6_init(NULL, NULL, 0, NULL, 0));
    EXPECT_EQ(IB_EINVAL, ib_ipset6_init(&set6, NULL, 1, NULL, 0));
    EXPECT_EQ(IB_EINVAL, ib_ipset6_init(&set6, NULL, 0, NULL, 1));
    EXPECT_EQ(
        IB_EINVAL, 
        ib_ipset6_query(NULL, ib_ipset6_ip_t(), NULL, NULL, NULL)
    );
}
