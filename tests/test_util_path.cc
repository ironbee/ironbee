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
/// @brief IronBee - Path Test Functions
/// 
/// @author Nick LeRoy <nleroy@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "ironbee_config_auto.h"

#include <ironbee/util.h>

#include "ironbee_private.h"

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

#include <string.h>
#include <stdexcept>

struct test_path_data_t {
    int            lineno;
    const char    *in1;
    const char    *in2;
    const char    *out;
};

static struct test_path_data_t test_path_join[] = {
    { __LINE__, "/",     "a/b",    "/a/b" },
    { __LINE__, "/a",    "b/c",    "/a/b/c" },
    { __LINE__, "/a",    "/b/c/",  "/a/b/c" },
    { __LINE__, "/a/",   "b/c",    "/a/b/c" },
    { __LINE__, "/a///", "b/c",    "/a/b/c" },
    { __LINE__, "/a/",   "///b/c", "/a/b/c" },
    { __LINE__, NULL,    NULL,     NULL },
};

static struct test_path_data_t test_rel_file[] = {
    { __LINE__, "x.conf",        "y.conf",      "./y.conf" },
    { __LINE__, "x.conf",        "y.conf",      "./y.conf" },
    { __LINE__, "./x.conf",      "y.conf",      "./y.conf" },
    { __LINE__, "./x.conf",      "a/y.conf",    "./a/y.conf" },
    { __LINE__, "/x.conf",       "a/y.conf",    "/a/y.conf" },
    { __LINE__, "/a/b/c/x.conf", "d/y.conf",    "/a/b/c/d/y.conf" },
    { __LINE__, "/a/x.conf",     "/b/c/y.conf", "/b/c/y.conf" },
    { __LINE__, "/a/x.conf",     "b/c/y.conf",  "/a/b/c/y.conf" },
    { __LINE__, "/a///x.conf",   "b/c/y.conf",  "/a/b/c/y.conf" },
    { __LINE__, NULL,     NULL,          NULL },
};

/* -- Tests -- */
class TestIBUtilPath : public ::testing::Test
{
public:
    TestIBUtilPath()
    {
        ib_status_t rc;
        
        ib_initialize();
        rc = ib_mpool_create(&m_pool, NULL, NULL);
        if (rc != IB_OK) {
            throw std::runtime_error("Could not create mpool.");
        }
    }

    ~TestIBUtilPath()
    {
        ib_shutdown();        
    }

protected:
    ib_mpool_t* m_pool;
};

/// @test Test util path functions - ib_util_relative_path()
TEST_F(TestIBUtilPath, path_join)
{
    struct test_path_data_t *test;

    for (test = &test_path_join[0]; test->in1 != NULL; ++test) {
        const char *out;
        out = ib_util_path_join(m_pool, test->in1, test->in2);
        EXPECT_STREQ(test->out, out)
            << "Test: in1 = '" << test->in1 << "'"
            << ", in2 = '" << test->in2 << "'";
    }
}

/// @test Test util path functions - ib_util_relative_path()
TEST_F(TestIBUtilPath, relative_path)
{
    struct test_path_data_t *test;

    for (test = &test_rel_file[0]; test->in1 != NULL; ++test) {
        const char *out;
        out = ib_util_relative_file(m_pool, test->in1, test->in2);
        EXPECT_STREQ(test->out, out)
            << "Test: in1 = '" << test->in1 << "'"
            << ", in2 = '" << test->in2 << "'";
    }
}
