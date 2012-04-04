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
 *****************************************************************************/

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
#include <ironbee/mpool.h>

#include <list>

class ParsedContentTest : public BaseFixture {
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

class ParsedContentHeaderTest : public ParsedContentTest {
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

    ib_parsed_header_t *headers;

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
    ib_parsed_tx_t *t;

    ASSERT_IB_OK(ib_parsed_tx_create(tx_mpool, &t, ib_engine));
    ASSERT_TRUE(t!=NULL);

    ib_parsed_tx_destroy(t);
}


TEST_F(ParsedContentHeaderTest, list_err)
{
    ib_status_t rc;

    ASSERT_IB_OK(ib_parsed_header_create(&headers, tx_mpool));
    ASSERT_TRUE(headers!=NULL);

    ASSERT_IB_OK(ib_parsed_header_add(headers,
                                      name1,
                                      strlen(name1),
                                      value1,
                                      strlen(value1)));
    ASSERT_IB_OK(ib_parsed_header_add(headers,
                                      name2,
                                      strlen(name2),
                                      value2,
                                      strlen(value2)));
    ASSERT_IB_OK(ib_parsed_header_add(headers,
                                      name3,
                                      strlen(name3),
                                      value3,
                                      strlen(value3)));

    ASSERT_EQ(3U, ib_parsed_header_list_size(headers));


    rc = ib_parsed_tx_each_header(headers,
                                  &ParsedContentHeaderTest::list_callback1,
                                  this);
    ASSERT_EQ(IB_EOTHER, rc);
    ASSERT_EQ(1, count);
}

TEST_F(ParsedContentHeaderTest, list_ok)
{
    ib_status_t rc;

    ASSERT_IB_OK(ib_parsed_header_create(&headers, tx_mpool));
    ASSERT_TRUE(headers!=NULL);

    ASSERT_IB_OK(ib_parsed_header_add(headers,
                                      name1,
                                      strlen(name1),
                                      value1,
                                      strlen(value1)));
    ASSERT_IB_OK(ib_parsed_header_add(headers,
                                      name2,
                                      strlen(name2),
                                      value2,
                                      strlen(value2)));
    ASSERT_IB_OK(ib_parsed_header_add(headers,
                                      name3,
                                      strlen(name3),
                                      value3,
                                      strlen(value3)));

    ASSERT_EQ(3U, ib_parsed_header_list_size(headers));


    rc = ib_parsed_tx_each_header(headers,
                                  &ParsedContentHeaderTest::list_callback2,
                                  this);
    ASSERT_EQ(3U, names.size());
    ASSERT_EQ(3U, values.size());
    ASSERT_EQ(IB_OK, rc);

    std::list<const char*>::const_iterator it = names.begin();
    ASSERT_STREQ(name1, *it);
    ASSERT_STREQ(name2, *(++it));
    ASSERT_STREQ(name3, *(++it));

    it = values.begin();
    ASSERT_STREQ(value1, *it);
    ASSERT_STREQ(value2, *(++it));
    ASSERT_STREQ(value3, *(++it));
}
