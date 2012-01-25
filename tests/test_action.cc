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
/// @brief IronBee - Action Test Functions
///
/// @author Craig Forbes <cforbes@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include <ironbee/action.h>
#include <ironbee/plugin.h>
#include <ironbee/engine.h>
#include <ironbee/mpool.h>

#include "ironbee_private.h"

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

#include "base_fixture.h"

class ActionTest : public BaseFixture {
};

TEST_F(ActionTest, RegisterTest) {
    ib_status_t status;
    status = ib_action_register(ib_engine,
                                "test_action",
                                IB_ACT_FLAG_NONE,
                                NULL,
                                NULL,
                                NULL);
    EXPECT_EQ(IB_OK, status);
}

TEST_F(ActionTest, RegisterDup) {
    ib_status_t status;
    status = ib_action_register(ib_engine,
                                "test_action",
                                IB_ACT_FLAG_NONE,
                                NULL,
                                NULL,
                                NULL);
    ASSERT_EQ(IB_OK, status);
    status = ib_action_register(ib_engine,
                                "test_action",
                                IB_ACT_FLAG_NONE,
                                NULL,
                                NULL,
                                NULL);
    EXPECT_EQ(IB_EINVAL, status);
}

TEST_F(ActionTest, CallAction) {
    ib_status_t status;
    ib_action_inst_t *act;
    status = ib_action_register(ib_engine,
                                "test_action",
                                IB_ACT_FLAG_NONE,
                                NULL,
                                NULL,
                                NULL);
    ASSERT_EQ(IB_OK, status);

    status = ib_action_inst_create(ib_engine,
                                   "test_action", "parameters",
                                   IB_ACTINST_FLAG_NONE,
                                   &act);
    ASSERT_EQ(IB_OK, status);

    status = ib_action_execute(act, NULL, NULL);
    ASSERT_EQ(IB_OK, status);
}
