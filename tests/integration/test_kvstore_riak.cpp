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

#include "ironbee_config_auto.h"
#include "../gtest/gtest.h"
#include "../gtest/gtest-spi.h"

#include <iostream>
#include <sstream>

extern "C" {
#include <ironbee/util.h>
#include <ironbee/kvstore_riak.h>
}

class RiakFixture : public ::testing::Test {
    public:

    const char *base_url;
    const char *bucket;
    ib_kvstore_t kvstore;

    virtual void SetUp(){
        using ::testing::TestInfo;
        using ::testing::UnitTest;

        const TestInfo* const info =
            UnitTest::GetInstance()->current_test_info();

        ib_initialize();
        base_url = "http://localhost:8098";
        bucket = "UnitTestBucket";

        std::ostringstream oss;
        oss << info->test_case_name() << "_" << info->name();

        bucket = strdup(oss.str().c_str());

        ib_kvstore_riak_init(
            &kvstore,
            base_url,
            bucket,
            NULL);
        ib_kvstore_connect(&kvstore);
    }

    virtual void TearDown(){
        ib_kvstore_disconnect(&kvstore);
        ib_kvstore_destroy(&kvstore);
        ib_shutdown();
        free(const_cast<char *>(bucket));
    }
};

/* Simple test. If this fails, don't bother continuing. */
TEST_F(RiakFixture, PING_OK){
    ASSERT_EQ(1, ib_kvstore_riak_ping(&kvstore));
}

/* This test needs failure and so re-inits to point at a non-server. */
TEST_F(RiakFixture, PING_FAIL){

    /* Undo fixture setup. */
    ib_kvstore_disconnect(&kvstore);
    ib_kvstore_destroy(&kvstore);

    /* Re-init with invalid URL. */
    ib_kvstore_riak_init(
        &kvstore,
        "http://localhost:1025",
        bucket,
        NULL);
    ib_kvstore_connect(&kvstore);

    ASSERT_EQ(0, ib_kvstore_riak_ping(&kvstore));

}

TEST_F(RiakFixture, Write) {
    ib_kvstore_key_t key = { "key1", 4 };
    ib_kvstore_value_t val = {
        const_cast<char *>("val1"), 4,
        const_cast<char *>("text/plain"),
        10,
        0
    };

    ib_kvstore_set(&kvstore, NULL, &key, &val);
}

TEST_F(RiakFixture, Read) {
    ib_kvstore_key_t key = { "key1", 4 };
    ib_kvstore_value_t val = {
        const_cast<char *>("val1"), 4,
        const_cast<char *>("text/plain"),
        10,
        0
    };
    ib_kvstore_value_t *val2;

    ib_kvstore_set(&kvstore, NULL, &key, &val);
    ib_kvstore_get(&kvstore, NULL, &key, &val2);

    ASSERT_TRUE(val2);
    ASSERT_EQ(val.value_length, val2->value_length);
    ASSERT_EQ(
        0,
        strncmp(
            reinterpret_cast<const char *>(val.value),
            reinterpret_cast<const char *>(val2->value),
            val.value_length));
}

