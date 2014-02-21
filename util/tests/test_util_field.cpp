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
/// @brief IronBee --- Field Test Functions
///
/// @author Brian Rectanus <brectanus@qualys.com>
/// @author Christopher Alfeld <calfeld@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "ironbee_config_auto.h"

#include "gtest/gtest.h"
#include "simple_fixture.hpp"

#include <ironbee/field.h>
#include <ironbee/string.h>
#include <ironbee/util.h>
#include <ironbee/mpool.h>
#include <ironbee/bytestr.h>

#include <stdexcept>

class TestIBUtilField : public SimpleFixture
{
public:
    TestIBUtilField()
    {
        ib_util_initialize();
    }

    ~TestIBUtilField()
    {
        ib_util_shutdown();
    }
};

/* -- Tests -- */

/// @test Test util field library - ib_field_create() ib_field_create()
TEST_F(TestIBUtilField, test_field_create)
{
    ib_field_t *f;
    ib_status_t rc;
    const char *nulstrval = "TestValue";
    ib_num_t numval = 5;
    ib_bytestr_t *bytestrval;
    const char *nulout;
    const char *nulcopy;

    nulcopy = MemPoolStrDup(nulstrval);
    ASSERT_STRNE(NULL, nulcopy);
    rc = ib_field_create(&f, MemPool(), IB_S2SL("test_nulstr"),
                         IB_FTYPE_NULSTR, ib_ftype_nulstr_in(nulcopy));
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(f);
    ASSERT_EQ(11UL, f->nlen);
    ASSERT_EQ(0, memcmp("test_nulstr", f->name, 11));

    rc = ib_field_value(f, ib_ftype_nulstr_out(&nulout));
    ASSERT_EQ(IB_OK, rc);
    ASSERT_STREQ(nulstrval, nulout);

    rc = ib_field_create(&f, MemPool(), IB_S2SL("test_num"),
                         IB_FTYPE_NUM, ib_ftype_num_in(&numval));
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(f);
    ASSERT_EQ(8UL, f->nlen);
    ASSERT_EQ(0, memcmp("test_num", f->name, 8));

    rc = ib_bytestr_dup_mem(&bytestrval, MemPool(),
                            (uint8_t *)nulstrval, strlen(nulstrval));
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(f);

    rc = ib_field_create(&f, MemPool(), IB_S2SL("test_bytestr"),
                         IB_FTYPE_BYTESTR, ib_ftype_bytestr_in(bytestrval));
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(f);
    ASSERT_EQ(12UL, f->nlen);
    ASSERT_EQ(0, memcmp("test_bytestr", f->name, 12));

    rc = ib_field_create(&f, MemPool(), IB_S2SL("test_nulstr_ex"),
                         IB_FTYPE_NULSTR, ib_ftype_nulstr_in(nulstrval));
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(f);

    rc = ib_field_create(&f, MemPool(), IB_S2SL("test_num_ex"),
                         IB_FTYPE_NUM, ib_ftype_num_in(&numval));
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(f);

    rc = ib_field_create(&f, MemPool(), IB_S2SL("test_bytestr_ex"),
                         IB_FTYPE_BYTESTR, ib_ftype_bytestr_in(bytestrval));
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(f);
}

/// @test Test util field library - ib_field_from_string()
TEST_F(TestIBUtilField, test_field_from_string)
{
    ib_field_t *f;
    ib_status_t rc;
    ib_num_t fnum;
    ib_float_t ffloat;
    const char *fnulstr;

    rc = ib_field_from_string(MemPool(), IB_S2SL("test_num"),
                              "11", &f);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(f);
    ASSERT_EQ(IB_FTYPE_NUM, f->type);
    ASSERT_EQ(IB_OK, ib_field_value(f, ib_ftype_num_out(&fnum)));
    ASSERT_EQ(11, fnum);

    rc = ib_field_from_string(MemPool(), IB_S2SL("test_num"),
                              "-11", &f);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(f);
    ASSERT_EQ(IB_FTYPE_NUM, f->type);
    ASSERT_EQ(IB_OK, ib_field_value(f, ib_ftype_num_out(&fnum)));
    ASSERT_EQ(-11, fnum);

    rc = ib_field_from_string(MemPool(), IB_S2SL("test_float"),
                              "1.0", &f);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(f);
    ASSERT_EQ(IB_FTYPE_FLOAT, f->type);
    ASSERT_EQ(IB_OK, ib_field_value(f, ib_ftype_float_out(&ffloat)));
    ASSERT_EQ(1.0, ffloat);

    rc = ib_field_from_string(MemPool(), IB_S2SL("test_num"),
                              "-1.0", &f);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(f);
    ASSERT_EQ(IB_FTYPE_FLOAT, f->type);
    ASSERT_EQ(IB_OK, ib_field_value(f, ib_ftype_float_out(&ffloat)));
    ASSERT_EQ(-1.0, ffloat);

    rc = ib_field_from_string(MemPool(), IB_S2SL("test_str"),
                              "x", &f);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(f);
    ASSERT_EQ(IB_FTYPE_NULSTR, f->type);
    ASSERT_EQ(IB_OK, ib_field_value(f, ib_ftype_nulstr_out(&fnulstr)));
    ASSERT_STREQ("x", fnulstr);

    rc = ib_field_from_string(MemPool(), IB_S2SL("test_str"),
                              "-1.1x", &f);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(f);
    ASSERT_EQ(IB_FTYPE_NULSTR, f->type);
    ASSERT_EQ(IB_OK, ib_field_value(f, ib_ftype_nulstr_out(&fnulstr)));
    ASSERT_STREQ("-1.1x", fnulstr);
}

// Globals used to test if dyn_* caching is working
static int g_dyn_call_count;

static char g_dyn_call_val[1024];

// Dynamic get function which increments a global counter and modifies
// a global buffer so that the number of calls can be tracked.  One of the
// tests is to determine if the function was called only once (result
// cached).
static ib_status_t dyn_get(
    const ib_field_t *f,
    void *out_value,
    const void *arg,
    size_t alen,
    void *data
)
{
    /* Keep track of how many times this was called */
    ++g_dyn_call_count;

    snprintf(g_dyn_call_val, sizeof(g_dyn_call_val),
             "testval_%s_%.*s_call%02d",
             (const char *)data,
             (int)alen,
             (const char *)arg,
             g_dyn_call_count);

    *reinterpret_cast<const char**>(out_value) = g_dyn_call_val;

    return IB_OK;
}

// Cached version of the above dyn_get function.
static ib_status_t dyn_get_cached(
    const ib_field_t *f,
    void *out_value,
    const void *arg,
    size_t alen,
    void *data
)
{
    /* Call the get function */
    const char* cval;
    dyn_get(f, &cval, arg, alen, data);

    /* Cache the value */
    /* Caching does not semantically change value, so we can safely ignore
     * the constness of f. */
    ib_field_make_static((ib_field_t *)f);
    ib_field_setv((ib_field_t *)f, ib_ftype_nulstr_in(cval));

    *reinterpret_cast<const char**>(out_value) = cval;

    return IB_OK;
}

static ib_status_t dyn_set(
    ib_field_t *field,
    const void *arg, size_t alen,
    void *val,
    void *data
)
{
    ++g_dyn_call_count;

    snprintf(g_dyn_call_val, sizeof(g_dyn_call_val),
             "testval_%s_%.*s_%s_call%02d",
             (const char *)data, (int)alen,
             (const char *)arg, (const char *)val, g_dyn_call_count);

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
    rc = ib_field_create_dynamic(
        &dynf, MemPool(),
        IB_S2SL("test_dynf"), IB_FTYPE_NULSTR,
        dyn_get, (void *)"dynf_get",
        dyn_set, (void *)"dynf_set"
    );
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(dynf);
    ASSERT_EQ(9UL, dynf->nlen);
    ASSERT_EQ(0, memcmp("test_dynf", dynf->name, 9));

    /* Get the value from the dynamic field. */
    rc = ib_field_value_ex(dynf,
        ib_ftype_nulstr_out(&fval),
        (void *)"fetch1", 6
    );
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(fval);
    ASSERT_EQ(
        std::string("testval_dynf_get_fetch1_call01"),
        fval
    );

    /* Get the value from the dynamic field again. */
    rc = ib_field_value_ex(dynf,
        ib_ftype_nulstr_out(&fval),
        (void *)"fetch2", 6
    );
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(fval);
    ASSERT_EQ(
        std::string("testval_dynf_get_fetch2_call02"),
        fval
    );

    /* Set */
    rc = ib_field_setv_ex(dynf, (void *)"val1", (void *)"set1", 4);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(std::string("testval_dynf_set_set1_val1_call03"), g_dyn_call_val);

    /* Reset call counter. */
    g_dyn_call_count = 0;

    /* Create another field with no initial value. */
    rc = ib_field_create_dynamic(
        &cdynf, MemPool(),
        IB_S2SL("test_cdynf"), IB_FTYPE_NULSTR,
        dyn_get_cached, (void *)("cdynf_get"),
        dyn_set, NULL
    );
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(cdynf);
    ASSERT_EQ(10UL, cdynf->nlen);
    ASSERT_EQ(0, memcmp("test_cdynf", cdynf->name, 10));

    /* Get the value from the dynamic field. */
    rc = ib_field_value_ex(cdynf,
        ib_ftype_nulstr_out(&fval),
        (void *)"fetch1", 6
    );
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(fval);
    ASSERT_EQ(
        std::string("testval_cdynf_get_fetch1_call01"),
        fval
    );

    /* Get the value from the dynamic field again. */
    rc = ib_field_value_ex(cdynf,
        ib_ftype_nulstr_out(&fval),
        NULL, 0
    );
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(fval);
    ASSERT_EQ(
        std::string("testval_cdynf_get_fetch1_call01"),
        fval
    );
}

TEST_F(TestIBUtilField, Alias)
{
    ib_num_t   num1;
    ib_num_t   num2;
    ib_float_t flt1;
    ib_float_t flt2;
    char *s = NULL;
    const char *v;
    ib_field_t *f;
    ib_status_t rc;

    rc = ib_field_create_alias(&f, MemPool(), "foo", 3,
                               IB_FTYPE_NULSTR,
                               ib_ftype_nulstr_mutable_out(&s));
    ASSERT_EQ(IB_OK, rc);
    v = "hello";
    rc = ib_field_setv(f, ib_ftype_nulstr_in(v));
    ASSERT_EQ(IB_OK, rc);
    ASSERT_STREQ(v, s);

    /*
     * Alias a numeric field
     */
    num1 = 1;
    rc = ib_field_create_alias(&f, MemPool(), "num", 3,
                               IB_FTYPE_NUM,
                               ib_ftype_num_in(&num1));
    ASSERT_EQ(IB_OK, rc);

    rc = ib_field_value(f, ib_ftype_num_out(&num2));
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(num1, num2);

    num1 = 3;
    rc = ib_field_value(f, ib_ftype_num_out(&num2));
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(num1, num2);


    /*
     * Alias a floating point field
     */
    flt1 = 1.1;
    rc = ib_field_create_alias(&f, MemPool(), "flt", 3,
                               IB_FTYPE_FLOAT,
                               ib_ftype_float_in(&flt1));
    ASSERT_EQ(IB_OK, rc);

    rc = ib_field_value(f, ib_ftype_float_out(&flt2));
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(flt1, flt2);

    flt1 = 1.5;
    rc = ib_field_value(f, ib_ftype_float_out(&flt2));
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(flt1, flt2);
}

TEST_F(TestIBUtilField, ConvertString)
{
    ib_field_t *f1;
    ib_field_t *f2;
    ib_status_t rc;
    ib_num_t    num;
    ib_float_t  flt;

    /*
     * Convert numeric string to number
     */

    /* Create the field */
    rc = ib_field_create(&f1, MemPool(), "one", 3,
                         IB_FTYPE_NULSTR, ib_ftype_nulstr_in("1"));
    ASSERT_EQ(IB_OK, rc);

    /* Attempt a numeric conversion. */
    rc = ib_field_convert(MemPool(), IB_FTYPE_NUM, f1, &f2);
    ASSERT_EQ(IB_OK, rc);

    /* Pull out param value for check. */
    rc = ib_field_value(f2, ib_ftype_num_out(&num));
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(1, num);

    /*
     * Convert floating-point string to float
     */

    /* Create the field */
    rc = ib_field_create(&f1, MemPool(), "one", 3,
                         IB_FTYPE_NULSTR, ib_ftype_nulstr_in("1.1"));
    ASSERT_EQ(IB_OK, rc);

    /* Attempt a numeric conversion. */
    rc = ib_field_convert(MemPool(), IB_FTYPE_FLOAT, f1, &f2);
    ASSERT_EQ(IB_OK, rc);

    /* Pull out param value for check. */
    rc = ib_field_value(f2, ib_ftype_float_out(&flt));
    ASSERT_EQ(IB_OK, rc);
    ASSERT_FLOAT_EQ(1.1, flt);

    /*
     * Convert non-numeric string to number
     */

    /* Create the field */
    rc = ib_field_create(&f1, MemPool(), "one", 3,
                         IB_FTYPE_NULSTR, ib_ftype_nulstr_in("x1"));
    ASSERT_EQ(IB_OK, rc);

    /* Attempt a numeric conversion. */
    rc = ib_field_convert(MemPool(), IB_FTYPE_NUM, f1, &f2);
    ASSERT_EQ(IB_EINVAL, rc);

    /*
     * Convert floating-point string to number
     */

    /* Create the field */
    rc = ib_field_create(&f1, MemPool(), "one", 3,
                         IB_FTYPE_NULSTR, ib_ftype_nulstr_in("1.1"));
    ASSERT_EQ(IB_OK, rc);

    /* Attempt a numeric conversion. */
    rc = ib_field_convert(MemPool(), IB_FTYPE_NUM, f1, &f2);
    ASSERT_EQ(IB_EINVAL, rc);


    /*
     * Convert non-numeric string to float
     */

    /* Create the field */
    rc = ib_field_create(&f1, MemPool(), "one", 3,
                         IB_FTYPE_NULSTR, ib_ftype_nulstr_in("x1.1"));
    ASSERT_EQ(IB_OK, rc);

    /* Attempt a numeric conversion. */
    rc = ib_field_convert(MemPool(), IB_FTYPE_FLOAT, f1, &f2);
    ASSERT_EQ(IB_EINVAL, rc);
}

TEST_F(TestIBUtilField, ConvertNumbers)
{
    ib_field_t *f1;
    ib_field_t *f2;
    ib_status_t rc;
    ib_num_t    num;
    ib_float_t  flt;

    /*
     * Convert numeric to a float
     */

    /* Create the field */
    num = 1;
    rc = ib_field_create(&f1, MemPool(), "one", 3,
                         IB_FTYPE_NUM, ib_ftype_num_in(&num));
    ASSERT_EQ(IB_OK, rc);

    /* Attempt a numeric conversion. */
    rc = ib_field_convert(MemPool(), IB_FTYPE_FLOAT, f1, &f2);
    ASSERT_EQ(IB_OK, rc);

    /* Pull out param value for check. */
    rc = ib_field_value(f2, ib_ftype_float_out(&flt));
    ASSERT_EQ(IB_OK, rc);
    ASSERT_FLOAT_EQ(1.0, flt);

    /*
     * Convert floating-point to num
     */

    /* Create the field */
    flt = 1.0;
    rc = ib_field_create(&f1, MemPool(), "one", 3,
                         IB_FTYPE_FLOAT, ib_ftype_float_in(&flt));
    ASSERT_EQ(IB_OK, rc);

    /* Attempt a numeric conversion. */
    rc = ib_field_convert(MemPool(), IB_FTYPE_NUM, f1, &f2);
    ASSERT_EQ(IB_OK, rc);

    /* Pull out param value for check. */
    rc = ib_field_value(f2, ib_ftype_num_in(&num));
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(1, num);

    /*
     * Convert floating-point to num
     */

    /* Create the field */
    flt = 1.1;
    rc = ib_field_create(&f1, MemPool(), "one", 3,
                         IB_FTYPE_FLOAT, ib_ftype_float_in(&flt));
    ASSERT_EQ(IB_OK, rc);

    /* Attempt a numeric conversion. */
    rc = ib_field_convert(MemPool(), IB_FTYPE_NUM, f1, &f2);
    ASSERT_EQ(IB_OK, rc);

    /* Pull out param value for check. */
    rc = ib_field_value(f2, ib_ftype_num_in(&num));
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(1, num);
}

TEST_F(TestIBUtilField, AliasBytestr)
{
    const char s1[] = "hello";
    const char s2[] = "bye";
    ib_field_t *f;
    const ib_bytestr_t *obs;
    ib_status_t rc;
    ib_bytestr_t *bs;
    uint8_t *copy;

    copy = (uint8_t *)MemPoolMemDup("x", 1);
    rc = ib_field_create_bytestr_alias(&f, MemPool(),
                                       IB_S2SL("foo"), copy, 0);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_bytestr_dup_nulstr(&bs, MemPool(), s1);
    ASSERT_EQ(IB_OK, rc);
    rc = ib_field_setv(f, bs);
    ASSERT_EQ(IB_OK, rc);
    rc = ib_field_value(f, ib_ftype_bytestr_out(&obs));
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(strlen(s1), ib_bytestr_length(obs));
    ASSERT_EQ(0, memcmp(s1,
                        ib_bytestr_const_ptr(obs), ib_bytestr_length(obs)) );

    rc = ib_bytestr_dup_nulstr(&bs, MemPool(), s2);
    ASSERT_EQ(IB_OK, rc);
    rc = ib_field_setv(f, bs);
    ASSERT_EQ(IB_OK, rc);
    rc = ib_field_value(f, ib_ftype_bytestr_out(&obs));
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(strlen(s2), ib_bytestr_length(obs));
    ASSERT_EQ(0, memcmp(s2,
                        ib_bytestr_const_ptr(obs), ib_bytestr_length(obs)) );
}

TEST_F(TestIBUtilField, AliasConvert)
{
    char       *str;
    ib_field_t *f1;
    ib_field_t *f2;
    ib_status_t rc;
    ib_num_t    num;
    ib_float_t  flt;

    /*
     * Convert numeric string to number
     */

    /* Copy a number into the string */
    str = ib_mpool_strdup(MemPool(), "1");
    ASSERT_TRUE(str != NULL);

    /* Create the aliased field */
    rc = ib_field_create_alias(&f1, MemPool(), "one", 3,
                               IB_FTYPE_NULSTR,
                               ib_ftype_nulstr_mutable_out(&str));
    ASSERT_EQ(IB_OK, rc);

    /* Attempt a numeric conversion. */
    rc = ib_field_convert(MemPool(), IB_FTYPE_NUM, f1, &f2);
    ASSERT_EQ(IB_OK, rc);

    /* Pull out param value for check. */
    rc = ib_field_value(f2, ib_ftype_num_out(&num));
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(1, num);

    /*
     * Convert floating-point string to float
     */

    /* Copy a number into the string */
    str = ib_mpool_strdup(MemPool(), "1.1");
    ASSERT_TRUE(str != NULL);

    /* Create the aliased field */
    rc = ib_field_create_alias(&f1, MemPool(), "one", 3,
                               IB_FTYPE_NULSTR,
                               ib_ftype_nulstr_mutable_out(&str));
    ASSERT_EQ(IB_OK, rc);

    /* Attempt a numeric conversion. */
    rc = ib_field_convert(MemPool(), IB_FTYPE_FLOAT, f1, &f2);
    ASSERT_EQ(IB_OK, rc);

    /* Pull out param value for check. */
    rc = ib_field_value(f2, ib_ftype_float_out(&flt));
    ASSERT_EQ(IB_OK, rc);
    ASSERT_FLOAT_EQ(1.1, flt);

    /*
     * Convert non-numeric string to number
     */

    /* Copy a number into the string */
    str = ib_mpool_strdup(MemPool(), "x1");
    ASSERT_TRUE(str != NULL);

    /* Create the aliased field */
    rc = ib_field_create_alias(&f1, MemPool(), "one", 3,
                               IB_FTYPE_NULSTR,
                               ib_ftype_nulstr_mutable_out(&str));
    ASSERT_EQ(IB_OK, rc);

    /* Attempt a numeric conversion. */
    rc = ib_field_convert(MemPool(), IB_FTYPE_NUM, f1, &f2);
    ASSERT_EQ(IB_EINVAL, rc);

    /*
     * Convert floating-point string to number
     */

    /* Copy a number into the string */
    str = ib_mpool_strdup(MemPool(), "1.1");
    ASSERT_TRUE(str != NULL);

    /* Create the aliased field */
    rc = ib_field_create_alias(&f1, MemPool(), "one", 3,
                               IB_FTYPE_NULSTR,
                               ib_ftype_nulstr_mutable_out(&str));
    ASSERT_EQ(IB_OK, rc);

    /* Attempt a numeric conversion. */
    rc = ib_field_convert(MemPool(), IB_FTYPE_NUM, f1, &f2);
    ASSERT_EQ(IB_EINVAL, rc);


    /*
     * Convert non-numeric string to float
     */

    /* Copy a number into the string */
    str = ib_mpool_strdup(MemPool(), "1.1");
    ASSERT_TRUE(str != NULL);

    /* Create the aliased field */
    rc = ib_field_create_alias(&f1, MemPool(), "one", 3,
                               IB_FTYPE_NULSTR,
                               ib_ftype_nulstr_mutable_out(&str));
    ASSERT_EQ(IB_OK, rc);

    /* Attempt a numeric conversion. */
    rc = ib_field_convert(MemPool(), IB_FTYPE_FLOAT, f1, &f2);
    ASSERT_EQ(IB_OK, rc);

    /* Pull out param value for check. */
    rc = ib_field_value(f2, ib_ftype_float_out(&flt));
    ASSERT_EQ(IB_OK, rc);
    ASSERT_FLOAT_EQ(1.1, flt);
}
