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
/// @brief IronBee &mdash; Engine Test Functions
/// 
/// @author Brian Rectanus <brectanus@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"
#include "base_fixture.h"

#include <ironbee/field.h>
#include <ironbee/state_notify.h>
#include <ironbee/bytestr.h>
#include <ironbee/transformation.h>
#include <ironbee/debug.h>
#include <ironbee/provider.h>

#include "config-parser.h"
#include "ibtest_util.hh"
#include "engine_private.h"

/// @test Test ironbee library - ib_engine_create()
TEST(TestIronBee, test_engine_create_null_server)
{
    ib_engine_t *ib;
    ib_status_t rc;

    rc = ib_initialize();
    ASSERT_EQ(IB_OK, rc);

    rc = ib_engine_create(&ib, NULL);
    ASSERT_EQ(IB_EINVAL, rc);
    ASSERT_FALSE(ib);
    
    ib_shutdown();
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
        "#Log /tmp/ironbee-debug.log\n"
        "LogLevel 9\n"
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
    ibtest_engine_config_buf(ib, cfgbuf, strlen(cfgbuf), "test.conf", 1);
    ibtest_engine_destroy(ib);
}

static ib_status_t foo2bar(ib_engine_t *ib,
                           ib_mpool_t *mp,
                           void *fndata,
                           const ib_field_t *fin,
                           ib_field_t **fout,
                           ib_flags_t *pflags)
{
    ib_status_t rc = IB_OK;

    if (fin->type == IB_FTYPE_BYTESTR) {
        const ib_bytestr_t *ibs;
        rc = ib_field_value(fin, ib_ftype_bytestr_out(&ibs));
        if (rc != IB_OK) {
            return rc;
        }
        
        const uint8_t *data_in;
        size_t dlen_in;
        uint8_t *data_out;

        assert (ibs != NULL);

        data_in = ib_bytestr_const_ptr(ibs);
        dlen_in = ib_bytestr_length(ibs);

        if ( (data_in != NULL) &&
             (dlen_in == 3) &&
             (strncmp("foo", (char *)data_in, 3) == 0) )
        {
            data_out = (uint8_t *)ib_mpool_alloc(mp, dlen_in);
            if (data_out == NULL) {
                return IB_EINVAL;
            }
            *pflags = (IB_TFN_FMODIFIED);
            *(data_out+0) = 'b';
            *(data_out+1) = 'a';
            *(data_out+2) = 'r';
        }
        else {
            data_out = (uint8_t *)data_in;
        }
        rc = ib_field_create_bytestr_alias(fout, mp,
                                           fin->name, fin->nlen,
                                           data_out, dlen_in);
    }
    else if (fin->type == IB_FTYPE_NULSTR) {
        const char *in;
        char *out;

        rc = ib_field_value(fin, ib_ftype_nulstr_out(&in));
        if (rc != IB_OK) {
            return rc;
        }
        if ( (in != NULL) && (strncmp(in, "foo", 3) == 0) ) {
            out = (char *)ib_mpool_alloc(mp, strlen(in));
            if (out == NULL) {
                return IB_EINVAL;
            }

            *pflags = (IB_TFN_FMODIFIED);
            *(out+0) = 'b';
            *(out+1) = 'a';
            *(out+2) = 'r';
        }
        else {
            out = (char *)in;
        }
        rc = ib_field_create(fout, mp, fin->name, fin->nlen,
                             IB_FTYPE_NULSTR, ib_ftype_nulstr_in(out));
    }
    else {
        return IB_EINVAL;
    }

    return rc;
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
    ib_field_create(
        &fin, ib->mp, IB_FIELD_NAME("ByteStr"), 
        IB_FTYPE_BYTESTR, ib_ftype_bytestr_in(bs)
    );
    fout = NULL;
    flags = 0;
    rc = ib_tfn_transform(ib, ib->mp, tfn, fin, &fout, &flags);
    ASSERT_EQ(rc, IB_OK);
    ASSERT_NE((ib_tfn_t *)-1, tfn);
    ASSERT_TRUE(IB_TFN_CHECK_FMODIFIED(flags));
    ASSERT_NE(fin, fout);

    strcpy((char *)data_in, "foo");
    fin = NULL;
    ib_field_create(
        &fin, ib->mp, IB_FIELD_NAME("NulStr"), 
        IB_FTYPE_NULSTR, 
        ib_ftype_nulstr_in((char *)data_in)
    );
    fout = NULL;
    flags = 0;
    rc = ib_tfn_transform(ib, ib->mp, tfn, fin, &fout, &flags);
    ASSERT_EQ(rc, IB_OK);
    ASSERT_NE((ib_tfn_t *)-1, tfn);
    ASSERT_TRUE(IB_TFN_CHECK_FMODIFIED(flags));
    ASSERT_NE(fin, fout);

    ibtest_engine_destroy(ib);
}

static ib_status_t dyn_get(
    const ib_field_t *f,
    void *out_value,
    const void *arg,
    size_t alen,
    void *data
)
{
    ib_mpool_t *mp = (ib_mpool_t *)data;
    ib_num_t numval = 5;
    ib_field_t *newf;
    ib_status_t rc;
    
    const char* carg = (const char *)arg;

    rc = ib_field_create(&newf, mp, carg, alen, IB_FTYPE_NUM, 
        ib_ftype_num_in(&numval));
    if (rc != IB_OK) {
        return rc;
    }

    *(void**)out_value = newf;
    
    return IB_OK;
}

/// @test Test ironbee library - data provider
TEST(TestIronBee, test_dpi_dynf)
{
    ib_engine_t *ib;
    ib_provider_inst_t *dpi;
    ib_field_t *dynf;
    ib_field_t *f;
    ib_status_t rc;
    ib_num_t n;

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
        ib_field_create_dynamic(
            &dynf, 
            ib_engine_pool_main_get(ib),
            IB_FIELD_NAME("test_dynf"), 
            IB_FTYPE_GENERIC, 
            dyn_get, (void *)ib_engine_pool_main_get(ib),
            NULL, NULL
        )
    );
    ASSERT_TRUE(dynf);
    ASSERT_EQ(9UL, dynf->nlen);
    ASSERT_MEMEQ("test_dynf", dynf->name, 9);

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
    rc = ib_field_value(f, ib_ftype_num_out(&n));
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(5, n);

    /* Fetch another dynamic field from the data store */
    ASSERT_EQ(IB_OK, ib_data_get(dpi, "test_dynf:dyn_subkey2", &f));
    ASSERT_TRUE(f);
    ASSERT_EQ(11UL, f->nlen);
    ASSERT_MEMEQ("dyn_subkey2", f->name, 11);

    /* Get the value from the dynamic field. */
    rc = ib_field_value(f, ib_ftype_num_out(&n));
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(5, n);

    ibtest_engine_destroy(ib);
}

TEST(TestIronBee, test_dpi_name)
{
    ib_engine_t *ib = NULL;
    ib_provider_inst_t *dpi = NULL;
    ib_field_t *list_field = NULL;
    ib_field_t *out_field = NULL;

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

    ASSERT_IB_OK(ib_data_add_list(dpi, "ARGV", &list_field));
    ASSERT_IB_OK(ib_data_get(dpi, "ARGV", &out_field));
    IB_PROVIDER_API_TYPE(data) *api =
        (IB_PROVIDER_API_TYPE(data) *)dpi->pr->api;

    ASSERT_IB_OK(api->get(dpi, "ARGV", 4, &out_field));
    ASSERT_TRUE(out_field);
    out_field = NULL;
    ASSERT_IB_OK(api->get(dpi, "ARGV:/.*(1|3)/", 4, &out_field));
    ASSERT_TRUE(out_field);
    ibtest_engine_destroy(ib);
}

// Test pattern matching a field.
TEST(TestIronBee, test_dpi_pcre)
{
    ib_engine_t *ib;
    ib_provider_inst_t *dpi;
    ib_field_t *list_field;
    ib_field_t *out_field;
    ib_list_t *list;
    ib_list_t *out_list;
    ib_field_t *field1;
    ib_field_t *field2;
    ib_field_t *field3;
    ib_num_t num1 = 1;
    ib_num_t num2 = 2;
    ib_num_t num3 = 3;

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

    ASSERT_IB_OK(
        ib_field_create(&field1, dpi->mp, "field1", 6, IB_FTYPE_NUM, &num1));
    ASSERT_IB_OK(
        ib_field_create(&field2, dpi->mp, "field2", 6, IB_FTYPE_NUM, &num2));
    ASSERT_IB_OK(
        ib_field_create(&field3, dpi->mp, "field3", 6, IB_FTYPE_NUM, &num3));
    ASSERT_IB_OK(ib_data_add_list(dpi, "ARGV", &list_field));
    ASSERT_IB_OK(ib_data_get(dpi, "ARGV", &out_field));

    ASSERT_IB_OK(ib_field_value(list_field, &list));
    ASSERT_IB_OK(ib_list_push(list, field1));
    ASSERT_IB_OK(ib_list_push(list, field2));
    ASSERT_IB_OK(ib_list_push(list, field3));

    ASSERT_IB_OK(ib_data_get(dpi, "ARGV:/.*(1|3)/", &out_field));

    ASSERT_IB_OK(ib_field_value(out_field, &out_list));
    ASSERT_NE(list, out_list); /* Make sure it's a different list. */

    ASSERT_EQ(2U, IB_LIST_ELEMENTS(out_list));

    out_field = (ib_field_t *) IB_LIST_FIRST(out_list)->data;
    ASSERT_FALSE(memcmp(out_field->name, field1->name, field1->nlen));

    out_field = (ib_field_t *) IB_LIST_LAST(out_list)->data;
    ASSERT_FALSE(memcmp(out_field->name, field3->name, field3->nlen));

    ibtest_engine_destroy(ib);
}
