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
/// @brief IronBee --- Action Test Functions
///
/// @author Craig Forbes <cforbes@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include <ironbee/action.h>
#include <ironbee/server.h>
#include <ironbee/engine.h>
#include <ironbee/mpool.h>
#include <ironbee/string.h>

#include "gtest/gtest.h"

#include "base_fixture.h"

class CoreActionTest : public BaseTransactionFixture
{
protected:
    void SetUp()
    {
        BaseTransactionFixture::SetUp();
        configureIronBee();
        performTx( );
    }
};

TEST_F(CoreActionTest, setVarAdd) {
    ib_field_t *f;
    ib_num_t n;

    f = getVar("a");
    ASSERT_TRUE(f);
    ASSERT_EQ(IB_FTYPE_NUM, f->type);

    ib_field_value(f, ib_ftype_num_out(&n));

    ASSERT_EQ(3, n);
}

TEST_F(CoreActionTest, setVarSub) {
    ib_field_t *f;
    ib_num_t n;

    f = getVar("b");
    ASSERT_TRUE(f);
    ASSERT_EQ(IB_FTYPE_NUM, f->type);

    ib_field_value(f, ib_ftype_num_out(&n));

    ASSERT_EQ(-1, n);
}

TEST_F(CoreActionTest, setVarMult) {
    ib_field_t *f;
    ib_num_t n;

    f = getVar("c");
    ASSERT_TRUE(f);
    ASSERT_EQ(IB_FTYPE_NUM, f->type);

    ib_field_value(f, ib_ftype_num_out(&n));

    ASSERT_EQ(2, n);
}

/**
 * Do a larger integration test.
 */
TEST_F(CoreActionTest, integration) {
    ib_field_t *f;
    ib_num_t n;

    f = getVar("r1");
    ASSERT_TRUE(f);
    ASSERT_EQ(IB_FTYPE_NUM, f->type);
    ib_field_value(f, ib_ftype_num_out(&n));
    ASSERT_EQ(1, n);

    f = getVar("r2");
    ASSERT_TRUE(f);
    ASSERT_EQ(IB_FTYPE_NUM, f->type);
    ib_field_value(f, ib_ftype_num_out(&n));
    ASSERT_EQ(1, n);

    f = getVar("r3");
    ASSERT_TRUE(f);
    ASSERT_EQ(IB_FTYPE_NUM, f->type);
    ib_field_value(f, ib_ftype_num_out(&n));
    ASSERT_EQ(1, n);

    f = getVar("r4");
    ASSERT_TRUE(f);
    ASSERT_EQ(IB_FTYPE_NUM, f->type);
    ib_field_value(f, ib_ftype_num_out(&n));
    ASSERT_EQ(1, n);
}
