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

extern "C" {
#include <ironbee/util.h>
#include <ironbee/kvstore_riak.h>
}

class RiakFixture : public ::testing::Test {
    public:

    const char *base_url;
    const char *bucket;

    virtual void SetUp(){
        ib_initialize();
        base_url = "http://localhost:8098";
        bucket = "UnitTestBucket";
    }

    virtual void TearDown(){
        ib_shutdown();
    }
};

/* Simple test. If this fails, don't bother continuing. */
TEST_F(RiakFixture, PING_OK){
    ib_kvstore_t kvstore;

    ib_kvstore_riak_init(
        &kvstore,
        base_url,
        bucket,
        NULL);
    ib_kvstore_connect(&kvstore);

    ASSERT_EQ(1, ib_kvstore_riak_ping(&kvstore));

    ib_kvstore_disconnect(&kvstore);
}

TEST_F(RiakFixture, PING_FAIL){
    ib_kvstore_t kvstore;

    ib_kvstore_riak_init(
        &kvstore,
        "http://localhost:1025",
        bucket,
        NULL);
    ib_kvstore_connect(&kvstore);

    ASSERT_EQ(0, ib_kvstore_riak_ping(&kvstore));

    ib_kvstore_disconnect(&kvstore);
}
