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
/// @brief IronBee &mdash; UUID Test Functions
/// 
/// @author Brian Rectanus <brectanus@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "ironbee_config_auto.h"

#include <ironbee/util.h>

#include "ironbee_private.h"

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

#include <string.h>

namespace OSSPUUID {
#include <uuid.h>
}

struct testval {
    const char    *str;
    ib_status_t    ret;
    ib_uuid_t      val;
};

static struct testval uuidstr[] = {
    { "01234567-89ab-cdef-0123-456789abcdef",
      IB_OK,
      { { 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
          0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef } } },
    { "01234567-89ab-cdef-0123-456789abcdef ",
      IB_EINVAL, { { 0 } } },
    { " 01234567-89ab-cdef-0123-456789abcdef",
      IB_EINVAL, { { 0 } } },
    { " 01234567-89ab-cdef-0123-456789abcdef ",
      IB_EINVAL, { { 0 } } },
    { "0123456789abcdef0123456789abcdef",
      IB_EINVAL, { { 0 } } },
    { "1234",
      IB_EINVAL, { { 0 } } },
    { "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx",
      IB_EINVAL, { { 0 } } },
    { "0123456789abcdef0123456789abcdex",
      IB_EINVAL, { { 0 } } },
    { "0123456789abcdef0123456789abcdxf",
      IB_EINVAL, { { 0 } } },
    { "0123456789abcdef0123456789abcdefxxx",
      IB_EINVAL, { { 0 } } },
    { "xxx0123456789abcdef0123456789abcdef",
      IB_EINVAL, { { 0 } } },
    { "xxx0123456789abcdef0123456789abcdefxxx",
      IB_EINVAL, { { 0 } } },
    { NULL, IB_OK, { { 0 } } }
};

/* -- Tests -- */

/// @test Test util uuid library - ib_uuid_ascii_to_bin()
TEST(TestIBUtilUUID, predefined)
{
    struct testval *rec;
    ib_status_t rc;

    ib_uuid_initialize();
    
    for (rec = &uuidstr[0];rec->str != NULL; ++rec) {
        ib_uuid_t uuid;
        bzero(&uuid, UUID_LEN_BIN);
        rc = ib_uuid_ascii_to_bin(&uuid, rec->str);
        ASSERT_EQ(rec->ret, rc);
                
        if (rc == IB_OK) {
            ASSERT_EQ(0, memcmp(&rec->val, &uuid, UUID_LEN_BIN));
        }
    }
    
    ib_uuid_shutdown();
}

TEST(TestIBUtilUUID, random)
{
    ib_uuid_t uuid;
    ib_uuid_t uuid2;
    ib_status_t rc;
    char *str = (char *)malloc(UUID_LEN_STR+1);
    int i;
 
    ib_uuid_initialize();   
     
    for (i=0; i<100; ++i) {
        bzero(&uuid, UUID_LEN_BIN);
    
        rc = ib_uuid_create_v4(&uuid);
        EXPECT_EQ(IB_OK, rc);
        EXPECT_NE(0UL, uuid.uint64[0]+uuid.uint64[1]);
        
        rc = ib_uuid_bin_to_ascii(str, &uuid);
        EXPECT_EQ(IB_OK, rc);
        
        rc = ib_uuid_ascii_to_bin(&uuid2, str);
        EXPECT_EQ(IB_OK, rc);
        
        EXPECT_EQ(0, memcmp(&uuid, &uuid2, UUID_LEN_BIN));
    }
    
    free(str);
    ib_uuid_shutdown();
}
