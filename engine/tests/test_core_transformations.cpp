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
/// @brief IronBee --- Transformation Tests
///
/// @author Sam Baskinger <sbaskinger@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include <ironbee/action.h>
#include <ironbee/server.h>
#include <ironbee/engine.h>
#include <ironbee/transformation.h>
#include <ironbee/mm.h>
#include <ironbee/string.h>
#include <ironbee/var.h>

#include "gtest/gtest.h"

#include "base_fixture.h"

class TransformationTest : public BaseTransactionFixture
{
protected:
    void SetUp()
    {
        BaseTransactionFixture::SetUp();
        configureIronBee();
    }
};

class TransformationParaterizedTest :
    public TransformationTest,
    public ::testing::WithParamInterface<const char*>
{
};

TEST_P(TransformationParaterizedTest, EmptyStringIsValid) {
    const char* tfn_name = GetParam();
    const ib_transformation_t *tfn;
    ib_transformation_inst_t  *tfn_inst;

    ib_field_t   *fin;
    const ib_field_t   *fout;
    ib_bytestr_t *bs;

    /* Create byte string. */
    ASSERT_EQ(
        IB_OK,
        ib_bytestr_alias_nulstr(&bs, MainMM(), "")
    );

    /* Create in field. */
    ASSERT_EQ(
        IB_OK,
        ib_field_create(
            &fin,
            MainMM(),
            IB_S2SL("empty string"),
            IB_FTYPE_BYTESTR,
            ib_ftype_bytestr_in(bs)
        )
    );

    ASSERT_EQ(
        IB_OK,
        ib_transformation_lookup(ib_engine, IB_S2SL(tfn_name), &tfn)
    );

    ASSERT_EQ(
        IB_OK,
        ib_transformation_inst_create(
            &tfn_inst,
            MainMM(),
            tfn,
            "any value"
        )
    );

    ASSERT_EQ(
        IB_OK,
        ib_transformation_inst_execute(
            tfn_inst,
            MainMM(),
            fin,
            &fout
        )
    );

    ASSERT_EQ(fin, fout);
}

INSTANTIATE_TEST_CASE_P(
    TransformationsWithEmptyString,
    TransformationParaterizedTest,
    ::testing::Values(
        "lowercase",
        "trimLeft",
        "trimRight",
        "trim",
        "removeWhitespace",
        "compressWhitespace",
        "urlDecode",
        "htmlEntityDecode",
        "normalizePath",
        "normalizePathWin"
    )
);
