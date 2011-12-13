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
/// @brief IronBee - UUID Test Functions
/// 
/// @author Brian Rectanus <brectanus@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "ironbee_config_auto.h"

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

#define TESTING

#include "util/uuid.c"

struct testval {
    const char    *str;
    ib_status_t    ret;
    ib_uuid_t      val;
};

static struct testval uuidstr[] = {
    { "01234567-89ab-cdef-0123-456789abcdef",
      IB_OK,
      { 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
        0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef } },
    { "01234567-89ab-cdef-0123-456789abcdef ",
      IB_EINVAL, { 0 } },
    { " 01234567-89ab-cdef-0123-456789abcdef",
      IB_EINVAL, { 0 } },
    { " 01234567-89ab-cdef-0123-456789abcdef ",
      IB_EINVAL, { 0 } },
    { "0123456789abcdef0123456789abcdef",
      IB_EINVAL, { 0 } },
    { "1234",
      IB_EINVAL, { 0 } },
    { "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx",
      IB_EINVAL, { 0 } },
    { "0123456789abcdef0123456789abcdex",
      IB_EINVAL, { 0 } },
    { "0123456789abcdef0123456789abcdxf",
      IB_EINVAL, { 0 } },
    { "0123456789abcdef0123456789abcdefxxx",
      IB_EINVAL, { 0 } },
    { "xxx0123456789abcdef0123456789abcdef",
      IB_EINVAL, { 0 } },
    { "xxx0123456789abcdef0123456789abcdefxxx",
      IB_EINVAL, { 0 } },
    { NULL, IB_OK, { 0 } }
};


/* -- Tests -- */

/// @test Test util uuid library - ib_uuid_ascii_to_bin()
TEST(TestIBUtilUUID, test_field_create)
{
    struct testval *rec;
    ib_uuid_t uuid;
    ib_status_t rc;

    for (rec = &uuidstr[0];rec->str != NULL; ++rec) {
        ib_uuid_t uuid = { 0 };
        rc = ib_uuid_ascii_to_bin(&uuid, rec->str);
        ASSERT_TRUE(rc == rec->ret) << "ib_uuid_ascii_to_bin() failed - bad return code: " << rec->str;
        if (rc == IB_OK) {
            ASSERT_TRUE(memcmp(&rec->val, &uuid, 16) == 0) <<
                "ib_uuid_ascii_to_bin() failed: " <<
                rec->str << " - wrong binary value";
        }
    }
}
