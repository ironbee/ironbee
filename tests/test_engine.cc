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
#include "engine/data.c"
#include "engine/tfn.c"
#include "engine/filter.c"
#include "engine/core.c"

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
                           ib_mpool_t *pool,
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
    rc = ib_tfn_transform(tfn, ib->mp, data_in, dlen_in, &data_out, &dlen_out, &flags);
    ASSERT_TRUE(tfn != (ib_tfn_t *)-1) << "ib_tfn_lookup() failed - unset";
    ASSERT_TRUE(IB_TFN_CHECK_FMODIFIED(flags)) << "ib_tfn_lookup() failed - not modified";
    ASSERT_TRUE(IB_TFN_CHECK_FINPLACE(flags)) << "ib_tfn_lookup() failed - not inplace";

    ib_engine_destroy(ib);
}

static ib_field_t *dyn_get(ib_field_t *f,
                           const char *arg,
                           size_t alen,
                           void *data)
{
    ib_mpool_t *mp = (ib_mpool_t *)data;
    ib_num_t numval = 5;
    ib_field_t *newf;
    ib_status_t rc;

    rc = ib_field_create_ex(&newf, mp, arg, alen, IB_FTYPE_NUM, &numval);
    if (rc != IB_OK) {
        return NULL;
    }

    return newf;
}

/// @test Test ironbee library - data provider
TEST(TestIronBee, test_dpi)
{
    ib_engine_t *ib;
    ib_provider_inst_t *dpi;
    ib_field_t *dynf;
    ib_field_t *f;
    ib_num_t *pnumval;
    ib_status_t rc;

    atexit(ib_shutdown);
    rc = ib_initialize();
    ASSERT_TRUE(rc == IB_OK) << "ib_initialize() failed - rc != IB_OK";

    rc = ib_engine_create(&ib, &ibplugin);
    ASSERT_TRUE(rc == IB_OK) << "ib_engine_create() failed - rc != IB_OK";
    ASSERT_TRUE(ib != NULL) << "ib_engine_create() failed - NULL";
    ASSERT_TRUE(ib->mp != NULL) << "ib_engine_create() - NULL mp";

    rc = ib_provider_instance_create(ib,
                                     IB_PROVIDER_TYPE_DATA,
                                     IB_DSTR_CORE,
                                     &dpi,
                                     ib_engine_pool_main_get(ib),
                                     NULL);
    ASSERT_TRUE(rc == IB_OK) << "ib_provider_instance_create() failed - rc != IB_OK";
    ASSERT_TRUE(dpi != NULL) << "ib_provider_instance_create() failed - NULL";

    /* Create a field with no initial value. */
    rc = ib_field_create(&dynf, ib_engine_pool_main_get(ib), "test_dynf", IB_FTYPE_GENERIC, NULL);
    ASSERT_TRUE(rc == IB_OK) << "ib_field_create() NULSTR failed - rc != IB_OK";
    ASSERT_TRUE(dynf != NULL) << "ib_field_create() NULSTR failed - NULL value";
    ASSERT_TRUE(dynf->nlen == 9) << "ib_field_create() NULSTR failed - incorrect nlen";
    ASSERT_TRUE(memcmp("test_dynf", dynf->name, 9) == 0) << "ib_field_create() NULSTR failed - wrong name";

    /* Make it a dynamic field which calls dyn_get() with "dynf" as the data. */
    ib_field_dyn_register_get(dynf, (ib_field_get_fn_t)dyn_get);
    ib_field_dyn_set_data(dynf, (void *)ib_engine_pool_main_get(ib));

    /* Add the field to the data store. */
    rc = ib_data_add(dpi, dynf);
    ASSERT_TRUE(rc == IB_OK) << "ib_data_add() failed - rc != IB_OK";

    /* Fetch the field from the data store */
    rc = ib_data_get(dpi, "test_dynf", &f);
    ASSERT_TRUE(rc == IB_OK) << "ib_data_get() failed - rc != IB_OK";
    ASSERT_TRUE(f != NULL) << "ib_data_get() failed - NULL value";
    ASSERT_TRUE(f == dynf) << "ib_data_get() failed - wrong field";

    /* Fetch a dynamic field from the data store */
    rc = ib_data_get(dpi, "test_dynf.dyn_subkey", &f);
    ASSERT_TRUE(rc == IB_OK) << "ib_data_get() dynamic failed - rc != IB_OK (" << rc << ")";
    ASSERT_TRUE(f != NULL) << "ib_data_get() dynamic failed - NULL value";
    ASSERT_TRUE((f->nlen == 10) && (memcmp("dyn_subkey", f->name, 10) == 0)) << "ib_data_get() dynamic failed - wrong field name";

    /* Get the value from the dynamic field. */
    pnumval = ib_field_value_num(f);
    ASSERT_TRUE(*pnumval == 5) << "bad dynamic field value: " << *pnumval;

    /* Fetch another dynamic field from the data store */
    rc = ib_data_get(dpi, "test_dynf.dyn_subkey2", &f);
    ASSERT_TRUE(rc == IB_OK) << "ib_data_get() dynamic2 failed - rc != IB_OK";
    ASSERT_TRUE(f != NULL) << "ib_data_get() dynamic2 failed - NULL value";
    ASSERT_TRUE((f->nlen == 11) && (memcmp("dyn_subkey2", f->name, 11) == 0)) << "ib_data_get() dynamic2 failed - wrong field name";

    /* Get the value from the dynamic field. */
    pnumval = ib_field_value_num(f);
    ASSERT_TRUE(*pnumval == 5) << "bad dynamic field value: " << *pnumval;

    ib_engine_destroy(ib);
}
