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
/// @brief IronBee - Expand String Test Functions
/// 
/// @author Nick LeRoy <nleroy@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "ironbee_config_auto.h"

#include <ironbee/expand.h>
#include <ironbee/types.h>
#include <ironbee/field.h>
#include <ironbee/bytestr.h>
#include <ironbee/hash.h>

#include "ironbee_private.h"

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

#include <stdexcept>
#include <string>

// Used to initialize fields in the hash
typedef struct
{
    const char     *key;
    ib_ftype_t      type;
    const char     *vstr;
    ib_num_t        vnum;
    ib_unum_t       vunum;
} field_def_t;

class TestIBUtilExpand : public testing::Test
{
public:
    TestIBUtilExpand() 
    {
        m_pool = NULL;
        m_hash = NULL;
    }

    ~TestIBUtilExpand()
    {
        TearDown( );
    }

    virtual void SetUp()
    {
        static const field_def_t field_defs [] = {
            { "Key1", IB_FTYPE_NULSTR,  "Value1", 0,  0 },
            { "Key2", IB_FTYPE_NULSTR,  "Value2", 0,  0 },
            { "Key3", IB_FTYPE_BYTESTR, "Value3", 0,  0 },
            { "Key4", IB_FTYPE_NUM,     NULL,     0,  0 },
            { "Key5", IB_FTYPE_NUM,     NULL,     1,  0 },
            { "Key6", IB_FTYPE_NUM,     NULL,     -1, 0 },
            { "Key7", IB_FTYPE_UNUM,    NULL,     0,  0 },
            { "Key8", IB_FTYPE_UNUM,    NULL,     0,  1 },
            { NULL,   IB_FTYPE_GENERIC, NULL,     0,  0 },
        };
        ib_status_t rc;

        rc = ib_mpool_create(&m_pool, NULL, NULL);
        if (rc != IB_OK) {
            throw std::runtime_error("Could not initialize mpool.");
        }
        rc = ib_hash_create(&m_hash, m_pool);
        if (rc != IB_OK) {
            throw std::runtime_error("Could not initialize hash.");
        }
        PopulateHash( field_defs );
    }

    virtual void TearDown()
    {
        if (m_pool != NULL) {
            ib_mpool_destroy(m_pool);
        }
        m_pool = NULL;
    }

    virtual void PopulateHash( const field_def_t field_defs[] )
    {
        const field_def_t *fdef;
        std::string msg;
        ib_status_t rc;
        ib_num_t n;

        for (n = 0, fdef = &field_defs[n]; fdef->key != NULL; ++n, ++fdef) {
            ib_field_t *field;
            ib_bytestr_t *bs;
            switch( fdef->type ) {
                case IB_FTYPE_NULSTR:
                    rc = ib_field_create(&field, m_pool, fdef->key,
                                         fdef->type, (void *)&(fdef->vstr));
                    break;
                case IB_FTYPE_BYTESTR:
                    rc = ib_bytestr_dup_nulstr(&bs, m_pool, fdef->vstr);
                    if (rc != IB_OK) {
                        msg  = "Error creating bytestr from '";
                        msg += fdef->vstr;
                        msg += "': ";
                        msg += rc;
                        throw std::runtime_error(msg);
                    }
                    rc = ib_field_create(&field, m_pool, fdef->key,
                                         fdef->type, (void *)&bs);
                    break;
                case IB_FTYPE_NUM:
                    rc = ib_field_create(&field, m_pool, fdef->key,
                                         fdef->type, (void *)&(fdef->vnum));
                    break;
                case IB_FTYPE_UNUM:
                    rc = ib_field_create(&field, m_pool, fdef->key,
                                         fdef->type, (void *)&(fdef->vunum));
                    break;
            }
            if (rc != IB_OK) {
                msg  = "Error creating field '";
                msg += fdef->key;
                msg += "': ";
                msg += rc;
                throw std::runtime_error(msg);
            }
            rc = ib_hash_set(m_hash, fdef->key, field);
            if (rc != IB_OK) {
                msg  = "Error adding field '";
                msg += fdef->key;
                msg += "' to hash: ";
                msg += rc;
                throw std::runtime_error(msg);
            }
        }
    }

    void PrintError(ib_num_t lineno,
                    const char *text,
                    const char *prefix,
                    const char *suffix,
                    const char *expected,
                    const char *value)
    {
        std::cout << "Test defined on line " << lineno << " failed"
                  << std::endl;
        std::cout << "'" << text << "' expanded using '" << prefix << suffix
                  << "' -> '" << value << "' expected '" << expected
                  << "'" << std::endl;
    }

    void PrintError(ib_num_t lineno,
                    const char *text,
                    const char *prefix,
                    const char *suffix,
                    ib_num_t expected,
                    ib_num_t value)
    {
        std::cout << "Test defined on line " << lineno << " failed"
                  << std::endl;
        std::cout << "'" << text << "' expanded using '" << prefix << suffix
                  << "' -> '" << value << "' expected '" << expected
                  << "'" << std::endl;
    }
    
protected:
    ib_mpool_t *m_pool;
    ib_hash_t  *m_hash;
};

class TestIBUtilExpandStr : public TestIBUtilExpand
{
public:

    ib_status_t ExpandStr(const char *text,
                          const char *prefix,
                          const char *suffix,
                          char **result)
    {
        return ::ib_expand_str(m_pool, text, prefix, suffix, m_hash, result);
    }

    bool IsExpected(ib_num_t lineno,
                    const char *text,
                    const char *prefix,
                    const char *suffix,
                    const char *expected,
                    const char *value)
    {
        if (strcmp(value, expected) == 0) {
            return true;
        }
        else {
            PrintError(lineno, text, prefix, suffix, expected, value);
            return false;
        }
    }

    void RunTest( ib_num_t lineno,
                  const char *text,
                  const char *prefix,
                  const char *suffix,
                  const char *expected )
    {
        char *result;
        ib_status_t rc;
        rc = ExpandStr(text, prefix, suffix, &result);
        ASSERT_EQ(IB_OK, rc);
        EXPECT_TRUE(IsExpected(lineno, text, prefix, suffix, expected, result));
    }
};

class TestIBUtilExpandTestStr : public TestIBUtilExpand
{
public:
    ib_status_t ExpandTestStr(const char *text,
                              const char *prefix,
                              const char *suffix,
                              ib_bool_t *result)
    {
        return ::ib_expand_test_str(text, prefix, suffix, result);
    }

    bool IsExpected(ib_num_t lineno,
                    const char *text,
                    const char *prefix,
                    const char *suffix,
                    ib_bool_t expected,
                    ib_bool_t value)
    {
        if (value == expected) {
            return true;
        }
        else {
            PrintError(lineno, text, prefix, suffix, expected, value);
            return false;
        }
    }

    void RunTest( ib_num_t lineno,
                  const char *text,
                  const char *prefix,
                  const char *suffix,
                  ib_bool_t expected )
    {
        ib_bool_t result;
        ib_status_t rc;
        rc = ExpandTestStr(text, prefix, suffix, &result);
        ASSERT_EQ(IB_OK, rc);
        EXPECT_TRUE(IsExpected(lineno, text, prefix, suffix, expected, result));
    }
};


/* -- Tests -- */

TEST_F(TestIBUtilExpandStr, test_expand_errors)
{
    ib_status_t rc;
    char *expanded;

    rc = ExpandStr("%{foo}", "", "}", &expanded);
    ASSERT_EQ(IB_EINVAL, rc);
    ASSERT_EQ( (char *)NULL, expanded );

    rc = ExpandStr("%{foo}", "{", "", &expanded);
    ASSERT_EQ(IB_EINVAL, rc);
    ASSERT_EQ( (char *)NULL, expanded);

    rc = ExpandStr("%{foo}", "{", "}", &expanded);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_NE( (char *)NULL, expanded);

    rc = ExpandStr("%{foo}", "(", "", &expanded);
    ASSERT_EQ(IB_EINVAL, rc);
    ASSERT_EQ( (char *)NULL, expanded);

    rc = ExpandStr("%{foo}", "", ")", &expanded);
    ASSERT_EQ(IB_EINVAL, rc);
    ASSERT_EQ( (char *)NULL, expanded);

    rc = ExpandStr("%{foo}", "(", ")", &expanded);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_NE( (char *)NULL, expanded);
}

TEST_F(TestIBUtilExpandStr, test_expand_basics)
{
    RunTest(__LINE__, "simple text",      "%{", "}",  "simple text");
    RunTest(__LINE__, "simple text",      "$(", ")",  "simple text");
    RunTest(__LINE__, "text:%{Key1}",     "%{", "}",  "text:Value1");
    RunTest(__LINE__, "text:%{Key1}",     "$(", ")",  "text:%{Key1}");
    RunTest(__LINE__, "text:{Key1}",      "{",  "}",  "text:Value1");
    RunTest(__LINE__, "text:%{Key1}",     "<<", ">>", "text:%{Key1}");
    RunTest(__LINE__, "text:<<Key1>>",    "<<", ">>", "text:Value1");
    RunTest(__LINE__, "text:<<Key1>>",    "%{", "}",  "text:<<Key1>>");
    RunTest(__LINE__, "text:$(Key1)",     "%{", "}",  "text:$(Key1)");
    RunTest(__LINE__, "text:$(Key1)",     "$(", ")",  "text:Value1");
    RunTest(__LINE__, "text:${Key1}",     "%{", "}",  "text:${Key1}");
    RunTest(__LINE__, "text:${Key1}",     "$(", ")",  "text:${Key1}");
    RunTest(__LINE__, "text:${Key1}",     "${", "}",  "text:Value1");
    RunTest(__LINE__, "text:%{Key2}",     "%{", "}",  "text:Value2");
}

TEST_F(TestIBUtilExpandStr, test_expand_corner_cases)
{
    RunTest(__LINE__, "%{}",              "%{", "}",  "");
    RunTest(__LINE__, "%{}",              "{",  "}",  "%");
    RunTest(__LINE__, "%{}%" ,            "%{", "}",  "%");
    RunTest(__LINE__, "%{}%{",            "%{", "}",  "%{");
    RunTest(__LINE__, "%{}}",             "%{", "}",  "}");
    RunTest(__LINE__, "%{foo}",           "%{", "}",  "");
    RunTest(__LINE__, "%%{foo}",          "%{", "}",  "%");
    RunTest(__LINE__, "%%{Key1}",         "%{", "}",  "%Value1");
    RunTest(__LINE__, "%{%{foo}",         "%{", "}",  "");
    RunTest(__LINE__, "%{%{Key1}",        "%{", "}",  "");
    RunTest(__LINE__, "%{%{Key1}}",       "%{", "}",  "}");
    RunTest(__LINE__, "text:%{Key11}",    "%{", "}",  "text:");
    RunTest(__LINE__, "text:%{Key 1}",    "%{", "}",  "text:");
    RunTest(__LINE__, "text:%{Key*1}",    "%{", "}",  "text:");
    RunTest(__LINE__, "text:%{Key1 }",    "%{", "}",  "text:");
    RunTest(__LINE__, "%{Key9}",          "%{", "}",  "");
}

TEST_F(TestIBUtilExpandStr, test_expand_complex)
{
    RunTest(__LINE__, "%{Key1}:%{Key2}",  "%{", "}",  "Value1:Value2");
    RunTest(__LINE__, "%{Key1}:%{Key2}",  "%{", "}",  "Value1:Value2");
    RunTest(__LINE__, "%{Key3}:%{Key1}",  "%{", "}",  "Value3:Value1");
    RunTest(__LINE__, "%{Key1}:%{Key2}==${Key3}", "%{", "}",
            "Value1:Value2==${Key3}");
    RunTest(__LINE__, "%{Key1}:%{Key2}==%{Key3}", "%{", "}",
             "Value1:Value2==Value3");
}

TEST_F(TestIBUtilExpandStr, test_expand_numbers)
{
    RunTest(__LINE__, "%{Key4}",          "%{", "}",  "0");
    RunTest(__LINE__, "%{Key5}",          "%{", "}",  "1");
    RunTest(__LINE__, "%{Key6}",          "%{", "}",  "-1");
    RunTest(__LINE__, "%{Key7}",          "%{", "}",  "0");
    RunTest(__LINE__, "%{Key8}",          "%{", "}",  "1");
    RunTest(__LINE__, "%{Key4}-%{Key8}",  "%{", "}",  "0-1");
    RunTest(__LINE__, "%{Key4}-%{Key6}",  "%{", "}",  "0--1");
    RunTest(__LINE__, "%{Key4}+%{Key8}",  "%{", "}",  "0+1");
}

TEST_F(TestIBUtilExpandTestStr, test_expand_test_errors)
{
    ib_status_t rc;
    ib_bool_t expand;
    rc = ExpandTestStr("%{foo}", "", "}", &expand);
    ASSERT_EQ(IB_EINVAL, rc);
    ASSERT_EQ(IB_FALSE, expand);

    rc = ExpandTestStr("%{foo}", "{", "}", &expand);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(IB_TRUE, expand);

    rc = ExpandTestStr("%{foo}", "{", "", &expand);
    ASSERT_EQ(IB_EINVAL, rc);
    ASSERT_EQ(IB_FALSE, expand);

    rc = ExpandTestStr("%{foo}", "(", ")", &expand);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(IB_FALSE, expand);

    rc = ExpandTestStr("%{foo}", "(", "", &expand);
    ASSERT_EQ(IB_EINVAL, rc);
    ASSERT_EQ(IB_FALSE, expand);

    rc = ExpandTestStr("%{foo}", "", ")", &expand);
    ASSERT_EQ(IB_EINVAL, rc);
    ASSERT_EQ(IB_FALSE, expand);
}

TEST_F(TestIBUtilExpandTestStr, test_expand_test_str)
{
    RunTest(__LINE__, "simple text",      "%{", "}",  IB_FALSE);
    RunTest(__LINE__, "simple text",      "$(", ")",  IB_FALSE);
    RunTest(__LINE__, "text:%{Key1}",     "%{", "}",  IB_TRUE);
    RunTest(__LINE__, "text:%{Key1}",     "$(", ")",  IB_FALSE);
    RunTest(__LINE__, "text:{Key1}",      "{",  "}",  IB_TRUE);
    RunTest(__LINE__, "text:%{Key1}",     "<<", ">>", IB_FALSE);
    RunTest(__LINE__, "text:<<Key1>>",    "<<", ">>", IB_TRUE);
    RunTest(__LINE__, "text:<<Key1>>",    "%{", "}",  IB_FALSE);
    RunTest(__LINE__, "text:$(Key1)",     "%{", "}",  IB_FALSE);
    RunTest(__LINE__, "text:$(Key1)",     "$(", ")",  IB_TRUE);
    RunTest(__LINE__, "text:${Key1}",     "%{", "}",  IB_FALSE);
    RunTest(__LINE__, "text:${Key1}",     "$(", ")",  IB_FALSE);
    RunTest(__LINE__, "text:${Key1}",     "${", "}",  IB_TRUE);
    RunTest(__LINE__, "text:%{Key2}",     "%{", "}",  IB_TRUE);
}
