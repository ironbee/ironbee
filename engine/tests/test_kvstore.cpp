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
 * @brief IronBee --- KVStore tests.
 */

extern "C" {
#include "ironbee_config_auto.h"

#include "util/kvstore_private.h"
#include <ironbee/kvstore.h>
#include <ironbee/kvstore_filesystem.h>
#include <ironbee/mpool.h>
#include <ironbee/util.h>
#include <ironbee/uuid.h>
#include <ironbee/mm_mpool.h>

}

#include "gtest/gtest.h"


class TestKVStore : public testing::Test
{
    public:

    ib_kvstore_t kvstore;
    ib_mpool_t *mp;
    ib_mm_t mm;

    virtual void SetUp() {
        mkdir("TestKVStore.d", 0777);
        ib_uuid_initialize();
        ib_kvstore_filesystem_init(&kvstore, "TestKVStore.d");
        ib_mpool_create(&mp, "TestKVStore", NULL);
        mm = ib_mm_mpool(mp);
    }

    virtual void TearDown() {
        ib_kvstore_destroy(&kvstore);
        ib_mpool_destroy(mp);
        ib_uuid_shutdown();
    }
};

/**
 * Exercise SetUp and TearDown.
 */
TEST_F(TestKVStore, test_init) {
    /* Nop */
}

TEST_F(TestKVStore, test_writes) {
    ib_kvstore_key_t key;
    ib_kvstore_value_t val;
    ib_kvstore_value_t *result;

    key.key = (void *)ib_mm_strdup(mm, "k1");
    key.length = 2;
    val.value = (void *)ib_mm_strdup(mm, "A key");
    val.value_length = 5;
    val.type = ib_mm_strdup(mm, "txt");
    val.type_length = 3;
    val.expiration = 10 * 1000000LU;

    ASSERT_EQ(IB_OK, ib_kvstore_set(&kvstore, NULL, &key, &val));

    /* Force a pruning on multiple test runs. */
    ASSERT_EQ(IB_OK, ib_kvstore_get(&kvstore, NULL, &key, &result));

    if (result) {
        ib_kvstore_free_value(&kvstore, result);
    }
}

TEST_F(TestKVStore, test_reads) {
    ib_kvstore_key_t key;
    ib_kvstore_value_t val;
    ib_kvstore_value_t *result;

    key.key = (void *)ib_mm_strdup(mm, "k2");
    key.length = 2;
    val.value = (void *)ib_mm_strdup(mm, "A key");
    val.value_length = 5;
    val.type = ib_mm_strdup(mm, "txt");
    val.type_length = 3;
    val.expiration = 10 * 1000000LU;

    ASSERT_EQ(IB_OK, ib_kvstore_set(&kvstore, NULL, &key, &val));

    val.value = (void *)ib_mm_strdup(mm, "Another key");
    val.value_length = 11;
    val.expiration = 5;

    ASSERT_EQ(IB_OK, ib_kvstore_set(&kvstore, NULL, &key, &val));

    memset(&val, 0, sizeof(val));

    ASSERT_EQ(IB_OK, ib_kvstore_get(&kvstore, NULL, &key, &result));

    ASSERT_TRUE(result);
    ASSERT_EQ((size_t)3, result->type_length);
    ASSERT_TRUE(( 11 == result->value_length) || ( 5 == result->value_length ));

    ib_kvstore_free_value(&kvstore, result);
}

TEST_F(TestKVStore, test_removes) {
    ib_kvstore_key_t key;
    ib_kvstore_value_t val;
    ib_kvstore_value_t *result;

    key.key = (void *)ib_mm_strdup(mm, "k3");
    key.length = 2;
    val.value = (void *)ib_mm_strdup(mm, "A key");
    val.value_length = 5;
    val.type = ib_mm_strdup(mm, "txt");
    val.type_length = 3;
    val.expiration = 10 * 1000000LU;

    ASSERT_EQ(IB_OK, ib_kvstore_set(&kvstore, NULL, &key, &val));
    ASSERT_EQ(IB_OK, ib_kvstore_remove(&kvstore, &key));
    ASSERT_EQ(IB_ENOENT, ib_kvstore_get(&kvstore, NULL, &key, &result));

    ASSERT_FALSE(result);
}
