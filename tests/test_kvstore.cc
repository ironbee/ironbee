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

extern "C" {
#include "ironbee_config_auto.h"

#include <ironbee/kvstore.h>
#include <ironbee/kvstore_filesystem.h>
#include <ironbee/mpool.h>
#include <ironbee/util.h>
#include <ironbee/mpool.h>

}

#include "gtest/gtest.h"


class TestKVStore : public testing::Test
{
    public:

    kvstore_t kvstore;
    ib_mpool_t *mp;

    virtual void SetUp() {
        mkdir("TestKVStore.d", 0777);
        kvstore_filesystem_init(&kvstore, "TestKVStore.d");
        ib_mpool_create(&mp, "TestKVStore", NULL);
    }

    virtual void TearDown() {
        kvstore_filesystem_destroy(&kvstore);
        ib_mpool_destroy(mp);
    }
};

/** 
 * Exercise SetUp and TearDown.
 */
TEST_F(TestKVStore, test_init) {
    /* Nop */
}

TEST_F(TestKVStore, test_writes) {
    kvstore_key_t key;
    kvstore_value_t val;

    key.key = (void*)ib_mpool_strdup(mp, "k1");
    key.length = 2;
    val.value = (void*)ib_mpool_strdup(mp, "A key");
    val.value_length = 5;
    val.type = ib_mpool_strdup(mp, "txt");
    val.type_length = 3;
    val.expiration = 10;

    ASSERT_EQ(IB_OK, kvstore_set(&kvstore, NULL, &key, &val));
}

TEST_F(TestKVStore, DISABLED_test_reads) {
    kvstore_key_t key;
    kvstore_value_t val;

    key.key = (void*)ib_mpool_strdup(mp, "k2");
    key.length = 2;
    val.value = (void*)ib_mpool_strdup(mp, "A key");
    val.value_length = 5;
    val.type = ib_mpool_strdup(mp, "txt");
    val.type_length = 3;
    val.expiration = 10;

    ASSERT_EQ(IB_OK, kvstore_set(&kvstore, NULL, &key, &val));

    val.value = (void*)ib_mpool_strdup(mp, "Another key");
    val.value_length = 11;
    val.expiration = 5;

    ASSERT_EQ(IB_OK, kvstore_set(&kvstore, NULL, &key, &val));

    memset(&val, 0, sizeof(val));

    ASSERT_EQ(IB_OK, kvstore_get(&kvstore, NULL, &key, &val));

    ASSERT_EQ((size_t)3, val.type_length);
    ASSERT_EQ((size_t)5, val.value_length);
}

TEST_F(TestKVStore, test_removes) {
}
