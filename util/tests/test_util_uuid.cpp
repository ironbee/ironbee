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
/// @brief IronBee --- UUID Test Functions
///
/// @author Brian Rectanus <brectanus@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "ironbee_config_auto.h"

#include <ironbee/uuid.h>

#include "gtest/gtest.h"

#include <string.h>

TEST(TestIBUtilUUID, random)
{
    char uuid[IB_UUID_LENGTH];
    char uuid2[IB_UUID_LENGTH];
    ib_status_t rc;

    ib_uuid_initialize();

    rc = ib_uuid_create_v4(uuid);
    EXPECT_EQ(IB_OK, rc);
    EXPECT_NE('\0', uuid[0]);
    rc = ib_uuid_create_v4(uuid2);
    EXPECT_EQ(IB_OK, rc);
    EXPECT_NE('\0', uuid2[0]);

    EXPECT_NE(0, memcmp(uuid, uuid2, IB_UUID_LENGTH));

    ib_uuid_shutdown();
}
