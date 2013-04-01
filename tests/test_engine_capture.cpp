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
#include <ironbee/data.h>
#include <ironbee/field.h>
#include <ironbee/list.h>
#include <ironbee/mpool.h>

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
    ib_status_t CaptureGet(
        int          num,
        ib_field_t **pfield)
    {
        return ib_data_get(ib_tx->data,
                           ib_capture_fullname(ib_tx, NULL, 0),
                           pfield);
    };

    ib_status_t CaptureGet(
        const char  *capture,
        int          num,
        ib_field_t **pfield)
    {
        return ib_data_get(ib_tx->data,
                           ib_capture_fullname(ib_tx, capture, 0),
                           pfield);
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

        name = ib_capture_name(num);
        rc = ib_bytestr_dup_nulstr(&bstr, MainPool(), value);
        if (rc != IB_OK) {
            throw std::runtime_error("Failed to dup NulStr into ByteStr");
        }
        rc = ib_field_create(pfield, MainPool(),
                             IB_FIELD_NAME(name),
                             IB_FTYPE_BYTESTR,
                             ib_ftype_bytestr_in(bstr));
        if (rc != IB_OK) {
            throw std::runtime_error("Failed to create ByteStr field");
        }
        rc = ib_capture_set_item(ib_tx, capture, num, *pfield);
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
    ib_status_t         rc;
    ib_field_t         *ifield;
    ib_field_t         *ofield;
    const ib_bytestr_t *bs;

    rc = CaptureGet(0, &ofield);
    ASSERT_EQ(IB_ENOENT, rc);

    rc = CaptureBytestr(NULL, 0, "value0", &ifield);
    ASSERT_EQ(IB_OK, rc);

    rc = CaptureGet(0, &ofield);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(IB_FTYPE_BYTESTR, ofield->type);
    ASSERT_EQ(IB_OK, ib_field_value(ofield, ib_ftype_bytestr_out(&bs)));
    ASSERT_EQ(0, memcmp("value0", ib_bytestr_const_ptr(bs), 5));

    rc = CaptureGet(1, &ofield);
    ASSERT_EQ(IB_ENOENT, rc);

    rc = CaptureBytestr(NULL, 1, "value1", &ifield);
    ASSERT_EQ(IB_OK, rc);

    rc = CaptureGet(1, &ofield);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(IB_FTYPE_BYTESTR, ofield->type);
    ASSERT_EQ(IB_OK, ib_field_value(ofield, ib_ftype_bytestr_out(&bs)));
    ASSERT_EQ(0, memcmp("value1", ib_bytestr_const_ptr(bs), 5));

    rc = CaptureGet(2, &ofield);
    ASSERT_EQ(IB_ENOENT, rc);

    rc = ib_capture_clear(ib_tx, NULL);
    ASSERT_EQ(IB_OK, rc);
    rc = CaptureGet(0, &ofield);
    ASSERT_EQ(IB_ENOENT, rc);
    rc = CaptureGet(1, &ofield);
    ASSERT_EQ(IB_ENOENT, rc);
    rc = CaptureGet(2, &ofield);
    ASSERT_EQ(IB_ENOENT, rc);
}

TEST_F(CaptureTest, collection_type)
{
    ib_status_t         rc;
    ib_field_t         *ifield;
    ib_field_t         *ofield;
    const ib_bytestr_t *bs;
    ib_num_t            n = 666;

    rc = ib_field_create(&ofield, MainPool(),
                         IB_FIELD_NAME(CAP_NAME),
                         IB_FTYPE_NUM, ib_ftype_num_in(&n));
    ASSERT_EQ(IB_OK, rc);
    rc = ib_data_get(ib_tx->data,
                     ib_capture_fullname(ib_tx, CAP_NAME, 0),
                     &ofield);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(IB_FTYPE_NUM, ofield->type);

    rc = CaptureBytestr(CAP_NAME, 0, "value0", &ifield);
    ASSERT_EQ(IB_OK, rc);

    rc = CaptureGet(0, &ofield);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(IB_FTYPE_BYTESTR, ofield->type);
    ASSERT_EQ(IB_OK, ib_field_value(ofield, ib_ftype_bytestr_out(&bs)));
    ASSERT_EQ(0, memcmp("value0", ib_bytestr_const_ptr(bs), 5));

    rc = CaptureGet(1, &ofield);
    ASSERT_EQ(IB_ENOENT, rc);

    rc = CaptureBytestr(CAP_NAME, 1, "value1", &ifield);
    ASSERT_EQ(IB_OK, rc);

    rc = CaptureGet(1, &ofield);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(IB_FTYPE_BYTESTR, ofield->type);
    ASSERT_EQ(IB_OK, ib_field_value(ofield, ib_ftype_bytestr_out(&bs)));
    ASSERT_EQ(0, memcmp("value1", ib_bytestr_const_ptr(bs), 5));

    rc = CaptureGet(2, &ofield);
    ASSERT_EQ(IB_ENOENT, rc);

    rc = ib_capture_clear(ib_tx, CAP_NAME);
    ASSERT_EQ(IB_OK, rc);
    rc = CaptureGet(CAP_NAME, 0, &ofield);
    ASSERT_EQ(IB_ENOENT, rc);
    rc = CaptureGet(CAP_NAME, 1, &ofield);
    ASSERT_EQ(IB_ENOENT, rc);
    rc = CaptureGet(CAP_NAME, 2, &ofield);
    ASSERT_EQ(IB_ENOENT, rc);
}
