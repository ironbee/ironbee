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

#include <ironbee/field.h>
#include <ironbee/bytestr.h>
#include <ironbee/transformation.h>

#include "config-parser.h"
#include "ibtest_util.hh"

/// @test Test ironbee library - ib_engine_create()
TEST(TestIronBee, test_engine_create_null_plugin)
{
    ib_engine_t *ib;
    ib_status_t rc;

    atexit(ib_shutdown);
    rc = ib_initialize();
    ASSERT_EQ(IB_OK, rc);

    rc = ib_engine_create(&ib, NULL);
    ASSERT_EQ(IB_EINVAL, rc);
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
        "ModuleBasePath " IB_XSTRINGIFY(MODULE_BASE_PATH) "\n"
        "RuleBasePath " IB_XSTRINGIFY(RULE_BASE_PATH) "\n"
        "LoadModule ibmod_htp.so\n"
        "<Site *>\n"
        "  Hostname *\n"
        "</Site>\n";

    ibtest_engine_create(&ib);
    ibtest_engine_config_buf(ib, cfgbuf, strlen(cfgbuf));
    ibtest_engine_destroy(ib);
}

static ib_status_t foo2bar(ib_engine_t *ib,
                           ib_mpool_t *mp,
                           void *fndata,
                           ib_field_t *fin,
                           ib_field_t **fout,
                           ib_flags_t *pflags)
{
    if (fin->type == IB_FTYPE_BYTESTR) {
        ib_bytestr_t *ibs =
            const_cast<ib_bytestr_t *>(ib_field_value_bytestr(fin));
        char *data_in;
        size_t dlen_in;

        assert (ibs != NULL);

        data_in = (char *)ib_bytestr_ptr(ibs);
        dlen_in = ib_bytestr_length(ibs);

        if ( (data_in != NULL) &&
             (dlen_in == 3) &&
             (strncmp("foo", (char *)data_in, 3) == 0) )
        {
            *pflags = (IB_TFN_FMODIFIED | IB_TFN_FINPLACE);
            *(data_in+0) = 'b';
            *(data_in+1) = 'a';
            *(data_in+2) = 'r';
        }
        *fout = fin;
    }
    else if (fin->type == IB_FTYPE_NULSTR) {
        char *data = const_cast<char *>(ib_field_value_nulstr(fin));
        if ( (data != NULL) && (strncmp(data, "foo", 3) == 0) ) {
            *pflags = (IB_TFN_FMODIFIED | IB_TFN_FINPLACE);
            *(data+0) = 'b';
            *(data+1) = 'a';
            *(data+2) = 'r';
        }
        *fout = fin;
    }
    else {
        return IB_EINVAL;
    }

    return IB_OK;
}

/// @test Test ironbee library - transformation registration
TEST(TestIronBee, test_tfn)
{
    ib_engine_t *ib;
    ib_status_t rc;
    ib_tfn_t *tfn = (ib_tfn_t *)-1;
    ib_flags_t flags;
    uint8_t data_in[128];
    ib_field_t *fin;
    ib_field_t *fout;
    ib_bytestr_t *bs;

    ibtest_engine_create(&ib);

    ASSERT_EQ(IB_OK, ib_tfn_register(ib, "foo2bar", foo2bar, NULL));
    ASSERT_EQ(IB_OK, ib_tfn_lookup(ib, "foo2bar", &tfn));
    ASSERT_NE((ib_tfn_t *)-1, tfn);
    ASSERT_TRUE(tfn);

    ib_bytestr_dup_nulstr(&bs, ib->mp, "foo");
    fin = NULL;
    ib_field_create(&fin, ib->mp, "ByteStr", IB_FTYPE_BYTESTR, &bs);
    fout = NULL;
    flags = 0;
    rc = ib_tfn_transform(ib, ib->mp, tfn, fin, &fout, &flags);
    ASSERT_EQ(rc, IB_OK);
    ASSERT_NE((ib_tfn_t *)-1, tfn);
    ASSERT_TRUE(IB_TFN_CHECK_FMODIFIED(flags));
    ASSERT_TRUE(IB_TFN_CHECK_FINPLACE(flags));
    ASSERT_EQ(fin, fout);

    strcpy((char *)data_in, "foo");
    fin = NULL;
    void *p = &data_in;
    ib_field_create(&fin, ib->mp, "NulStr", IB_FTYPE_NULSTR, &p);
    fout = NULL;
    flags = 0;
    rc = ib_tfn_transform(ib, ib->mp, tfn, fin, &fout, &flags);
    ASSERT_EQ(rc, IB_OK);
    ASSERT_NE((ib_tfn_t *)-1, tfn);
    ASSERT_TRUE(IB_TFN_CHECK_FMODIFIED(flags));
    ASSERT_TRUE(IB_TFN_CHECK_FINPLACE(flags));
    ASSERT_EQ(fin, fout);

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
    ASSERT_EQ(9UL, dynf->nlen);
    ASSERT_MEMEQ("test_dynf", dynf->name, 9);

    /* Make it a dynamic field which calls dyn_get() with "dynf" as the data. */
    ib_field_dyn_register_get(dynf, (ib_field_get_fn_t)dyn_get, 
        (void *)ib_engine_pool_main_get(ib));

    /* Add the field to the data store. */
    ASSERT_EQ(IB_OK, ib_data_add(dpi, dynf));

    /* Fetch the field from the data store */
    ASSERT_EQ(IB_OK, ib_data_get(dpi, "test_dynf", &f));
    ASSERT_TRUE(f);
    ASSERT_EQ(dynf, f);

    /* Fetch a dynamic field from the data store */
    ASSERT_EQ(IB_OK, ib_data_get(dpi, "test_dynf:dyn_subkey", &f));
    ASSERT_TRUE(f);
    ASSERT_EQ(10UL, f->nlen);

    /* Get the value from the dynamic field. */
    ASSERT_EQ(5, *ib_field_value_num(f));

    /* Fetch another dynamic field from the data store */
    ASSERT_EQ(IB_OK, ib_data_get(dpi, "test_dynf:dyn_subkey2", &f));
    ASSERT_TRUE(f);
    ASSERT_EQ(11UL, f->nlen);
    ASSERT_MEMEQ("dyn_subkey2", f->name, 11);

    /* Get the value from the dynamic field. */
    ASSERT_EQ(5, *ib_field_value_num(f));

    ibtest_engine_destroy(ib);
}
