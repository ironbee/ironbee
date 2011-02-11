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
/// @brief IronBee - Engine Test Functions
/// 
/// @author Brian Rectanus <brectanus@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

#define TESTING

#include "engine/engine.c"
#include "engine/logger.c"
#include "engine/provider.c"
#include "engine/parser.c"
#include "engine/config.c"
#include "engine/config-parser.c"
#include "engine/tfn.c"
#include "engine/core.c"
#include "util/debug.c"

static ib_plugin_t ibplugin = {
    IB_PLUGIN_HEADER_DEFAULTS,
    "unit_tests"
};

/// @test Test ironbee library - ib_engine_create()
TEST(TestIronBee, test_engine_create_null_plugin)
{
    ib_engine_t *ib;
    ib_status_t rc;

    atexit(ib_shutdown);
    rc = ib_initialize();
    ASSERT_TRUE(rc == IB_OK) << "ib_initialize() failed - rc != IB_OK";

    rc = ib_engine_create(&ib, NULL);
    ASSERT_TRUE(rc == IB_EINVAL) << "ib_engine_create() did not fail with IB_EINVAL";
    ASSERT_TRUE(ib == NULL) << "ib_engine_create() succeeded with NULL plugin handle";
}

/// @test Test ironbee library - ib_engine_create() and ib_engine_destroy()
TEST(TestIronBee, test_engine_create_and_destroy)
{
    ib_engine_t *ib;
    ib_status_t rc;

    atexit(ib_shutdown);
    rc = ib_initialize();
    ASSERT_TRUE(rc == IB_OK) << "ib_initialize() failed - rc != IB_OK";

    rc = ib_engine_create(&ib, &ibplugin);
    ASSERT_TRUE(rc == IB_OK) << "ib_engine_create() failed - rc != IB_OK";
    ASSERT_TRUE(ib != NULL) << "ib_engine_create() failed - NULL";
    ASSERT_TRUE(ib->mp != NULL) << "ib_engine_create() - NULL mp";

    ib_engine_destroy(ib);
}

static ib_status_t foo2bar(void *fndata,
                           uint8_t *data_in, size_t dlen_in,
                           uint8_t **data_out, size_t *dlen_out,
                           ib_flags_t *pflags)
{
    if (data_in && dlen_in == 3 && strncmp("foo", (char *)data_in, 3) == 0) {
        *data_out = data_in;
        *dlen_out = dlen_in;
        *pflags = IB_TFN_FMODIFIED | IB_TFN_FINPLACE;
        (*data_out)[0] = 'b';
        (*data_out)[1] = 'a';
        (*data_out)[2] = 'r';
    }

    return IB_OK;
}

/// @test Test ironbee library - transformation registration
TEST(TestIronBee, test_tfn)
{
    ib_engine_t *ib;
    ib_tfn_t *tfn = (ib_tfn_t *)-1;
    uint8_t *data_in;
    size_t dlen_in;
    uint8_t *data_out;
    size_t dlen_out;
    ib_flags_t flags;
    ib_status_t rc;

    atexit(ib_shutdown);
    rc = ib_initialize();
    ASSERT_TRUE(rc == IB_OK) << "ib_initialize() failed - rc != IB_OK";

    data_in = (uint8_t *)malloc(128);
    ASSERT_TRUE(data_in != NULL) << "data_in alloc failed - NULL";
    data_out = (uint8_t *)malloc(128);
    ASSERT_TRUE(data_out != NULL) << "data_out alloc failed - NULL";

    rc = ib_engine_create(&ib, &ibplugin);
    ASSERT_TRUE(rc == IB_OK) << "ib_engine_create() failed - rc != IB_OK";
    ASSERT_TRUE(ib != NULL) << "ib_engine_create() failed - NULL";
    ASSERT_TRUE(ib->mp != NULL) << "ib_engine_create() - NULL mp";

    rc = ib_tfn_create(ib, "foo2bar", foo2bar, NULL, NULL);
    ASSERT_TRUE(rc == IB_OK) << "ib_tfn_create() failed - rc != IB_OK";

    rc = ib_tfn_lookup(ib, "foo2bar", &tfn);
    ASSERT_TRUE(tfn != (ib_tfn_t *)-1) << "ib_tfn_lookup() failed - unset";
    ASSERT_TRUE(tfn != NULL) << "ib_tfn_lookup() failed - NULL";

    memcpy(data_in, "foo", 4);
    dlen_in = 3;
    rc = ib_tfn_transform(tfn, data_in, dlen_in, &data_out, &dlen_out, &flags);
    ASSERT_TRUE(tfn != (ib_tfn_t *)-1) << "ib_tfn_lookup() failed - unset";
    ASSERT_TRUE(IB_TFN_CHECK_FMODIFIED(flags)) << "ib_tfn_lookup() failed - not modified";
    ASSERT_TRUE(IB_TFN_CHECK_FINPLACE(flags)) << "ib_tfn_lookup() failed - not inplace";

    ib_engine_destroy(ib);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    ib_trace_init(NULL);
    return RUN_ALL_TESTS();
}
