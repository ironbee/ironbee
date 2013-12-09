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
#include <ironbee/var.h>

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

/**
 * Test that the TX flags are set in the tx var.
 */
class CoreActionFlagVarTest
    :
    public BaseTransactionFixture,
    public ::testing::WithParamInterface<const char*>
{
    void SetUp() {
        BaseTransactionFixture::SetUp();
        configureIronBee("CoreActionTest.setFlag.config");
        performTx();
    }
};

/**
 * Test that the TX flags are set in the tx.
 */
class CoreActionFlagTxTest
    :
    public BaseTransactionFixture,
    public ::testing::WithParamInterface<ib_flags_t>
{
    void SetUp() {
        BaseTransactionFixture::SetUp();
        configureIronBee("CoreActionTest.setFlag.config");
        performTx();
    }
};

TEST_P(CoreActionFlagVarTest, FlagSet) {
    ib_field_t       *f;
    ib_num_t          n;
    const ib_list_t  *l;
    ib_var_target_t  *target;

    ASSERT_EQ(
        IB_OK,
        ib_var_target_acquire_from_string(
            &target,
            ib_tx->mp,
            ib_var_store_config(ib_tx->var_store),
            GetParam(),
            strlen(GetParam()),
            NULL,
            NULL)
    );

    ASSERT_EQ(
        IB_OK,
        ib_var_target_get_const(
            target,
            &l,
            ib_tx->mp,
            ib_tx->var_store)
    );

    ASSERT_EQ(1U, ib_list_elements(l));

    f = (ib_field_t *)ib_list_node_data_const(ib_list_first_const(l));

    ASSERT_EQ(IB_FTYPE_NUM, f->type);

    ib_field_value(f, ib_ftype_num_out(&n));

    ASSERT_EQ(1, n);
}

TEST_P(CoreActionFlagTxTest, FlagSet) {
    ASSERT_TRUE(ib_flags_all(ib_tx->flags, GetParam()));
}

INSTANTIATE_TEST_CASE_P(AllFlags, CoreActionFlagVarTest, testing::Values(
    "FLAGS:suspicious",
    "FLAGS:inspectRequestHeader",
    "FLAGS:inspectRequestBody",
    "FLAGS:inspectResponseHeader",
    "FLAGS:inspectResponseBody",
    "FLAGS:inspectRequestParams",
    "FLAGS:inspectRequestUri",
    "FLAGS:blockingMode"));

INSTANTIATE_TEST_CASE_P(AllTxFlags, CoreActionFlagTxTest, testing::Values(
    IB_TX_FSUSPICIOUS,
    IB_TX_FINSPECT_REQHDR,
    IB_TX_FINSPECT_REQBODY,
    IB_TX_FINSPECT_RESHDR,
    IB_TX_FINSPECT_RESBODY,
    IB_TX_FINSPECT_REQPARAMS,
    IB_TX_FINSPECT_REQURI,
    IB_TX_FBLOCKING_MODE));

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

TEST_F(CoreActionTest, DoubleSetFlag) {
    ASSERT_EQ(IB_OK, ib_tx_flags_set(ib_tx, IB_TX_FINSPECT_RESBODY));
    ASSERT_EQ(IB_OK, ib_tx_flags_set(ib_tx, IB_TX_FINSPECT_RESBODY));
    ASSERT_TRUE(ib_tx->flags & IB_TX_FINSPECT_RESBODY);
}

TEST_F(CoreActionTest, DoubleUnSetFlag) {
    ASSERT_EQ(IB_OK, ib_tx_flags_unset(ib_tx, IB_TX_FINSPECT_RESBODY));
    ASSERT_EQ(IB_OK, ib_tx_flags_unset(ib_tx, IB_TX_FINSPECT_RESBODY));
    ASSERT_FALSE(ib_tx->flags & IB_TX_FINSPECT_RESBODY);
}
