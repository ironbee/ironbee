/*****************************************************************************
 * Licensed to Qualys, Inc. (QUALYS) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * QUALYS licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ****************************************************************************/

/**
 * @file
 * @brief Tests of IronBee capture interface
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

/* Testing fixture. */
#include "base_fixture.h"

/* Header of what we are testing. */
#include <ironbee/capture.h>
#include <ironbee/field.h>
#include <ironbee/list.h>
#include <ironbee/mpool.h>
#include <ironbee/string.h>

/**
 * Test capture
 */
class CaptureTest : public BaseTransactionFixture
{
    void SetUp()
    {
        BaseFixture::SetUp();
        configureIronBee();
        performTx();
    }
public:
    ib_field_t *CaptureGet(int num)
    {
        const char *capture_name = ib_capture_fullname(ib_tx, NULL, num);
        return getTarget1(capture_name);
    };

    ib_field_t *CaptureGet(const char *capture, int num)
    {
        const char *capture_name = ib_capture_fullname(ib_tx, capture, num);
        return getTarget1(capture_name);
    };

    ib_status_t CaptureBytestr(
        const char  *capture,
        int          num,
        const char  *value,
        ib_field_t **pfield)
    {
        const char   *name;
        ib_bytestr_t *bstr;
        ib_status_t   rc;
        ib_field_t *capture_field;

        name = ib_capture_name(num);
        name = ib_mpool_strdup(MainPool(), name);
        if (name == NULL) {
            throw std::runtime_error("Failed to dup name");
        }
        rc = ib_bytestr_dup_nulstr(&bstr, MainPool(), value);
        if (rc != IB_OK) {
            throw std::runtime_error("Failed to dup NulStr into ByteStr");
        }
        rc = ib_field_create(pfield, MainPool(),
                             IB_S2SL(name),
                             IB_FTYPE_BYTESTR,
                             ib_ftype_bytestr_in(bstr));
        if (rc != IB_OK) {
            throw std::runtime_error("Failed to create ByteStr field");
        }

        rc = ib_capture_acquire(ib_tx, capture, &capture_field);
        if (rc != IB_OK) {
            throw std::runtime_error("Failed to acquire capture field");
        }
        rc = ib_capture_set_item(capture_field, num, ib_tx->mp, *pfield);
        return rc;
    }
};

#define CAP_NAME "xyzzy"
TEST_F(CaptureTest, test_names)
{
    const char *name;

    name = ib_capture_name(0);
    ASSERT_STREQ("0", name);

    name = ib_capture_name(9);
    ASSERT_STREQ("9", name);

    name = ib_capture_name(10);
    ASSERT_STREQ("??", name);

    name = ib_capture_fullname(ib_tx, NULL, 0);
    ASSERT_STREQ(IB_TX_CAPTURE":0", name);

    name = ib_capture_fullname(ib_tx, NULL, 9);
    ASSERT_STREQ(IB_TX_CAPTURE":9", name);

    name = ib_capture_fullname(ib_tx, NULL, 10);
    ASSERT_STREQ(IB_TX_CAPTURE":??", name);

    name = ib_capture_fullname(ib_tx, CAP_NAME, 0);
    ASSERT_STREQ(CAP_NAME":0", name);

    name = ib_capture_fullname(ib_tx, CAP_NAME, 9);
    ASSERT_STREQ(CAP_NAME":9", name);

    name = ib_capture_fullname(ib_tx, CAP_NAME, 10);
    ASSERT_STREQ(CAP_NAME":??", name);

}

TEST_F(CaptureTest, basic)
{
    ib_status_t           rc;
    ib_field_t           *ifield;
    ib_field_t           *cfield;
    const ib_field_t     *tfield;
    const ib_bytestr_t   *bs;

    tfield = CaptureGet(0);
    ASSERT_FALSE(tfield);

    rc = CaptureBytestr(NULL, 0, "value0", &ifield);
    ASSERT_EQ(IB_OK, rc);

    tfield = CaptureGet(0);
    ASSERT_TRUE(tfield);
    ASSERT_EQ(IB_FTYPE_BYTESTR, tfield->type);
    ASSERT_EQ(IB_OK, ib_field_value(tfield, ib_ftype_bytestr_out(&bs)));
    ASSERT_EQ(6U, ib_bytestr_length(bs));
    ASSERT_EQ(0, memcmp("value0", ib_bytestr_const_ptr(bs), 6));

    rc = CaptureBytestr(NULL, 1, "xyzzy1", &ifield);
    ASSERT_EQ(IB_OK, rc);

    tfield = CaptureGet(1);
    ASSERT_EQ(IB_FTYPE_BYTESTR, tfield->type);
    ASSERT_EQ(IB_OK, ib_field_value(tfield, ib_ftype_bytestr_out(&bs)));
    ASSERT_EQ(6U, ib_bytestr_length(bs));
    ASSERT_EQ(0, memcmp("xyzzy1", ib_bytestr_const_ptr(bs), 6));

    tfield = CaptureGet(2);
    ASSERT_FALSE(tfield);

    rc = ib_capture_acquire(ib_tx, NULL, &cfield);
    ASSERT_EQ(IB_OK, rc);
    rc = ib_capture_clear(cfield);
    ASSERT_EQ(IB_OK, rc);

    tfield = CaptureGet(0);
    ASSERT_FALSE(tfield);

    tfield = CaptureGet(1);
    ASSERT_FALSE(tfield);
}

TEST_F(CaptureTest, named_collection)
{
    ib_status_t           rc;
    ib_field_t           *ifield;
    ib_field_t           *cfield;
    const ib_field_t     *tfield;
    const ib_bytestr_t   *bs;

    rc = CaptureBytestr(CAP_NAME, 0, "value0", &ifield);
    ASSERT_EQ(IB_OK, rc);

    tfield = CaptureGet(CAP_NAME, 0);
    ASSERT_EQ(IB_FTYPE_BYTESTR, tfield->type);
    ASSERT_EQ(IB_OK, ib_field_value(tfield, ib_ftype_bytestr_out(&bs)));
    ASSERT_EQ(6U, ib_bytestr_length(bs));
    ASSERT_EQ(0, memcmp("value0", ib_bytestr_const_ptr(bs), 6));

    rc = CaptureBytestr(CAP_NAME, 1, "xyzzy1", &ifield);
    ASSERT_EQ(IB_OK, rc);

    tfield = CaptureGet(CAP_NAME, 1);
    ASSERT_EQ(IB_FTYPE_BYTESTR, tfield->type);
    ASSERT_EQ(IB_OK, ib_field_value(tfield, ib_ftype_bytestr_out(&bs)));
    ASSERT_EQ(6U, ib_bytestr_length(bs));
    ASSERT_EQ(0, memcmp("xyzzy1", ib_bytestr_const_ptr(bs), 6));

    tfield = CaptureGet(CAP_NAME, 2);
    ASSERT_FALSE(tfield);

    rc = ib_capture_acquire(ib_tx, CAP_NAME, &cfield);
    ASSERT_EQ(IB_OK, rc);
    rc = ib_capture_clear(cfield);
    ASSERT_EQ(IB_OK, rc);

    tfield = CaptureGet(CAP_NAME, 0);
    ASSERT_FALSE(tfield);

    tfield = CaptureGet(CAP_NAME, 1);
    ASSERT_FALSE(tfield);
}

TEST_F(CaptureTest, collection_type)
{
    ib_status_t           rc;
    ib_field_t           *ifield;
    ib_field_t           *ofield;
    const ib_field_t     *tfield;
    const ib_bytestr_t   *bs;
    ib_var_source_t      *source;

    rc = ib_var_source_acquire(
        &source,
        ib_tx->mp,
        ib_engine_var_config_get(ib_engine),
        IB_S2SL(CAP_NAME)
    );
    ASSERT_EQ(IB_OK, rc);
    rc = ib_var_source_initialize(
        source,
        &ifield,
        ib_tx->var_store,
        IB_FTYPE_NUM
    );
    ASSERT_EQ(IB_OK, rc);

    ofield = getVar(CAP_NAME);
    ASSERT_TRUE(ofield);
    ASSERT_EQ(IB_FTYPE_NUM, ofield->type);

    rc = CaptureBytestr(CAP_NAME, 0, "value0", &ifield);
    ASSERT_EQ(IB_OK, rc);

    tfield = CaptureGet(CAP_NAME, 0);
    ASSERT_EQ(IB_FTYPE_BYTESTR, tfield->type);
    ASSERT_EQ(IB_OK, ib_field_value(tfield, ib_ftype_bytestr_out(&bs)));
    ASSERT_EQ(6U, ib_bytestr_length(bs));
    ASSERT_EQ(0, memcmp("value0", ib_bytestr_const_ptr(bs), 6));
}
