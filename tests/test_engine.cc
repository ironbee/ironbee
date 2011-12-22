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

#include "ibtest_util.c"

/// @test Test ironbee library - ib_engine_create()
TEST(TestIronBee, test_engine_create_null_plugin)
{
    ib_engine_t *ib;
    ib_status_t rc;

    atexit(ib_shutdown);
    rc = ib_initialize();
    ASSERT_TRUE(rc == IB_OK) << "ib_initialize() failed - rc != IB_OK";

    rc = ib_engine_create(&ib, NULL);
    ASSERT_EQ(IB_EINVAL, ib_engine_create(&ib, NULL));
    ASSERT_FALSE(ib);
}

/// @test Test ironbee library - ib_engine_create() and ib_engine_destroy()
TEST(TestIronBee, test_engine_create_and_destroy)
{
    ib_engine_t *ib;

    ibtest_engine_create(&ib);
    ibtest_engine_destroy(ib);
}

/// @test Test ironbee library - test configuration
TEST(TestIronBee, test_engine_config_basic)
{
    ib_engine_t *ib;
    const char *cfgbuf =
        "#DebugLog /tmp/ironbee-debug.log\n"
        "DebugLogLevel 9\n"
        "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
        "SensorName UnitTesting\n"
        "SensorHostname unit-testing.sensor.tld\n"
        "LoadModule ibmod_htp.so\n"
        "<Site *>\n"
        "  Hostname *\n"
        "</Site>\n";

    ibtest_engine_create(&ib);
    ibtest_engine_config_buf(ib, (uint8_t *)cfgbuf, strlen(cfgbuf));
    ibtest_engine_destroy(ib);
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
    size_t dlen_in;
    size_t dlen_out;
    ib_flags_t flags;
    uint8_t data_in[128];
    uint8_t data_out[128];
    ib_status_t rc;

    ibtest_engine_create(&ib);

    ibtest_engine_create(&ib);

    ASSERT_EQ(IB_OK, ib_tfn_create(ib, "foo2bar", foo2bar, NULL, NULL));
    ASSERT_EQ(IB_OK, ib_tfn_lookup(ib, "foo2bar", &tfn));
    ASSERT_NE((ib_tfn_t *)-1, tfn);
    ASSERT_TRUE(tfn);

    memcpy(data_in, "foo", 4);
    dlen_in = 3;

    ASSERT_EQ(
        IB_OK,
        ib_tfn_transform(tfn, ib->mp, data_in, dlen_in,
                         (uint8_t **)&data_out, &dlen_out, &flags)
    );
    ASSERT_NE((ib_tfn_t *)-1, tfn);
    ASSERT_TRUE(IB_TFN_CHECK_FMODIFIED(flags));
    ASSERT_TRUE(IB_TFN_CHECK_FINPLACE(flags));

    ibtest_engine_destroy(ib);
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

    ibtest_engine_create(&ib);

    ASSERT_EQ(
        IB_OK,
        ib_provider_instance_create(ib,
                                    IB_PROVIDER_TYPE_DATA,
                                    IB_DSTR_CORE,
                                    &dpi,
                                    ib_engine_pool_main_get(ib),
                                    NULL)
    );
    ASSERT_TRUE(dpi);

    /* Create a field with no initial value. */
    ASSERT_EQ(
        IB_OK,
        ib_field_create(&dynf, ib_engine_pool_main_get(ib),
                        "test_dynf", IB_FTYPE_GENERIC, NULL)
    );
    ASSERT_TRUE(dynf);
    ASSERT_EQ(9, dynf->nlen);
    ASSERT_MEMEQ("test_dynf", dynf->name, 9);

    /* Make it a dynamic field which calls dyn_get() with "dynf" as the data. */
    ib_field_dyn_register_get(dynf, (ib_field_get_fn_t)dyn_get);
    ib_field_dyn_set_data(dynf, (void *)ib_engine_pool_main_get(ib));

    /* Add the field to the data store. */
    ASSERT_EQ(IB_OK, ib_data_add(dpi, dynf));

    /* Fetch the field from the data store */
    ASSERT_EQ(IB_OK, ib_data_get(dpi, "test_dynf", &f));
    ASSERT_TRUE(f);
    ASSERT_EQ(dynf, f);

    /* Fetch a dynamic field from the data store */
    ASSERT_EQ(IB_OK, ib_data_get(dpi, "test_dynf.dyn_subkey", &f));
    ASSERT_TRUE(f);
    ASSERT_EQ(10, f->nlen);

    /* Get the value from the dynamic field. */
    ASSERT_EQ(5, *ib_field_value_num(f));

    /* Fetch another dynamic field from the data store */
    ASSERT_EQ(IB_OK, ib_data_get(dpi, "test_dynf.dyn_subkey2", &f));
    ASSERT_TRUE(f);
    ASSERT_EQ(11, f->nlen);
    ASSERT_MEMEQ("dyn_subkey2", f->name, 11);

    /* Get the value from the dynamic field. */
    ASSERT_EQ(5, *ib_field_value_num(f));

    ibtest_engine_destroy(ib);
}
