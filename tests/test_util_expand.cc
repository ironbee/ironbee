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

// Used to define test cases
typedef struct
{
    ib_num_t        lineno;
    const char     *text;
    const char     *start;
    const char     *end;
    const char     *expected;
} test_data_t;

class TestIBUtilExpandStr : public testing::Test
{
public:
    TestIBUtilExpandStr() 
    {
        ib_status_t rc;

        rc = ib_mpool_create(&m_pool, NULL, NULL);
        if (rc != IB_OK) {
            throw std::runtime_error("Could not initialize mpool.");
        }
        rc = ib_hash_create(&m_hash, m_pool);
        if (rc != IB_OK) {
            throw std::runtime_error("Could not initialize hash.");
        }
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
        PopulateHash( field_defs );
    }

    virtual void TearDown()
    {
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

    ib_status_t ExpandStr(const char *str,
                          const char *start,
                          const char *end,
                          char **result)
    {
        return ::expand_str(m_pool, str, start, end, m_hash, result);
    }

    ~TestIBUtilExpandStr()
    {
        ib_mpool_destroy(m_pool);
    }
    
protected:
    ib_mpool_t *m_pool;
    ib_hash_t  *m_hash;
};

bool IsExpected(const test_data_t *test, const char *value)
{
    if (strcmp(value, test->expected) == 0) {
        return true;
    }
    else {
        std::cout << "Test defined on line " << test->lineno << " failed"
                  << std::endl;
        std::cout << "'" << test->text << "' expanded using '"
                  << test->start << test->end
                  << "' -> '" << value << "' expected '" << test->expected
                  << "'" << std::endl;
        return false;
    }
}

/* -- Tests -- */

TEST_F(TestIBUtilExpandStr, test_expand_str)
{
    const test_data_t data [] = {
        { __LINE__, "simple text",      "%{", "}", "simple text" },
        { __LINE__, "simple text",      "$(", ")", "simple text" },
        { __LINE__, "text:%{Key1}",     "%{", "}", "text:Value1" },
        { __LINE__, "text:%{Key1}",     "$(", ")", "text:%{Key1}" },
        { __LINE__, "text:$(Key1)",     "%{", "}", "text:$(Key1)" },
        { __LINE__, "text:$(Key1)",     "$(", ")", "text:Value1" },
        { __LINE__, "text:${Key1}",     "%{", "}", "text:${Key1}" },
        { __LINE__, "text:${Key1}",     "$(", ")", "text:${Key1}" },
        { __LINE__, "text:${Key1}",     "${", "}", "text:Value1" },
        { __LINE__, "text:%{Key2}",     "%{", "}", "text:Value2" },
        { __LINE__, "%{Key1}:%{Key2}",  "%{", "}", "Value1:Value2" },
        { __LINE__, "%{Key1}:%{Key2}",  "%{", "}", "Value1:Value2" },
        { __LINE__, "%{Key3}:%{Key1}",  "%{", "}", "Value3:Value1" },
        { __LINE__, "%{Key4}",          "%{", "}", "0" },
        { __LINE__, "%{Key5}",          "%{", "}", "1" },
        { __LINE__, "%{Key6}",          "%{", "}", "-1" },
        { __LINE__, "%{Key7}",          "%{", "}", "0" },
        { __LINE__, "%{Key8}",          "%{", "}", "1" },
        { __LINE__, "%{Key9}",          "%{", "}", "" },
        { __LINE__, NULL,               NULL, NULL, NULL },
    };
    const test_data_t *test;
    ib_num_t n;
    ib_status_t rc;

    printf("Starting tests\n");
    for (n = 1, test = &data[0];  test->text != NULL;  ++test, ++n) {
        char *expanded;
        rc = ExpandStr(test->text, test->start, test->end, &expanded);
        ASSERT_EQ(IB_OK, rc);
        EXPECT_TRUE(IsExpected(test, expanded));
    }
}
