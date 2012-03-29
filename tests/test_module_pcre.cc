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
/// @brief IronBee - poc_sig module tests
/// 
/// @author Brian Rectanus <brectanus@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

#include "base_fixture.h"
#include <ironbee/operator.h>
#include <ironbee/hash.h>
#include <ironbee/mpool.h>
#include <ironbee/field.h>
#include <ironbee/bytestr.h>

class PcreModuleTest : public BaseModuleFixture {
public:

    ib_conn_t *ib_conn;
    ib_tx_t *ib_tx;

    PcreModuleTest() : BaseModuleFixture("ibmod_pcre.so") 
    {
    }

    virtual void SetUp() {
        BaseModuleFixture::SetUp();

        configureIronBee();

        ib_conn = buildIronBeeConnection();

        // Create the transaction.
        sendDataIn(ib_conn,
                   "GET / HTTP/1.1\r\n"
                   "Host: UnitTest\r\n"
                   "X-MyHeader: header1\r\n"
                   "X-MyHeader: header2\r\n"
                   "\r\n");

        sendDataOut(ib_conn,
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/html\r\n"
                    "X-MyHeader: header3\r\n"
                    "X-MyHeader: header4\r\n"
                    "\r\n");

        assert(ib_conn->tx!=NULL);
        ib_tx = ib_conn->tx;

    }
};

TEST_F(PcreModuleTest, test_load_module)
{
    ib_operator_t op;
    ib_operator_inst_t *op_inst=NULL;
    ib_num_t result;

    ib_field_t* field1;
    ib_field_t* field2;

    char* str1 = (char *) ib_mpool_alloc(ib_engine->mp, (strlen("string 1")+1));
    char* str2 = (char *) ib_mpool_alloc(ib_engine->mp, (strlen("string 2")+1));
    strcpy(str1, "string 1");
    strcpy(str2, "string 2");
    
    // Create field 1.
    ASSERT_EQ(IB_OK,
        ib_field_create(
            &field1,
            ib_engine->mp,
            IB_FIELD_NAME("field1"),
            IB_FTYPE_NULSTR,
            ib_ftype_nulstr_in(str1)
        )
    );

    // Create field 2.
    ASSERT_EQ(IB_OK,
        ib_field_create(
            &field2,
            ib_engine->mp,
            IB_FIELD_NAME("field2"),
            IB_FTYPE_NULSTR,
            ib_ftype_nulstr_in(str2)
        )
    );

    // Ensure that the operator exists.
    ASSERT_EQ(IB_OK, ib_hash_get(ib_engine->operators, (void**)&op, "pcre"));

    // Get the operator.
    ASSERT_EQ(IB_OK,
              ib_operator_inst_create(ib_engine,
                                      NULL,
                                      "pcre",
                                      "string\\s2",
                                      0,
                                      &op_inst));

    // Attempt to match.
    ASSERT_EQ(IB_OK, op_inst->op->fn_execute(ib_engine,
                                             ib_conn->tx,
                                             op_inst->data,
                                             op_inst->flags,
                                             field1,
                                             &result));

    // We should fail.
    ASSERT_FALSE(result);

    // Attempt to match again.
    ASSERT_EQ(IB_OK, op_inst->op->fn_execute(ib_engine,
                                             ib_conn->tx,
                                             op_inst->data,
                                             op_inst->flags,
                                             field2,
                                             &result));

    // This time we should succeed.
    ASSERT_TRUE(result);
}

TEST_F(PcreModuleTest, matches)
{
    ib_field_t *ib_field;
    const ib_bytestr_t *ib_bytestr;
    char* s;
    size_t s_sz;
    ib_status_t rc;

    ib_data_get(ib_tx->dpi, "TX.0", &ib_field);
    ASSERT_NE(static_cast<ib_field_t*>(NULL), ib_field);
    ASSERT_EQ(static_cast<ib_ftype_t>(IB_FTYPE_BYTESTR), ib_field->type);

    ib_data_get(ib_tx->dpi, "TX.1", &ib_field);
    ASSERT_NE(static_cast<ib_field_t*>(NULL), ib_field);
    ASSERT_EQ(static_cast<ib_ftype_t>(IB_FTYPE_BYTESTR), ib_field->type);

    ib_data_get(ib_tx->dpi, "TX.2", &ib_field);
    ASSERT_NE(static_cast<ib_field_t*>(NULL), ib_field);
    ASSERT_EQ(static_cast<ib_ftype_t>(IB_FTYPE_BYTESTR), ib_field->type);

    rc = ib_field_value(ib_field, ib_ftype_bytestr_out(&ib_bytestr));
    ASSERT_EQ(IB_OK, rc);

    /* Check that a value is over written correctly. */
    s_sz = ib_bytestr_length(ib_bytestr);
    s = (char *) malloc(s_sz+1);
    memcpy(s, ib_bytestr_const_ptr(ib_bytestr), s_sz);
    s[s_sz] = '\0';
    ASSERT_STREQ("header4", s);

    ib_data_get(ib_tx->dpi, "TX.3", &ib_field);
    ASSERT_EQ(static_cast<ib_field_t*>(NULL), ib_field);
}

