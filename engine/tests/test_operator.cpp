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
/// @brief IronBee --- Operator Test Functions
///
/// @author Craig Forbes <cforbes@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "base_fixture.h"
#include <ironbee/operator.h>
#include <ironbee/server.h>
#include <ironbee/context.h>
#include <ironbee/engine.h>
#include <ironbee/mpool.h>
#include <ironbee/field.h>
#include "gtest/gtest.h"


ib_status_t test_create_fn(
    ib_context_t *ctx,
    const char   *parameters,
    void         *instance_data,
    void         *cbdata
)
{
    assert(ctx != NULL);
    char *str;
    ib_mpool_t *pool = ib_context_get_mpool(ctx);

    if (strcmp(parameters, "INVALID") == 0) {
        return IB_EINVAL;
    }
    str = ib_mpool_strdup(pool, parameters);
    if (str == NULL) {
        return IB_EALLOC;
    }

    *(char **)instance_data = str;

    return IB_OK;
}

ib_status_t test_execute_fn(
    ib_tx_t          *tx,
    void             *instance_data,
    const ib_field_t *field,
    ib_field_t       *capture,
    ib_num_t         *result,
    void             *cbdata
)
{
    char *searchstr = (char *)instance_data;
    const char* s;
    ib_status_t rc;

    if (field->type != IB_FTYPE_NULSTR) {
        return IB_EINVAL;
    }

    rc = ib_field_value(field, ib_ftype_nulstr_out(&s));
    if (rc != IB_OK) {
        return rc;
    }

    if (strstr(s, searchstr) == NULL) {
        *result = 0;
    }
    else {
        *result = 1;
    }

    return IB_OK;
}

class OperatorTest : public BaseTransactionFixture
{
    void SetUp()
    {
        BaseFixture::SetUp();
    }
};

TEST_F(OperatorTest, OperatorCallTest)
{
    ib_status_t status;
    ib_num_t call_result;
    void *instance_data;
    ib_operator_t *op;
    const ib_operator_t *cop;

    status = ib_operator_create_and_register(
        &op,
        ib_engine,
        "test_op",
        IB_OP_CAPABILITY_NONE,
        test_create_fn, NULL,
        NULL, NULL,
        test_execute_fn, NULL
    );
    ASSERT_EQ(IB_OK, status);

    status = ib_operator_lookup(ib_engine, "test_op", &cop);

    ASSERT_EQ(IB_OK, status);

    status = ib_operator_inst_create(op,
                                     ib_context_main(ib_engine),
                                     IB_OP_CAPABILITY_NONE,
                                     "INVALID",
                                     &instance_data);
    ASSERT_EQ(IB_EINVAL, status);

    status = ib_operator_inst_create(op,
                                     ib_context_main(ib_engine),
                                     IB_OP_CAPABILITY_NONE,
                                     "data",
                                     &instance_data);
    ASSERT_EQ(IB_OK, status);


    ib_field_t *field;
    const char *matching = "data matching string";
    const char *nonmatching = "non matching string";
    ib_field_create(
        &field,
        ib_engine_pool_main_get(ib_engine),
        IB_S2SL("testfield"),
        IB_FTYPE_NULSTR,
        NULL
    );

    ib_field_setv(field, ib_ftype_nulstr_in(matching));
    status = ib_operator_inst_execute(op, instance_data, ib_tx, field, NULL, &call_result);
    ASSERT_EQ(IB_OK, status);
    EXPECT_EQ(1, call_result);

    ib_field_setv(field, ib_ftype_nulstr_in(nonmatching));
    status = ib_operator_inst_execute(op, instance_data, ib_tx, field, NULL, &call_result);
    ASSERT_EQ(IB_OK, status);
    EXPECT_EQ(0, call_result);

    status = ib_operator_inst_destroy(op, instance_data);
    ASSERT_EQ(IB_OK, status);
}


class CoreOperatorsTest : public BaseTransactionFixture
{
    void SetUp()
    {
        BaseFixture::SetUp();
        configureIronBee();
        performTx();
    }
};

TEST_F(CoreOperatorsTest, ContainsTest)
{
    ib_status_t status;
    ib_num_t call_result;
    const ib_operator_t *op;
    void *instance_data;

    status = ib_operator_lookup(ib_engine, "contains", &op);

    ASSERT_EQ(IB_OK, status);


    status = ib_operator_inst_create(op,
                                     ib_context_main(ib_engine),
                                     IB_OP_CAPABILITY_NONE,
                                     "needle",
                                     &instance_data);
    ASSERT_EQ(IB_OK, status);

    // call contains
    ib_field_t *field;
    const char *matching = "data with needle in it";
    const char *nonmatching = "non matching string";
    ib_field_create(
        &field,
        ib_engine_pool_main_get(ib_engine),
        IB_S2SL("testfield"),
        IB_FTYPE_NULSTR,
        NULL
    );

    ib_field_setv(field, ib_ftype_nulstr_in(matching));
    status = ib_operator_inst_execute(op, instance_data, ib_tx, field, NULL, &call_result);
    ASSERT_EQ(IB_OK, status);
    EXPECT_EQ(1, call_result);

    ib_field_setv(field, ib_ftype_nulstr_in(nonmatching));
    status = ib_operator_inst_execute(op, instance_data, ib_tx, field, NULL, &call_result);
    ASSERT_EQ(IB_OK, status);
    EXPECT_EQ(0, call_result);
}

TEST_F(CoreOperatorsTest, EqTest)
{
    ib_status_t status;
    ib_num_t call_result;
    const ib_operator_t *op;
    void *instance_data;

    status = ib_operator_lookup(ib_engine, "eq", &op);

    ASSERT_EQ(IB_OK, status);

    status = ib_operator_inst_create(op,
                                     ib_context_main(ib_engine),
                                     IB_OP_CAPABILITY_NONE,
                                     "1",
                                     &instance_data);
    ASSERT_EQ(IB_OK, status);

    // call contains
    ib_field_t *field;
    const ib_num_t matching = 1;
    const ib_num_t nonmatching = 2;
    ib_field_create(
        &field,
        ib_engine_pool_main_get(ib_engine),
        IB_S2SL("testfield"),
        IB_FTYPE_NUM,
        ib_ftype_num_in(&matching)
    );

    ib_field_setv(field, ib_ftype_num_in(&matching));
    status = ib_operator_inst_execute(op, instance_data, ib_tx, field, NULL, &call_result);
    ASSERT_EQ(IB_OK, status);
    EXPECT_EQ(1, call_result);

    ib_field_setv(field, ib_ftype_num_in(&nonmatching));
    status = ib_operator_inst_execute(op, instance_data, ib_tx, field, NULL, &call_result);
    ASSERT_EQ(IB_OK, status);
    EXPECT_EQ(0, call_result);
}

TEST_F(CoreOperatorsTest, NeTest)
{
    ib_status_t status;
    ib_num_t call_result;
    const ib_operator_t *op;
    void *instance_data;

    status = ib_operator_lookup(ib_engine, "ne", &op);

    ASSERT_EQ(IB_OK, status);

    status = ib_operator_inst_create(op,
                                     ib_context_main(ib_engine),
                                     IB_OP_CAPABILITY_NONE,
                                     "1",
                                     &instance_data);
    ASSERT_EQ(IB_OK, status);

    // call contains
    ib_field_t *field;
    const ib_num_t matching = 2;
    const ib_num_t nonmatching = 1;
    ib_field_create(
        &field,
        ib_engine_pool_main_get(ib_engine),
        IB_S2SL("testfield"),
        IB_FTYPE_NUM,
        ib_ftype_num_in(&matching)
    );

    ib_field_setv(field, ib_ftype_num_in(&matching));
    status = ib_operator_inst_execute(op, instance_data, ib_tx, field, NULL, &call_result);
    ASSERT_EQ(IB_OK, status);
    EXPECT_EQ(1, call_result);

    ib_field_setv(field, ib_ftype_num_in(&nonmatching));
    status = ib_operator_inst_execute(op, instance_data, ib_tx, field, NULL, &call_result);
    ASSERT_EQ(IB_OK, status);
    EXPECT_EQ(0, call_result);
}

TEST_F(CoreOperatorsTest, IpMatchSegfault) {
    ib_num_t call_result = 17; /* 17 is an arbitrary value. */
    const ib_operator_t *op;
    void *instance_data;
    ib_field_t *field;
    ib_bytestr_t *bytestr;

    ASSERT_EQ(
        IB_OK,
        ib_bytestr_alias_nulstr(
            &bytestr,
            ib_engine_pool_main_get(ib_engine),
            "nleroy-laptop.msn01.qualys.com:8182")
    );

    ASSERT_EQ(
        IB_OK,
        ib_field_create(
            &field,
            ib_engine_pool_main_get(ib_engine),
            IB_S2SL("testfield"),
            IB_FTYPE_BYTESTR,
            ib_ftype_bytestr_in(bytestr))
    );

    ASSERT_EQ(IB_OK, ib_operator_lookup(ib_engine, "ipmatch", &op));

    ASSERT_EQ(
        IB_OK,
        ib_operator_inst_create(
            op,
            ib_context_main(ib_engine),
            IB_OP_CAPABILITY_NONE,
            "192.168.0.0/16",
            &instance_data)
    );

    /* Expected failure because the input value is incorrect. */
    ASSERT_EQ(
        IB_EINVAL,
        ib_operator_inst_execute(
            op,
            instance_data,
            ib_tx,
            field,
            NULL,
            &call_result)
    );

    /* And the result is left unchanged. */
    EXPECT_EQ(17, call_result);
}
