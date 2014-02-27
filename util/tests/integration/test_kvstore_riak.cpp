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

/**
 * @file
 * @brief IronBee --- Riak kvstore tests.
 */

#include "ironbee_config_auto.h"
#include "../gtest/gtest.h"

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
    ib_kvstore_key_t key;
    ib_kvstore_value_t val;

    virtual void SetUp(){
        using ::testing::TestInfo;
        using ::testing::UnitTest;

        const TestInfo* const info =
            UnitTest::GetInstance()->current_test_info();

        ib_util_initialize();
        base_url = "http://localhost:8098";
        bucket = "UnitTestBucket";

        std::ostringstream oss;
        oss << info->test_case_name() << "_" << info->name();

        bucket = strdup(oss.str().c_str());

        ib_kvstore_riak_init(
            &kvstore,
            "myTestClient",
            base_url,
            bucket,
            IB_MM_NULL);
        ib_kvstore_connect(&kvstore);

        /* Initialize the key. */
        key.key = reinterpret_cast<const void *>("key1");
        key.length = 4;

        val.value = const_cast<char *>("val1");
        val.value_length = 4;
        val.type = const_cast<char *>("text/plain");
        val.type_length = 10;
        val.expiration = 0;

        /* Delete it from whatever test we are in. */
        ib_kvstore_remove(&kvstore, &key);
    }

    virtual void TearDown(){
        ib_kvstore_disconnect(&kvstore);
        ib_kvstore_destroy(&kvstore);
        ib_util_shutdown();
        free(const_cast<char *>(bucket));
    }
};

/* Simple test. If this fails, don't bother continuing. */
TEST_F(RiakFixture, PING_OK){
    ASSERT_EQ(1, ib_kvstore_riak_ping(&kvstore));
}

/* This test needs failure and so re-initializes to point at a non-server. */
TEST_F(RiakFixture, PING_FAIL){

    /* Undo fixture setup. */
    ib_kvstore_disconnect(&kvstore);
    ib_kvstore_destroy(&kvstore);

    /* Re-init with invalid URL. */
    ib_kvstore_riak_init(
        &kvstore,
        "myTestClient",
        "http://localhost:1025",
        bucket,
        NULL);
    ib_kvstore_connect(&kvstore);

    ASSERT_EQ(0, ib_kvstore_riak_ping(&kvstore));

}

TEST_F(RiakFixture, Write) {
    ib_kvstore_set(&kvstore, NULL, &key, &val);
}

extern "C" {
    static ib_status_t counting_merge_policy(
        ib_kvstore_t *kvstore,
        ib_kvstore_value_t **values,
        size_t value_length,
        ib_kvstore_value_t **resultant_value,
        ib_kvstore_cbdata_t *cbdata)
    {
        int *i = reinterpret_cast<int *>(cbdata);

        *i = value_length;

        if ( value_length > 0 ) {
            *resultant_value = values[0];
        }

        return IB_OK;
    }
}

TEST_F(RiakFixture, WriteWithVectorClock) {
    int count;
    ib_kvstore_value_t *val2;

    /* Allow multiple versions. */
    ib_kvstore_riak_set_bucket_property_str(&kvstore, "allow_mult", "true");

    ib_kvstore_get(&kvstore, &counting_merge_policy, &key, &val2);
    if (val2) {
        ib_kvstore_free_value(&kvstore, val2);
    }

    /* We should not get multiples back. */
    ib_kvstore_set(&kvstore, NULL, &key, &val);
    ib_kvstore_set(&kvstore, NULL, &key, &val);

    kvstore.merge_policy_cbdata = reinterpret_cast<void*>(&count);

    /* Fetching will set the vclock. */
    ib_kvstore_get(&kvstore, &counting_merge_policy, &key, &val2);
    if (val2) {
        ib_kvstore_free_value(&kvstore, val2);
    }

    /* Set with VClock set should wipe out value. */
    ib_kvstore_set(&kvstore, NULL, &key, &val);

    /* Reset count to 1. The merge policy is not called on single responses. */
    count = 1;

    /* Fetching will set the vclock. */
    ib_kvstore_get(&kvstore, &counting_merge_policy, &key, &val2);
    if (val2) {
        ib_kvstore_free_value(&kvstore, val2);
    }

    ASSERT_EQ(1, count);
}

TEST_F(RiakFixture, Read) {
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

    ib_kvstore_free_value(&kvstore, val2);
}

TEST_F(RiakFixture, Remove) {
    ib_kvstore_value_t *val2;

    ib_kvstore_set(&kvstore, NULL, &key, &val);
    ib_kvstore_remove(&kvstore, &key);
    ASSERT_EQ(IB_OK, ib_kvstore_get(&kvstore, NULL, &key, &val2));

    ASSERT_FALSE(val2);
}

TEST_F(RiakFixture, Multi) {
    ib_kvstore_value_t *val2;

    /* Turn off last-write-wins semantics. */
    ASSERT_EQ(
        IB_OK,
        ib_kvstore_riak_set_bucket_property_str(
            &kvstore,
            "last_write_wins",
            "false"));

    /* Allow multiple versions. */
    ASSERT_EQ(
        IB_OK,
        ib_kvstore_riak_set_bucket_property_str(
            &kvstore,
            "allow_mult",
            "true"));

    /* Number of replicas. */
    ASSERT_EQ(
        IB_OK,
        ib_kvstore_riak_set_bucket_property_int(&kvstore, "n_val", 3));
    /* Set read-write requirement. */
    ASSERT_EQ(
        IB_OK,
        ib_kvstore_riak_set_bucket_property_str(&kvstore, "rw", "quorum"));
    /* Set durable write requirement. */
    ASSERT_EQ(
        IB_OK,
        ib_kvstore_riak_set_bucket_property_str(&kvstore, "dw", "quorum"));
    ASSERT_EQ(
        IB_OK,
        ib_kvstore_riak_set_bucket_property_str(&kvstore, "r", "quorum"));
    ASSERT_EQ(
        IB_OK,
        ib_kvstore_riak_set_bucket_property_str(&kvstore, "w", "quorum"));

    /* Ensure that there is nothing in riak when starting out. */
    val2 = NULL;
    ASSERT_EQ(IB_OK, ib_kvstore_get(&kvstore, NULL, &key, &val2));
    ASSERT_FALSE(val2);

    /* Write an initial value, and fetch it. */
    ASSERT_EQ(IB_OK, ib_kvstore_set(&kvstore, NULL, &key, &val));
    ASSERT_EQ(IB_OK, ib_kvstore_get(&kvstore, NULL, &key, &val2));

    /* Check that a single write works. */
    ASSERT_TRUE(val2);
    ASSERT_EQ(val.value_length, val2->value_length);
    ASSERT_EQ(
        0,
        strncmp(
            reinterpret_cast<const char *>(val.value),
            reinterpret_cast<const char *>(val2->value),
            val.value_length));
    ib_kvstore_free_value(&kvstore, val2);
    val2 = NULL;

    /* Clear kvstore vclock and etag to force a duplicate entry. */
    ib_kvstore_riak_set_vclock(&kvstore, NULL);
    ib_kvstore_riak_set_etag(&kvstore, NULL);

    ASSERT_EQ(IB_OK, ib_kvstore_set(&kvstore, NULL, &key, &val));
    ASSERT_EQ(IB_OK, ib_kvstore_get(&kvstore, NULL, &key, &val2));

    ASSERT_TRUE(val2);
    ASSERT_EQ(val.value_length, val2->value_length);
    ASSERT_EQ(
        0,
        strncmp(
            reinterpret_cast<const char *>(val.value),
            reinterpret_cast<const char *>(val2->value),
            val.value_length));

    ib_kvstore_free_value(&kvstore, val2);
}
