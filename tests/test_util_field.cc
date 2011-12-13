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

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

#define TESTING

#include "util/field.c"


/* -- Tests -- */

/// @test Test util field library - ib_field_create() ib_field_create_ex()
TEST(TestIBUtilField, test_field_create)
{
    ib_mpool_t *mp;
    ib_field_t *f;
    ib_status_t rc;
    const char *nulstrval = "TestValue";
    ib_num_t numval = 5;
    ib_bytestr_t *bytestrval;

    atexit(ib_shutdown);
    rc = ib_initialize();
    ASSERT_TRUE(rc == IB_OK) << "ib_initialize() failed - rc != IB_OK";
    rc = ib_mpool_create(&mp, NULL);
    ASSERT_TRUE(rc == IB_OK) << "ib_mpool_create() failed - rc != IB_OK";

    rc = ib_field_create(&f, mp, "test_nulstr", IB_FTYPE_NULSTR, &nulstrval);
    ASSERT_TRUE(rc == IB_OK) << "ib_field_create() NULSTR failed - rc != IB_OK";
    ASSERT_TRUE(f != NULL) << "ib_field_create() NULSTR failed - NULL value";
    ASSERT_TRUE(f->nlen == 11) << "ib_field_create() NULSTR failed - incorrect nlen";
    ASSERT_TRUE(memcmp("test_nulstr", f->name, 11) == 0) << "ib_field_create() NULSTR failed - wrong name";

    rc = ib_field_create(&f, mp, "test_num", IB_FTYPE_NUM, &numval);
    ASSERT_TRUE(rc == IB_OK) << "ib_field_create() NUM failed - rc != IB_OK";
    ASSERT_TRUE(f != NULL) << "ib_field_create() NUM failed - NULL value";
    ASSERT_TRUE(f->nlen == 8) << "ib_field_create() NUM failed - incorrect nlen";
    ASSERT_TRUE(memcmp("test_num", f->name, 8) == 0) << "ib_field_create() NUM failed - wrong name";

    rc = ib_bytestr_dup_mem(&bytestrval, mp, (uint8_t *)nulstrval, strlen(nulstrval));
    ASSERT_TRUE(rc == IB_OK) << "ib_bytestr_dup_mem() failed - rc != IB_OK";
    ASSERT_TRUE(f != NULL) << "ib_bytestr_dup_mem() failed - NULL value";

    rc = ib_field_create(&f, mp, "test_bytestr", IB_FTYPE_BYTESTR, &bytestrval);
    ASSERT_TRUE(rc == IB_OK) << "ib_field_create() BYTESTR failed - rc != IB_OK";
    ASSERT_TRUE(f != NULL) << "ib_field_create() BYTESTR failed - NULL value";
    ASSERT_TRUE(f->nlen == 12) << "ib_field_create() BYTESTR failed - incorrect nlen";
    ASSERT_TRUE(memcmp("test_bytestr", f->name, 12) == 0) << "ib_field_create() BYTESTR failed - wrong name";

    rc = ib_field_create_ex(&f, mp, "test_nulstr_ex", 14, IB_FTYPE_NULSTR, &nulstrval);
    ASSERT_TRUE(rc == IB_OK) << "ib_field_create_ex() NULSTR failed - rc != IB_OK";
    ASSERT_TRUE(f != NULL) << "ib_field_create_ex() NULSTR failed - NULL value";

    rc = ib_field_create_ex(&f, mp, "test_num_ex", 11, IB_FTYPE_NUM, &numval);
    ASSERT_TRUE(rc == IB_OK) << "ib_field_create_ex() NUM failed - rc != IB_OK";
    ASSERT_TRUE(f != NULL) << "ib_field_create_ex() NUM failed - NULL value";

    rc = ib_field_create_ex(&f, mp, "test_bytestr_ex", 15, IB_FTYPE_BYTESTR, &bytestrval);
    ASSERT_TRUE(rc == IB_OK) << "ib_field_create() BYTESTR failed - rc != IB_OK";
    ASSERT_TRUE(f != NULL) << "ib_field_create() BYTESTR failed - NULL value";

    ib_mpool_destroy(mp);
}

// Globals used to test if dyn_get caching is working
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
    ib_field_setv(f, &cval);

    return (void *)cval;
}

///@test Test util field library - ib_field_dyn_register_get()
TEST(TestIBUtilField, test_dyn_field)
{
    ib_mpool_t *mp;
    ib_field_t *dynf;
    ib_field_t *cdynf;
    ib_status_t rc;
    const char *fval;

    atexit(ib_shutdown);
    rc = ib_initialize();
    ASSERT_TRUE(rc == IB_OK) << "ib_initialize() failed - rc != IB_OK";
    rc = ib_mpool_create(&mp, NULL);
    ASSERT_TRUE(rc == IB_OK) << "ib_mpool_create() failed - rc != IB_OK";

    /* Create a field with no initial value. */
    rc = ib_field_create(&dynf, mp, "test_dynf", IB_FTYPE_NULSTR, NULL);
    ASSERT_TRUE(rc == IB_OK) << "ib_field_create() NULSTR failed - rc != IB_OK";
    ASSERT_TRUE(dynf != NULL) << "ib_field_create() NULSTR failed - NULL value";
    ASSERT_TRUE(dynf->nlen == 9) << "ib_field_create() NULSTR failed - incorrect nlen";
    ASSERT_TRUE(memcmp("test_dynf", dynf->name, 9) == 0) << "ib_field_create() NULSTR failed - wrong name";

    /* Make it a dynamic field which calls dyn_get() with "dynf" as the data. */
    ib_field_dyn_register_get(dynf, dyn_get);
    ib_field_dyn_set_data(dynf, (void *)"dynf");

    /* Get the value from the dynamic field. */
    fval = ib_field_value_nulstr_ex(dynf, (void *)"fetch1", 6);
    ASSERT_TRUE((fval != NULL) && (strcmp("testval_dynf_fetch1_call01", fval) == 0)) << "bad dynamic field value: " << fval;

    /* Get the value from the dynamic field again. */
    fval = ib_field_value_nulstr_ex(dynf, (void *)"fetch2", 6);
    ASSERT_TRUE((fval != NULL) && (strcmp("testval_dynf_fetch2_call02", fval) == 0)) << "bad dynamic field value - not incremented: " << fval;

    /* Reset call counter. */
    g_dyn_call_count = 0;

    /* Create another field with no initial value. */
    rc = ib_field_create(&cdynf, mp, "test_cdynf", IB_FTYPE_NULSTR, NULL);
    ASSERT_TRUE(rc == IB_OK) << "ib_field_create() NULSTR failed - rc != IB_OK";
    ASSERT_TRUE(cdynf != NULL) << "ib_field_create() NULSTR failed - NULL value";
    ASSERT_TRUE(cdynf->nlen == 10) << "ib_field_create() NULSTR failed - incorrect nlen";
    ASSERT_TRUE(memcmp("test_cdynf", cdynf->name, 10) == 0) << "ib_field_create() NULSTR failed - wrong name";

    /* Make it a dynamic field which calls dyn_get_cached() with "cdynf" as the data. */
    ib_field_dyn_register_get(cdynf, dyn_get_cached);
    ib_field_dyn_set_data(cdynf, (void *)"cdynf");

    /* Get the value from the dynamic field. */
    fval = ib_field_value_nulstr_ex(cdynf, (void *)"fetch1", 6);
    ASSERT_TRUE((fval != NULL) && (strcmp("testval_cdynf_fetch1_call01", fval) == 0)) << "bad dynamic field value: " << fval;

    /* Get the value from the dynamic field again. */
    fval = ib_field_value_nulstr_ex(cdynf, (void *)"fetch2", 6);
    ASSERT_TRUE((fval != NULL) && (strcmp("testval_cdynf_fetch1_call01", fval) == 0)) << "bad dynamic field value - not cached: " << fval;

    ib_mpool_destroy(mp);
}
