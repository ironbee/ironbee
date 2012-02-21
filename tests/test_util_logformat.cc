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
/// @brief IronBee - Logformat Test Functions
/// 
/// @author Pablo Rincon Crespo <pablo.rincon.crespo@gmail.com>
//////////////////////////////////////////////////////////////////////////////

#include "ironbee_config_auto.h"

#include <ironbee/types.h>
#include <ironbee/mpool.h>
#include <ironbee/util.h>
#include <ironbee/logformat.h>

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

#include <stdexcept>

/* -- Tests -- */

class TestIBUtilLogformat : public ::testing::Test
{
public:
    TestIBUtilLogformat()
    {
        ib_status_t rc;
        
        ib_initialize();
        rc = ib_mpool_create(&m_pool, NULL, NULL);
        if (rc != IB_OK) {
            throw std::runtime_error("Could not create mpool.");
        }
    }
    
    ~TestIBUtilLogformat()
    {
        ib_shutdown();        
    }
    
protected:
    ib_mpool_t* m_pool;
};

/// @test Test util logformat library - ib_logformat_create() and *_set()
TEST_F(TestIBUtilLogformat, test_logformat_create_and_set)
{
    ib_status_t rc;
 
    ib_logformat_t *lf = NULL;
    
    rc = ib_logformat_create(m_pool, &lf);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_logformat_set(lf, IB_LOGFORMAT_DEFAULT);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(0, strncmp(lf->fields, "ThaSstf", 7));
    ASSERT_EQ(7, lf->field_cnt);
    ASSERT_EQ(6, lf->literal_cnt);
    ASSERT_FALSE(lf->literals[6]);
    ASSERT_EQ(0, lf->literal_starts);

    int i = 0;
    /* All the literal strings between fields are spaces, because of the
       default format string */

    /* We DO want to check for '\0' as well as the data itself */
    ASSERT_EQ(0, memcmp(lf->literals[i++], " ", 2));
    ASSERT_EQ(0, memcmp(lf->literals[i++], " ", 2));
    ASSERT_EQ(0, memcmp(lf->literals[i++], " ", 2));
    ASSERT_EQ(0, memcmp(lf->literals[i++], " ", 2));
    ASSERT_EQ(0, memcmp(lf->literals[i++], " ", 2));
    ASSERT_EQ(0, memcmp(lf->literals[i++], " ", 2));
}

/// @test Test util logformat library - ib_logformat_create() and *_set()
TEST_F(TestIBUtilLogformat, test_logformat_set)
{
    ib_status_t rc;
    
    ib_logformat_t *lf = NULL;
    
    rc = ib_logformat_create(m_pool, &lf);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_logformat_set(lf, (char *)"Myformat %S %h %s %f end");
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(0, strncmp(lf->fields, "Shsf", 4));
    ASSERT_EQ(4, lf->field_cnt);
    ASSERT_EQ(5, lf->literal_cnt);
    ASSERT_FALSE(lf->literals[5]);
    ASSERT_EQ(1, lf->literal_starts);

    int i = 0;
    /* All the literal strings between fields are spaces, because of the
       default format string */

    /* We DO want to check for '\0' as well as the data itself */
    ASSERT_EQ(0, memcmp(lf->literals[i++], "Myformat ", 9));
    ASSERT_EQ(0, memcmp(lf->literals[i++], " ", 2));
    ASSERT_EQ(0, memcmp(lf->literals[i++], " ", 2));
    ASSERT_EQ(0, memcmp(lf->literals[i++], " ", 2));
    ASSERT_EQ(0, memcmp(lf->literals[i++], " end", 5));
}
