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

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

#include "util/util.c"
#include "util/mpool.c"
#include "util/bytestr.c"
#include "util/debug.c"


/* -- Tests -- */

/// @test Test util bytestr library - ib_bytestr_create() and ib_bytestr_destroy()
TEST(TestIBUtilByteStr, test_bytestr_create_and_destroy)
{
    ib_mpool_t *mp;
    ib_bytestr_t *bs;
    ib_status_t rc;
    
    atexit(ib_shutdown);
    rc = ib_initialize();
    ASSERT_TRUE(rc == IB_OK) << "ib_initialize() failed - rc != IB_OK";
    rc = ib_mpool_create(&mp, NULL);
    ASSERT_TRUE(rc == IB_OK) << "ib_mpool_create() failed - rc != IB_OK";
    
    rc = ib_bytestr_create(&bs, mp, 10);
    ASSERT_TRUE(rc == IB_OK) << "ib_bytestr_create() failed - rc != IB_OK";
    ASSERT_TRUE(bs != NULL) << "ib_bytestr_create() failed - NULL value";
    ASSERT_TRUE(ib_bytestr_length(bs) == 0) << "ib_bytestr_create() failed - wrong length ";
    ASSERT_TRUE(ib_bytestr_size(bs) == 10) << "ib_bytestr_create() failed - wrong size";

    ib_mpool_destroy(mp);
}

/// @test Test util bytestr library - ib_bytestr_dup_mem()
TEST(TestIBUtilByteStr, test_bytestr_dup_mem)
{
    ib_mpool_t *mp;
    ib_bytestr_t *bs;
    ib_status_t rc;
    uint8_t data[] = { 'a', 'b', 'c', 'd', 'e', 'f' };
    uint8_t *ptr;
    
    atexit(ib_shutdown);
    rc = ib_initialize();
    ASSERT_TRUE(rc == IB_OK) << "ib_initialize() failed - rc != IB_OK";
    rc = ib_mpool_create(&mp, NULL);
    ASSERT_TRUE(rc == IB_OK) << "ib_mpool_create() failed - rc != IB_OK";
    
    rc = ib_bytestr_dup_mem(&bs, mp, data, sizeof(data));
    ASSERT_TRUE(rc == IB_OK) << "ib_bytestr_dup_mem() failed - rc != IB_OK";
    ASSERT_TRUE(bs != NULL) << "ib_bytestr_dup_mem() failed - NULL value";
    ASSERT_TRUE(ib_bytestr_length(bs) == 6) << "ib_bytestr_dup_mem() failed - wrong length";
    ASSERT_TRUE(ib_bytestr_size(bs) == 6) << "ib_bytestr_dup_mem() failed - wrong size";
    ptr = ib_bytestr_ptr(bs);
    ASSERT_TRUE(ptr != data) << "ib_bytestr_dup_mem() failed - not a copy";
    ASSERT_TRUE(strncmp("abcdef", (char *)ptr, 6) == 0) << "ib_bytestr_dup_mem() failed - wrong data";

    ib_mpool_destroy(mp);
}

/// @test Test util bytestr library - ib_bytestr_dup_nulstr()
TEST(TestIBUtilByteStr, test_bytestr_dup_nulstr)
{
    ib_mpool_t *mp;
    ib_bytestr_t *bs;
    ib_status_t rc;
    char data[] = "abcdef";
    uint8_t *ptr;
    
    atexit(ib_shutdown);
    rc = ib_initialize();
    ASSERT_TRUE(rc == IB_OK) << "ib_initialize() failed - rc != IB_OK";
    rc = ib_mpool_create(&mp, NULL);
    ASSERT_TRUE(rc == IB_OK) << "ib_mpool_create() failed - rc != IB_OK";
    
    rc = ib_bytestr_dup_nulstr(&bs, mp, data);
    ASSERT_TRUE(rc == IB_OK) << "ib_bytestr_dup_nulstr() failed - rc != IB_OK";
    ASSERT_TRUE(bs != NULL) << "ib_bytestr_dup_nulstr() failed - NULL value";
    ASSERT_TRUE(ib_bytestr_length(bs) == 6) << "ib_bytestr_dup_nulstr() failed - wrong length";
    ASSERT_TRUE(ib_bytestr_size(bs) == 6) << "ib_bytestr_dup_nulstr() failed - wrong size";
    ptr = ib_bytestr_ptr(bs);
    ASSERT_TRUE(ptr != (uint8_t *)data) << "ib_bytestr_dup_nulstr() failed - not a copy";
    ASSERT_TRUE(strncmp("abcdef", (char *)ptr, 6) == 0) << "ib_bytestr_dup_nulstr() failed - wrong data: " << ptr;

    ib_mpool_destroy(mp);
}

/// @test Test util bytestr library - ib_bytestr_alias_mem()
TEST(TestIBUtilByteStr, test_bytestr_alias_mem)
{
    ib_mpool_t *mp;
    ib_bytestr_t *bs;
    ib_status_t rc;
    uint8_t data[] = { 'a', 'b', 'c', 'd', 'e', 'f' };
    uint8_t *ptr;
    
    atexit(ib_shutdown);
    rc = ib_initialize();
    ASSERT_TRUE(rc == IB_OK) << "ib_initialize() failed - rc != IB_OK";
    rc = ib_mpool_create(&mp, NULL);
    ASSERT_TRUE(rc == IB_OK) << "ib_mpool_create() failed - rc != IB_OK";
    
    rc = ib_bytestr_alias_mem(&bs, mp, data, sizeof(data));
    ASSERT_TRUE(rc == IB_OK) << "ib_bytestr_alias_mem() failed - rc != IB_OK";
    ASSERT_TRUE(bs != NULL) << "ib_bytestr_alias_mem() failed - NULL value";
    ASSERT_TRUE(ib_bytestr_length(bs) == 6) << "ib_bytestr_alias_mem() failed - wrong length";
    ASSERT_TRUE(ib_bytestr_size(bs) == 6) << "ib_bytestr_alias_mem() failed - wrong size";
    ptr = ib_bytestr_ptr(bs);
    ASSERT_TRUE(ptr == data) << "ib_bytestr_alias_mem() failed - copied";
    ASSERT_TRUE(strncmp("abcdef", (char *)ptr, 6) == 0) << "ib_bytestr_alias_mem() failed - wrong data";
    rc = ib_bytestr_append_nulstr(bs, "foo");
    ASSERT_TRUE(rc == IB_EINVAL) << "ib_bytestr_alias_mem() failed - read/write";

    ib_mpool_destroy(mp);
}

/// @test Test util bytestr library - ib_bytestr_alias_nulstr()
TEST(TestIBUtilByteStr, test_bytestr_alias_nulstr)
{
    ib_mpool_t *mp;
    ib_bytestr_t *bs;
    ib_status_t rc;
    char data[] = "abcdef";
    uint8_t *ptr;
    
    atexit(ib_shutdown);
    rc = ib_initialize();
    ASSERT_TRUE(rc == IB_OK) << "ib_initialize() failed - rc != IB_OK";
    rc = ib_mpool_create(&mp, NULL);
    ASSERT_TRUE(rc == IB_OK) << "ib_mpool_create() failed - rc != IB_OK";
    
    rc = ib_bytestr_alias_nulstr(&bs, mp, data);
    ASSERT_TRUE(rc == IB_OK) << "ib_bytestr_alias_nulstr() failed - rc != IB_OK";
    ASSERT_TRUE(bs != NULL) << "ib_bytestr_alias_nulstr() failed - NULL value";
    ASSERT_TRUE(ib_bytestr_length(bs) == 6) << "ib_bytestr_alias_nulstr() failed - wrong length";
    ASSERT_TRUE(ib_bytestr_size(bs) == 6) << "ib_bytestr_alias_nulstr() failed - wrong size";
    ptr = ib_bytestr_ptr(bs);
    ASSERT_TRUE(ptr == (uint8_t *)data) << "ib_bytestr_alias_nulstr() failed - copied";
    ASSERT_TRUE(strncmp("abcdef", (char *)ptr, 6) == 0) << "ib_bytestr_alias_nulstr() failed - wrong data";
    rc = ib_bytestr_append_nulstr(bs, "foo");
    ASSERT_TRUE(rc == IB_EINVAL) << "ib_bytestr_alias_nulstr() failed - read/write";

    ib_mpool_destroy(mp);
}

/// @test Test util bytestr library - ib_bytestr_append_*()
TEST(TestIBUtilByteStr, test_bytestr_append)
{
    ib_mpool_t *mp;
    ib_bytestr_t *bs1;
    ib_bytestr_t *bs2;
    ib_status_t rc;
    char data1[] = "abcdef";
    char data2[] = "ghijkl";
    char data3[] = "foo";
    uint8_t data4[] = { 'b', 'a', 'r' };
    uint8_t *ptr;
    
    atexit(ib_shutdown);
    rc = ib_initialize();
    ASSERT_TRUE(rc == IB_OK) << "ib_initialize() failed - rc != IB_OK";
    rc = ib_mpool_create(&mp, NULL);
    ASSERT_TRUE(rc == IB_OK) << "ib_mpool_create() failed - rc != IB_OK";
    
    rc = ib_bytestr_dup_nulstr(&bs1, mp, data1);
    ASSERT_TRUE(rc == IB_OK) << "ib_bytestr_append() failed - rc != IB_OK";
    ASSERT_TRUE(bs1 != NULL) << "ib_bytestr_append() failed - NULL value";
    ASSERT_TRUE(ib_bytestr_length(bs1) == 6) << "ib_bytestr_append() failed - wrong length";
    ASSERT_TRUE(ib_bytestr_size(bs1) == 6) << "ib_bytestr_append() failed - wrong size";
    ptr = ib_bytestr_ptr(bs1);
    ASSERT_TRUE(strncmp("abcdef", (char *)ptr, 6) == 0) << "ib_bytestr_append() failed - wrong data";

    rc = ib_bytestr_dup_nulstr(&bs2, mp, data2);
    ASSERT_TRUE(rc == IB_OK) << "ib_bytestr_append() failed - rc != IB_OK";
    ASSERT_TRUE(bs2 != NULL) << "ib_bytestr_append() failed - NULL value";
    ASSERT_TRUE(ib_bytestr_length(bs2) == 6) << "ib_bytestr_append() failed - wrong length";
    ASSERT_TRUE(ib_bytestr_size(bs2) == 6) << "ib_bytestr_append() failed - wrong size";
    ptr = ib_bytestr_ptr(bs2);
    ASSERT_TRUE(strncmp("ghijkl", (char *)ptr, 6) == 0) << "ib_bytestr_append() failed - wrong data";

    rc = ib_bytestr_append(bs1, bs2);
    ASSERT_TRUE(rc == IB_OK) << "ib_bytestr_append() failed - rc != IB_OK";
    ASSERT_TRUE(bs1 != NULL) << "ib_bytestr_append() failed - NULL value";
    ASSERT_TRUE(ib_bytestr_length(bs1) == 12) << "ib_bytestr_append() failed - wrong length";
    ASSERT_TRUE(ib_bytestr_size(bs1) == 12) << "ib_bytestr_append() failed - wrong size";
    ptr = ib_bytestr_ptr(bs1);
    ASSERT_TRUE(strncmp("abcdefghijkl", (char *)ptr, 12) == 0) << "ib_bytestr_append() failed - wrong data";

    rc = ib_bytestr_append_nulstr(bs1, data3);
    ASSERT_TRUE(rc == IB_OK) << "ib_bytestr_append() failed - rc != IB_OK";
    ASSERT_TRUE(bs1 != NULL) << "ib_bytestr_append() failed - NULL value";
    ASSERT_TRUE(ib_bytestr_length(bs1) == 15) << "ib_bytestr_append() failed - wrong length";
    ASSERT_TRUE(ib_bytestr_size(bs1) == 15) << "ib_bytestr_append() failed - wrong size";
    ptr = ib_bytestr_ptr(bs1);
    ASSERT_TRUE(strncmp("abcdefghijklfoo", (char *)ptr, 15) == 0) << "ib_bytestr_append() failed - wrong data";

    rc = ib_bytestr_append_mem(bs1, data4, sizeof(data4));
    ASSERT_TRUE(rc == IB_OK) << "ib_bytestr_append() failed - rc != IB_OK";
    ASSERT_TRUE(bs1 != NULL) << "ib_bytestr_append() failed - NULL value";
    ASSERT_TRUE(ib_bytestr_length(bs1) == 18) << "ib_bytestr_append() failed - wrong length";
    ASSERT_TRUE(ib_bytestr_size(bs1) == 18) << "ib_bytestr_append() failed - wrong size";
    ptr = ib_bytestr_ptr(bs1);
    ASSERT_TRUE(strncmp("abcdefghijklfoobar", (char *)ptr, 18) == 0) << "ib_bytestr_append() failed - wrong data";

    ib_mpool_destroy(mp);
}
