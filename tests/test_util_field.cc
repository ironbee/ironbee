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
/// @brief IronBee - Field Test Functions
/// 
/// @author Brian Rectanus <brectanus@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "ironbee_config_auto.h"

#include <ironbee/field.h>
#include <ironbee/util.h>
#include <ironbee/mpool.h>
#include <ironbee/bytestr.h>

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

#include <stdexcept>

class TestIBUtilField : public ::testing::Test
{
public:
    TestIBUtilField()
    {
        ib_status_t rc;
        
        ib_initialize();
        rc = ib_mpool_create(&m_pool, NULL, NULL);
        if (rc != IB_OK) {
            throw std::runtime_error("Could not create mpool.");
        }
    }
    
    ~TestIBUtilField()
    {
        ib_shutdown();        
    }
    
protected:
    ib_mpool_t* m_pool;
};

/* -- Tests -- */

/// @test Test util field library - ib_field_create() ib_field_create_ex()
TEST_F(TestIBUtilField, test_field_create)
{
    ib_field_t *f;
    ib_status_t rc;
    const char *nulstrval = "TestValue";
    ib_num_t numval = 5;
    ib_bytestr_t *bytestrval;

    rc = ib_field_create(&f, m_pool, "test_nulstr", IB_FTYPE_NULSTR, &nulstrval);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(f);
    ASSERT_EQ(11UL, f->nlen);
    ASSERT_EQ(0, memcmp("test_nulstr", f->name, 11));

    rc = ib_field_create(&f, m_pool, "test_num", IB_FTYPE_NUM, &numval);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(f);
    ASSERT_EQ(8UL, f->nlen);
    ASSERT_EQ(0, memcmp("test_num", f->name, 8));

    rc = ib_bytestr_dup_mem(&bytestrval, m_pool, (uint8_t *)nulstrval, strlen(nulstrval));
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(f);

    rc = ib_field_create(&f, m_pool, "test_bytestr", IB_FTYPE_BYTESTR, &bytestrval);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(f);
    ASSERT_EQ(12UL, f->nlen);
    ASSERT_EQ(0, memcmp("test_bytestr", f->name, 12));

    rc = ib_field_create_ex(&f, m_pool, "test_nulstr_ex", 14, IB_FTYPE_NULSTR, &nulstrval);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(f);

    rc = ib_field_create_ex(&f, m_pool, "test_num_ex", 11, IB_FTYPE_NUM, &numval);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(f);

    rc = ib_field_create_ex(&f, m_pool, "test_bytestr_ex", 15, IB_FTYPE_BYTESTR, &bytestrval);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(f);
}

// Globals used to test if dyn_* caching is working
static int g_dyn_call_count;
static char g_dyn_call_val[1024];

// Dynamic get function which increments a global counter and modifies
// a global buffer so that the number of calls can be tracked.  One of the
// tests is to determine if the function was called only once (result
// cached).
static void *dyn_get(ib_field_t *f,
                     const void *arg,
                     size_t alen,
                     void *data)
{
    /* Keep track of how many times this was called */
    ++g_dyn_call_count;

    snprintf(g_dyn_call_val, sizeof(g_dyn_call_val), "testval_%s_%.*s_call%02d", (const char *)data, (int)alen, (const char *)arg, g_dyn_call_count);

    return (void *)g_dyn_call_val;
}

// Cached version of the above dyn_get function.
static void *dyn_get_cached(ib_field_t *f,
                            const void *arg,
                            size_t alen,
                            void *data)
{
    /* Call the get function */
    void *cval = dyn_get(f, arg, alen, data);

    /* Cache the value */
    ib_field_setv_static(f, &cval);
    
    return (void *)cval;
}

static ib_status_t dyn_set(
    ib_field_t *field,
    const void *arg, size_t alen,
    void *val, 
    void *data
)
{
    ++g_dyn_call_count;
    
    snprintf(g_dyn_call_val, sizeof(g_dyn_call_val), "testval_%s_%.*s_%s_call%02d", (const char *)data, (int)alen, (const char *)arg, (const char*)val, g_dyn_call_count);
    
    return IB_OK;
}

///@test Test util field library - ib_field_dyn_register_get()
TEST_F(TestIBUtilField, test_dyn_field)
{
    ib_field_t *dynf;
    ib_field_t *cdynf;
    ib_status_t rc;
    const char *fval;

    /* Create a field with no initial value. */
    rc = ib_field_create(&dynf, m_pool, "test_dynf", IB_FTYPE_NULSTR, NULL);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(dynf);
    ASSERT_EQ(9UL, dynf->nlen);
    ASSERT_EQ(0, memcmp("test_dynf", dynf->name, 9));

    ib_field_dyn_register_get(dynf, dyn_get, (void*)"dynf_get");
    ib_field_dyn_register_set(dynf, dyn_set, (void*)"dynf_set");

    /* Get the value from the dynamic field. */
    fval = ib_field_value_nulstr_ex(dynf, (void *)"fetch1", 6);
    ASSERT_TRUE(fval);
    ASSERT_EQ(
        std::string("testval_dynf_get_fetch1_call01"), 
        fval
    );

    /* Get the value from the dynamic field again. */
    fval = ib_field_value_nulstr_ex(dynf, (void *)"fetch2", 6);
    ASSERT_TRUE(fval);
    ASSERT_EQ(
        std::string("testval_dynf_get_fetch2_call02"),
        fval
    );
    
    /* Set */
    rc = ib_field_setv_ex(dynf, (void*)"val1", (void*)"set1", 4);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(std::string("testval_dynf_set_set1_val1_call03"), g_dyn_call_val);

    /* Reset call counter. */
    g_dyn_call_count = 0;

    /* Create another field with no initial value. */
    rc = ib_field_create(&cdynf, m_pool, "test_cdynf", IB_FTYPE_NULSTR, NULL);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(cdynf);
    ASSERT_EQ(10UL, cdynf->nlen);
    ASSERT_EQ(0, memcmp("test_cdynf", cdynf->name, 10));

    /* Make it a dynamic field which calls dyn_get_cached() with "cdynf" as the data. */
    ib_field_dyn_register_get(cdynf, dyn_get_cached, (void *)"cdynf_get");

    /* Get the value from the dynamic field. */
    fval = ib_field_value_nulstr_ex(cdynf, (void *)"fetch1", 6);
    ASSERT_TRUE(fval);
    ASSERT_EQ(
        std::string("testval_cdynf_get_fetch1_call01"),
        fval
    );

    /* Get the value from the dynamic field again. */
    fval = ib_field_value_nulstr_ex(cdynf, NULL, 0);
    ASSERT_TRUE(fval);
    ASSERT_EQ(
        std::string("testval_cdynf_get_fetch1_call01"),
        fval
    );
}
