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
/// @brief IronBee --- Engine Test Functions
///
/// @author Brian Rectanus <brectanus@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"
#include "base_fixture.h"

#include <ironbee/field.h>
#include <ironbee/state_notify.h>
#include <ironbee/bytestr.h>
#include <ironbee/transformation.h>

#include "config-parser.h"
#include "ibtest_util.hpp"
#include "engine_private.h"

/// @test Test ironbee library - ib_engine_create()
TEST(TestIronBee, test_engine_create_null_server)
{
    ib_engine_t *ib = NULL;
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
        "AuditEngine Off\n"
        "LoadModule ibmod_htp.so\n"
        "<Site *>\n"
        "  Hostname *\n"
        "</Site>\n";

    ibtest_engine_create(&ib);
    ibtest_engine_config_buf(ib, cfgbuf, strlen(cfgbuf));
    ibtest_engine_destroy(ib);
}

static ib_status_t foo2bar(ib_mm_t mm,
                           const ib_field_t *fin,
                           const ib_field_t **fout,
                           void *fndata)
{
    ib_status_t rc = IB_OK;
    ib_field_t *fnew;

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
            data_out = (uint8_t *)ib_mm_alloc(mm, dlen_in);
            if (data_out == NULL) {
                return IB_EINVAL;
            }
            *(data_out+0) = 'b';
            *(data_out+1) = 'a';
            *(data_out+2) = 'r';
        }
        else {
            data_out = (uint8_t *)data_in;
        }
        rc = ib_field_create_bytestr_alias(&fnew, mm,
                                           fin->name, fin->nlen,
                                           data_out, dlen_in);
        if (rc == IB_OK) {
            *fout = fnew;
        }
    }
    else if (fin->type == IB_FTYPE_NULSTR) {
        const char *in;
        char *out;

        rc = ib_field_value(fin, ib_ftype_nulstr_out(&in));
        if (rc != IB_OK) {
            return rc;
        }
        if ( (in != NULL) && (strncmp(in, "foo", 3) == 0) ) {
            out = (char *)ib_mm_alloc(mm, strlen(in) + 1);
            if (out == NULL) {
                return IB_EINVAL;
            }

            *(out+0) = 'b';
            *(out+1) = 'a';
            *(out+2) = 'r';
            *(out+3) = '\0';
        }
        else {
            out = (char *)in;
        }
        rc = ib_field_create(&fnew, mm, fin->name, fin->nlen,
                             IB_FTYPE_NULSTR, ib_ftype_nulstr_in(out));
        if (rc == IB_OK) {
            *fout = fnew;
        }
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
    const ib_tfn_t *tfn = (ib_tfn_t *)-1;
    uint8_t data_in[128];
    ib_field_t *fin;
    const ib_field_t *fout;
    ib_bytestr_t *bs;

    ibtest_engine_create(&ib);

    ASSERT_EQ(IB_OK, ib_tfn_create_and_register(NULL, ib, "foo2bar", false,
                                     foo2bar, NULL));
    ASSERT_EQ(IB_OK, ib_tfn_lookup(ib, "foo2bar", &tfn));
    ASSERT_NE((ib_tfn_t *)-1, tfn);
    ASSERT_TRUE(tfn);

    ib_bytestr_dup_nulstr(&bs, ib_engine_mm_main_get(ib), "foo");
    fin = NULL;
    ib_field_create(
        &fin, ib_engine_mm_main_get(ib), IB_S2SL("ByteStr"),
        IB_FTYPE_BYTESTR, ib_ftype_bytestr_in(bs)
    );
    fout = NULL;
    rc = ib_tfn_execute(ib_engine_mm_main_get(ib), tfn, fin, &fout);
    ASSERT_EQ(rc, IB_OK);
    ASSERT_NE((ib_tfn_t *)-1, tfn);
    ASSERT_NE(fin, fout);

    strcpy((char *)data_in, "foo");
    fin = NULL;
    ib_field_create(
        &fin, ib_engine_mm_main_get(ib), IB_S2SL("NulStr"),
        IB_FTYPE_NULSTR,
        ib_ftype_nulstr_in((char *)data_in)
    );
    fout = NULL;
    rc = ib_tfn_execute(ib_engine_mm_main_get(ib), tfn, fin, &fout);
    ASSERT_EQ(rc, IB_OK);
    ASSERT_NE((ib_tfn_t *)-1, tfn);
    ASSERT_NE(fin, fout);

    ibtest_engine_destroy(ib);
}
