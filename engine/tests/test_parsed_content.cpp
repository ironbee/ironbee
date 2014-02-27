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
 * @brief Tests of IronBee interface for handling parsed content.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

/* Testing fixture. */
#include "base_fixture.h"

/* Header of what we are testing. */
#include <ironbee/parsed_content.h>
#include <ironbee/mm.h>

// TODO: Put this in the base fixture
#define ASSERT_MEMEQ(a,b,n) \
        ASSERT_PRED3(ibtest_assert_memeq,(a),(b),(n))

bool ibtest_assert_memeq(const void *v1, const void *v2, size_t n)
{
    return ! memcmp(v1, v2, n);
}

#include <list>

class ParsedContentTest : public BaseFixture
{
    protected:
    ib_mpool_t *tx_mpool;

    public:

    virtual void SetUp() {
        BaseFixture::SetUp();
        ib_mpool_create(&tx_mpool, "HI", NULL);
    }

    virtual void TearDown() {
        ib_mpool_destroy(tx_mpool);
        BaseFixture::TearDown();
    }

    virtual ~ParsedContentTest(){}
};

class ParsedContentHeaderTest : public ParsedContentTest
{
    public:

    ParsedContentHeaderTest(){
        name1 = "name1";
        name2 = "name2";
        name3 = "name3";
        value1 = "value1";
        value2 = "value2";
        value3 = "value3";
    }

    virtual ~ParsedContentHeaderTest(){}

    virtual void SetUp() {
        ParsedContentTest::SetUp();
        names.clear();
        values.clear();
        count = 0;
    }

    virtual void TearDown() {
        ParsedContentTest::TearDown();
    }

    protected:
    std::list<const char*> names;
    std::list<const char*> values;
    int count;

    ib_parsed_headers_t *headers;

    const char *name1;
    const char *value1;
    const char *name2;
    const char *value2;
    const char *name3;
    const char *value3;

    /**
     * Test that non-IB_OK status codes result in failure.
     */
    static ib_status_t list_callback1(const char *name,
                                      size_t name_len,
                                      const char *value,
                                      size_t value_len,
                                      void *user_data)
    {
        ParsedContentHeaderTest *o =
            reinterpret_cast<ParsedContentHeaderTest*>(user_data);

        o->count = o->count + 1;

        return IB_EOTHER;
    }
    static ib_status_t list_callback2(const char *name,
                                      size_t name_len,
                                      const char *value,
                                      size_t value_len,
                                      void *user_data)
    {
        using std::list;

        ParsedContentHeaderTest *o =
            reinterpret_cast<ParsedContentHeaderTest*>(user_data);

        o->count = o->count + 1;
        o->names.push_back(name);
        o->values.push_back(value);

        return IB_OK;
    }

};

TEST_F(ParsedContentTest, create_destroy)
{
    resetRuleBasePath();
    resetModuleBasePath();
    configureIronBee();
    ib_conn_t *c = buildIronBeeConnection();
    ib_tx_t *tx;


    ASSERT_IB_OK(ib_tx_create(&tx, c, NULL));
    ASSERT_TRUE(tx!=NULL);

    ib_tx_destroy(tx);
    ib_conn_destroy(c);
}
