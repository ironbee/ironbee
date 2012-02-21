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
/// @brief IronBee - Byte String Test Functions
/// 
/// @author Brian Rectanus <brectanus@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "ironbee_config_auto.h"

#include <ironbee/mpool.h>
#include <ironbee/util.h>
#include <ironbee/bytestr.h>

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

#include <stdexcept>

class TestIBUtilByteStr : public ::testing::Test
{
public:
    TestIBUtilByteStr()
    {
        ib_status_t rc;
        
        ib_initialize();
        rc = ib_mpool_create(&m_pool, NULL, NULL);
        if (rc != IB_OK) {
            throw std::runtime_error("Could not create mpool.");
        }
    }
    
    ~TestIBUtilByteStr()
    {
        ib_shutdown();        
    }
    
protected:
    ib_mpool_t* m_pool;
};

/* -- Tests -- */

/// @test Test util bytestr library - ib_bytestr_create() and ib_bytestr_destroy()
TEST_F(TestIBUtilByteStr, test_bytestr_create_and_destroy)
{
    ib_bytestr_t *bs;
    ib_status_t rc;
    
    rc = ib_bytestr_create(&bs, m_pool, 10);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(bs);
    ASSERT_EQ(0UL, ib_bytestr_length(bs));
    ASSERT_EQ(10UL, ib_bytestr_size(bs));
}

/// @test Test util bytestr library - ib_bytestr_dup_mem()
TEST_F(TestIBUtilByteStr, test_bytestr_dup_mem)
{
    ib_bytestr_t *bs;
    ib_status_t rc;
    uint8_t data[] = { 'a', 'b', 'c', 'd', 'e', 'f' };
    uint8_t *ptr;
        
    rc = ib_bytestr_dup_mem(&bs, m_pool, data, sizeof(data));
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(bs);
    ASSERT_EQ(6UL, ib_bytestr_length(bs));
    ASSERT_EQ(6UL, ib_bytestr_size(bs));
    ptr = ib_bytestr_ptr(bs);
    ASSERT_NE(data, ptr);
    ASSERT_EQ(0, strncmp("abcdef", (char *)ptr, 6));
}

/// @test Test util bytestr library - ib_bytestr_dup_nulstr()
TEST_F(TestIBUtilByteStr, test_bytestr_dup_nulstr)
{
    ib_bytestr_t *bs;
    ib_status_t rc;
    char data[] = "abcdef";
    uint8_t *ptr;
        
    rc = ib_bytestr_dup_nulstr(&bs, m_pool, data);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(bs);
    ASSERT_EQ(6UL, ib_bytestr_length(bs));
    ASSERT_EQ(6UL, ib_bytestr_size(bs));
    ptr = ib_bytestr_ptr(bs);
    ASSERT_NE(data, ptr);
    ASSERT_EQ(0, strncmp("abcdef", (char *)ptr, 6));
}

/// @test Test util bytestr library - ib_bytestr_alias_mem()
TEST_F(TestIBUtilByteStr, test_bytestr_alias_mem)
{
    ib_bytestr_t *bs;
    ib_status_t rc;
    uint8_t data[] = { 'a', 'b', 'c', 'd', 'e', 'f' };
    uint8_t *ptr;
    
    rc = ib_bytestr_alias_mem(&bs, m_pool, data, sizeof(data));
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(bs);
    ASSERT_EQ(6UL, ib_bytestr_length(bs));
    ASSERT_EQ(6UL, ib_bytestr_size(bs));
    ptr = ib_bytestr_ptr(bs);
    ASSERT_EQ(data, ptr);
    ASSERT_EQ(0, strncmp("abcdef", (char *)ptr, 6));
    rc = ib_bytestr_append_nulstr(bs, "foo");
    ASSERT_EQ(IB_EINVAL, rc);
}

/// @test Test util bytestr library - ib_bytestr_alias_nulstr()
TEST_F(TestIBUtilByteStr, test_bytestr_alias_nulstr)
{
    ib_mpool_t *m_pool;
    ib_bytestr_t *bs;
    ib_status_t rc;
    char data[] = "abcdef";
    uint8_t *ptr;
        
    rc = ib_bytestr_alias_nulstr(&bs, m_pool, data);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(bs);
    ASSERT_EQ(6UL, ib_bytestr_length(bs));
    ASSERT_EQ(6UL, ib_bytestr_size(bs));
    ptr = ib_bytestr_ptr(bs);
    ASSERT_EQ((uint8_t *, ptr);
    ASSERT_EQ(0, strncmp("abcdef", (char *)ptr, 6));
    rc = ib_bytestr_append_nulstr(bs, "foo");
    ASSERT_EQ(IB_EINVAL, rc);
}

/// @test Test util bytestr library - ib_bytestr_append_*()
TEST_F(TestIBUtilByteStr, test_bytestr_append)
{
    ib_bytestr_t *bs1;
    ib_bytestr_t *bs2;
    ib_status_t rc;
    char data1[] = "abcdef";
    char data2[] = "ghijkl";
    char data3[] = "foo";
    uint8_t data4[] = { 'b', 'a', 'r' };
    uint8_t *ptr;
        
    rc = ib_bytestr_dup_nulstr(&bs1, m_pool, data1);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(bs1);
    ASSERT_EQ(6UL, ib_bytestr_length(bs1));
    ASSERT_EQ(6UL, ib_bytestr_size(bs1));
    ptr = ib_bytestr_ptr(bs1);
    ASSERT_EQ(0, strncmp("abcdef", (char *)ptr, 6));

    rc = ib_bytestr_dup_nulstr(&bs2, m_pool, data2);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(bs2);
    ASSERT_EQ(6UL, ib_bytestr_length(bs2));
    ASSERT_EQ(6UL, ib_bytestr_size(bs2));
    ptr = ib_bytestr_ptr(bs2);
    ASSERT_EQ(0, strncmp("ghijkl", (char *)ptr, 6));

    rc = ib_bytestr_append(bs1, bs2);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(bs1);
    ASSERT_EQ(12UL, ib_bytestr_length(bs1));
    ASSERT_EQ(12UL, ib_bytestr_size(bs1));
    ptr = ib_bytestr_ptr(bs1);
    ASSERT_EQ(0, strncmp("abcdefghijkl", (char *)ptr, 12));

    rc = ib_bytestr_append_nulstr(bs1, data3);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(bs1);
    ASSERT_EQ(15UL, ib_bytestr_length(bs1));
    ASSERT_EQ(15UL, ib_bytestr_size(bs1));
    ptr = ib_bytestr_ptr(bs1);
    ASSERT_EQ(0, strncmp("abcdefghijklfoo", (char *)ptr, 15));

    rc = ib_bytestr_append_mem(bs1, data4, sizeof(data4));
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(bs1);
    ASSERT_EQ(18UL, ib_bytestr_length(bs1));
    ASSERT_EQ(18UL, ib_bytestr_size(bs1));
    ptr = ib_bytestr_ptr(bs1);
    ASSERT_EQ(0, strncmp("abcdefghijklfoobar", (char *)ptr, 18));
}
