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

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

#define TESTING

#include "util/util.c"
#include "util/logformat.c"
#include "util/mpool.c"
#include "util/debug.c"


/* -- Tests -- */

/// @test Test util logformat library - ib_logformat_create() and *_set()
TEST(TestIBUtilLogformat, test_logformat_create_and_set)
{
    ib_mpool_t *mp;
    ib_logformat_t *logformat;
    ib_status_t rc;
    
    atexit(ib_shutdown);
    rc = ib_initialize();
    ASSERT_TRUE(rc == IB_OK) << "ib_initialize() failed - rc != IB_OK";
    rc = ib_mpool_create(&mp, NULL);
    ASSERT_TRUE(rc == IB_OK) << "ib_mpool_create() failed - rc != IB_OK";
    
    ib_logformat_t *lf = NULL;
    
    rc = ib_logformat_create(mp, &lf);
    ASSERT_TRUE(rc == IB_OK) << "ib_logformat_create() failed - rc != IB_OK";

    rc = ib_logformat_set(lf, IB_LOGFORMAT_DEFAULT);
    ASSERT_TRUE(rc == IB_OK) << "ib_logformat_set() failed - rc != IB_OK";
    ASSERT_TRUE(strncmp(lf->fields, "ThaSstf", 7) == 0) << "ib_logformat_set()"
                            " failed - fields string (parsed) should be "
                            "ThaSstf, got " << lf->fields;

    ASSERT_TRUE(lf->field_cnt == 7) << "ib_logformat_set()"
                            " failed - should have " << lf->field_cnt <<
                            "fields, got " << lf->field_cnt;

    ASSERT_TRUE(lf->literal_cnt == 6) << "ib_logformat_set()"
                            " failed - should have " << lf->literal_cnt <<
                            "literals, got " << lf->literal_cnt;
    ASSERT_TRUE(lf->literals[6] == NULL) << "ib_logformat_set()"
                            " failed - should be NULL";

    ASSERT_TRUE(lf->literal_starts == 0) << "ib_logformat_set()"
                            " failed - should start with a field (because of "
                            "the default format string";
    int i = 0;
    /* All the literal strings between fields are spaces, because of the
       default format string */

    /* We DO want to check for '\0' as well as the data itself */
    ASSERT_TRUE(memcmp(lf->literals[i++], " ", 2) == 0) << "ib_logformat_set()"
                            " failed - wrong literal string here";

    ASSERT_TRUE(memcmp(lf->literals[i++], " ", 2) == 0) << "ib_logformat_set()"
                            " failed - wrong literal string here";
    ASSERT_TRUE(memcmp(lf->literals[i++], " ", 2) == 0) << "ib_logformat_set()"
                            " failed - wrong literal string here";
    ASSERT_TRUE(memcmp(lf->literals[i++], " ", 2) == 0) << "ib_logformat_set()"
                            " failed - wrong literal string here";
    ASSERT_TRUE(memcmp(lf->literals[i++], " ", 2) == 0) << "ib_logformat_set()"
                            " failed - wrong literal string here";
    ASSERT_TRUE(memcmp(lf->literals[i++], " ", 2) == 0) << "ib_logformat_set()"
                            " failed - wrong literal string here";

    ib_mpool_destroy(mp);
}

/// @test Test util logformat library - ib_logformat_create() and *_set()
TEST(TestIBUtilLogformat, test_logformat_set)
{
    ib_mpool_t *mp;
    ib_logformat_t *logformat;
    ib_status_t rc;
    
    atexit(ib_shutdown);
    rc = ib_initialize();
    ASSERT_TRUE(rc == IB_OK) << "ib_initialize() failed - rc != IB_OK";
    rc = ib_mpool_create(&mp, NULL);
    ASSERT_TRUE(rc == IB_OK) << "ib_mpool_create() failed - rc != IB_OK";
    
    ib_logformat_t *lf = NULL;
    
    rc = ib_logformat_create(mp, &lf);
    ASSERT_TRUE(rc == IB_OK) << "ib_logformat_create() failed - rc != IB_OK";

    rc = ib_logformat_set(lf, (char *)"Myformat %S %h %s %f end");
    ASSERT_TRUE(rc == IB_OK) << "ib_logformat_set() failed - rc != IB_OK";
    ASSERT_TRUE(strncmp(lf->fields, "Shsf", 4) == 0) << "ib_logformat_set()"
                            " failed - fields string (parsed) should be "
                            "ThaSstf, got " << lf->fields;

    ASSERT_TRUE(lf->field_cnt == 4) << "ib_logformat_set()"
                            " failed - should have " << lf->field_cnt <<
                            "fields, got " << lf->field_cnt;

    ASSERT_TRUE(lf->literal_cnt == 5) << "ib_logformat_set()"
                            " failed - should have " << lf->literal_cnt <<
                            "literals, got " << lf->literal_cnt;

    ASSERT_TRUE(lf->literals[5] == NULL) << "ib_logformat_set()"
                            " failed - should be NULL";

    ASSERT_TRUE(lf->literal_starts == 1) << "ib_logformat_set()"
                            " failed - should start with a literal";
    int i = 0;
    /* All the literal strings between fields are spaces, because of the
       default format string */

    /* We DO want to check for '\0' as well as the data itself */
    ASSERT_TRUE(memcmp(lf->literals[i++], "Myformat ", 9) == 0) << "ib_logformat_set()"
                            " failed - wrong literal string here";

    ASSERT_TRUE(memcmp(lf->literals[i++], " ", 2) == 0) << "ib_logformat_set()"
                            " failed - wrong literal string here";
    ASSERT_TRUE(memcmp(lf->literals[i++], " ", 2) == 0) << "ib_logformat_set()"
                            " failed - wrong literal string here";
    ASSERT_TRUE(memcmp(lf->literals[i++], " ", 2) == 0) << "ib_logformat_set()"
                            " failed - wrong literal string here";
    ASSERT_TRUE(memcmp(lf->literals[i++], " end", 5) == 0) << "ib_logformat_set()"
                            " failed - wrong literal string here";

    ib_mpool_destroy(mp);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    ib_trace_init(NULL);
    return RUN_ALL_TESTS();
}


