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
/// @brief IronBee - Hash Test Functions
///
/// @author Pablo Rincon <pablo.rincon.crespo@gmail.com>
//////////////////////////////////////////////////////////////////////////////

#include <ironbee/hash.h>

#include "ironbee_config_auto.h"

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

#include <ironbee/mpool.h>

/* -- Tests -- */

/// @test Test util hash library - ib_hash_create()
TEST(TestIBUtilHash, test_hash_create)
{
    ib_mpool_t *mp = NULL;
    ib_hash_t *ht = NULL;
    ib_status_t rc;

    rc = ib_mpool_create(&mp, NULL, NULL);
    ASSERT_TRUE(rc == IB_OK) << "ib_mpool_create() failed - rc != IB_OK";
    rc = ib_hash_create(&ht, mp);
    ASSERT_TRUE(rc == IB_OK) << "ib_hash_create_ex() failed - rc != IB_OK";
    ib_hash_clear(ht);

    ib_mpool_destroy(mp);
}

/// @test Test util hash library - ib_hash_create_ex(), ib_hash_set/get()
TEST(TestIBUtilHash, test_hash_set_and_get)
{
    ib_mpool_t *mp = NULL;
    ib_hash_t *ht = NULL;
    ib_status_t rc;

    rc = ib_mpool_create(&mp, NULL, NULL);
    ASSERT_TRUE(rc == IB_OK) << "ib_mpool_create() failed - rc != IB_OK";

    rc = ib_hash_create_ex(&ht, mp, 17, ib_hashfunc_djb2, ib_hashequal_default);
    ASSERT_TRUE(rc == IB_OK) << "ib_hash_create_ex() failed - rc != IB_OK";
    rc = ib_hash_set(ht, "Key", (void*)"value");
    ASSERT_TRUE(rc == IB_OK) << "ib_hash_set() failed - rc != IB_OK";

    char *val = NULL;
    rc = ib_hash_get((void **)&val, ht, "Key");
    ASSERT_TRUE(rc == IB_OK) << "ib_hash_get() failed - rc != IB_OK";
    ASSERT_TRUE(strncmp("value", val, 5) == 0) << "ib_hash_get() failed -"
                                             " expected \"value\", got " << val;
    rc = ib_hash_set(ht, "Key2", (void*)"value2");
    ASSERT_TRUE(rc == IB_OK) << "ib_hash_set() failed - rc != IB_OK";
    val = NULL;
    rc = ib_hash_get((void **)&val, ht, "Key");
    ASSERT_TRUE(rc == IB_OK) << "ib_hash_get() failed - rc != IB_OK";
    ASSERT_TRUE(strncmp("value", val, 5) == 0) << "ib_hash_get() failed -"
                                             " expected \"value\", got " << val;
    val = NULL;
    rc = ib_hash_get((void **)&val, ht, "Key2");
    ASSERT_TRUE(rc == IB_OK) << "ib_hash_get() failed - rc != IB_OK";
    ASSERT_TRUE(strncmp("value2", val, 5) == 0) << "ib_hash_get() failed -"
                                             " expected \"value\", got " << val;

    val = NULL;
    rc = ib_hash_get((void **)&val, ht, "noKey");
    ASSERT_TRUE(rc == IB_ENOENT) << "ib_hash_get() failed - rc != IB_ENOENT";

    ib_mpool_destroy(mp);
}

/// @test Test util hash library - ib_hash_set/get() with nocase
TEST(TestIBUtilHash, test_hash_nocase)
{
    ib_mpool_t *mp = NULL;
    ib_hash_t *ht = NULL;
    ib_status_t rc;

    rc = ib_mpool_create(&mp, NULL, NULL);
    ASSERT_TRUE(rc == IB_OK) << "ib_mpool_create() failed - rc != IB_OK";

    rc = ib_hash_create_ex(&ht, mp, 17, ib_hashfunc_djb2_nocase, ib_hashequal_nocase);
    ASSERT_TRUE(rc == IB_OK) << "ib_hash_create_ex() failed - rc != IB_OK";

    rc = ib_hash_set(ht, "Key", (void*)"value");
    ASSERT_TRUE(rc == IB_OK) << "ib_hash_set() failed - rc != IB_OK";

    char *val = NULL;
    rc = ib_hash_get((void **)&val, ht, "kEY");
    ASSERT_TRUE(rc == IB_OK) << "ib_hash_get() failed - rc != IB_OK";

    ASSERT_TRUE(strncmp("value", val, 5) == 0) << "ib_hash_get() "
                                                  "failed - expected "
                                                  "\"value\", got " << val;
    rc = ib_hash_set(ht, "KeY2", (void*)"value2");
    ASSERT_TRUE(rc == IB_OK) << "ib_hash_set() failed - rc != IB_OK";

    val = NULL;
    rc = ib_hash_get((void **)&val, ht, "KeY");
    ASSERT_TRUE(rc == IB_OK) << "ib_hash_get() failed - rc != IB_OK";

    ASSERT_TRUE(strncmp("value", val, 5) == 0) << "ib_hash_get() failed "
                                                  "- expected \"value\", "
                                                  "got " << val;
    val = NULL;
    rc = ib_hash_get((void **)&val, ht, "KEY2");
    ASSERT_TRUE(rc == IB_OK) << "ib_hash_get() failed - rc != IB_OK";

    ASSERT_TRUE(strncmp("value2", val, 6) == 0) << "ib_hash_get() failed"
                                           " - expected \"value\", got " << val;

    val = NULL;
    rc = ib_hash_get((void **)&val, ht, "noKey");
    ASSERT_TRUE(rc == IB_ENOENT) << "ib_hash_get() failed - "
                                    "rc != IB_ENOENT";

    ib_mpool_destroy(mp);
}

/// @test Test util hash library - ib_hash_create_ex(), ib_hash_set/get_ex()
TEST(TestIBUtilHash, test_hash_set_and_get_ex)
{
    ib_mpool_t *mp = NULL;
    ib_hash_t *ht = NULL;
    ib_status_t rc;
    char key1[] = "Key1";
    char key2[] = "Key2";
    char key3[] = "kEY1";
    char key4[] = "kEY2";

    rc = ib_mpool_create(&mp, NULL, NULL);
    ASSERT_TRUE(rc == IB_OK) << "ib_mpool_create() failed - rc != IB_OK";

    rc = ib_hash_create_ex(&ht, mp, 17, ib_hashfunc_djb2, 0);
    ASSERT_TRUE(rc == IB_OK) << "ib_hash_create_ex() failed - rc != IB_OK";

    rc = ib_hash_set_ex(ht, key1, 2, (void*)"value");
    ASSERT_TRUE(rc == IB_OK) << "ib_hash_set() failed - rc != IB_OK";

    char *val = NULL;
    rc = ib_hash_get_ex((void **) &val, ht, key1, 2);
    ASSERT_TRUE(rc == IB_OK) << "ib_hash_get_ex() failed - rc != IB_OK";
    ASSERT_TRUE(strncmp("value", val, 5) == 0) << "ib_hash_get_ex() failed -";

    rc = ib_hash_set_ex(ht, key2, 2, (void*)"other");
    ASSERT_TRUE(rc == IB_OK) << "ib_hash_set() failed - rc != IB_OK";

    val = NULL;
    rc = ib_hash_get_ex((void **) &val, ht, key2, 2);
    ASSERT_TRUE(rc == IB_OK) << "ib_hash_get_ex() failed - rc != IB_OK";
    ASSERT_TRUE(strncmp("other", val, 5) == 0) << "ib_hash_get_ex() failed -";

    val = NULL;
    rc = ib_hash_get_ex((void **) &val, ht, key1, 2);
    ASSERT_TRUE(rc == IB_OK) << "ib_hash_get_ex() failed - rc != IB_OK";
    ASSERT_TRUE(strncmp("other", val, 5) == 0) << "ib_hash_get_ex() failed -";

    val = NULL;
    rc = ib_hash_get_ex((void **) &val, ht, key3, 2);
    ASSERT_TRUE(rc == IB_ENOENT) << "ib_hash_get_ex() failed - rc != IB_ENOENT";

    val = NULL;
    rc = ib_hash_get_ex((void **) &val, ht, key4, 2);
    ASSERT_TRUE(rc == IB_ENOENT) << "ib_hash_get_ex() failed - rc != IB_ENOENT";

    ib_mpool_destroy(mp);
}

/// @test Test util hash library - ib_hash_set/get_ex() with nocase
TEST(TestIBUtilHash, test_hash_get_ex_nocase)
{
    ib_mpool_t *mp = NULL;
    ib_hash_t *ht = NULL;
    ib_status_t rc;
    char key1[] = "Key1";
    char key2[] = "Key2";
    char key3[] = "kEY1";
    char key4[] = "kEY2";

    rc = ib_mpool_create(&mp, NULL, NULL);
    ASSERT_TRUE(rc == IB_OK) << "ib_mpool_create() failed - rc != IB_OK";

    rc = ib_hash_create_ex(&ht, mp, 17, ib_hashfunc_djb2_nocase, ib_hashequal_nocase);
    ASSERT_TRUE(rc == IB_OK) << "ib_hash_create_ex() failed - rc != IB_OK";

    rc = ib_hash_set_ex(ht, key1, 2, (void*)"value");
    ASSERT_TRUE(rc == IB_OK) << "ib_hash_set() failed - rc != IB_OK";

    char *val = NULL;
    rc = ib_hash_get_ex((void **) &val, ht, key1, 2);
    ASSERT_TRUE(rc == IB_OK) << "ib_hash_get_ex() failed - rc != IB_OK";
    ASSERT_TRUE(strncmp("value", val, 5) == 0) << "ib_hash_get_ex() failed -";

    rc = ib_hash_set_ex(ht, key2, 2, (void*)"other");
    ASSERT_TRUE(rc == IB_OK) << "ib_hash_set() failed - rc != IB_OK";

    val = NULL;
    rc = ib_hash_get_ex((void **) &val, ht, key2, 2);
    ASSERT_TRUE(rc == IB_OK) << "ib_hash_get_ex() failed - rc != IB_OK";
    ASSERT_TRUE(strncmp("other", val, 5) == 0) << "ib_hash_get_ex() failed -";

    val = NULL;
    rc = ib_hash_get_ex((void **) &val, ht, key1, 2);
    ASSERT_TRUE(rc == IB_OK) << "ib_hash_get_ex() failed - rc != IB_OK";
    ASSERT_TRUE(strncmp("other", val, 5) == 0) << "ib_hash_get_ex() failed -";

    val = NULL;
    rc = ib_hash_get_ex((void **) &val, ht, key3, 2);
    ASSERT_TRUE(rc == IB_ENOENT) << "ib_hash_get_ex() failed - rc != IB_ENOENT";

    val = NULL;
    rc = ib_hash_get_ex((void **) &val, ht, key4, 2);
    ASSERT_TRUE(rc == IB_ENOENT) << "ib_hash_get_ex() failed - rc != IB_ENOENT";

    val = NULL;
    rc = ib_hash_get_ex((void **) &val, ht, key3, 2);
    ASSERT_TRUE(rc == IB_OK) << "ib_hash_get_ex() failed - rc != IB_OK";
    ASSERT_TRUE(strncmp("other", val, 5) == 0) << "ib_hash_get_ex() failed -";

    val = NULL;
    rc = ib_hash_get_ex((void **) &val, ht, key4, 2);
    ASSERT_TRUE(rc == IB_OK) << "ib_hash_get_ex() failed - rc != IB_OK";
    ASSERT_TRUE(strncmp("other", val, 5) == 0) << "ib_hash_get_ex() failed -";

    ib_mpool_destroy(mp);
}

/// @test Test util hash library - ib_hash_djb2 with flags
TEST(TestIBUtilHash, test_hashfunc_djb2)
{
    unsigned int hash1 = 0;
    unsigned int hash2 = 0;

    // Test with no case sensitive
    hash1 = ib_hashfunc_djb2("Key", 3);
    hash2 = ib_hashfunc_djb2("kEY", 3);
    ASSERT_TRUE(hash1 == hash2) << "ib_hashfunc_djb2() failed - hash1:"
                                << hash1 << " != Hash2:" << hash2
                                << " but should be equal";
    // Test with case sensitive
    hash1 = hash2 = 0;
    hash1 = ib_hashfunc_djb2("Key", 3);
    hash2 = ib_hashfunc_djb2("kEY", 3);
    ASSERT_TRUE(hash1 != hash2) << "ib_hashfunc_djb2() failed - hash1:"
                                << hash1 << " == Hash2:" << hash2
                                << " but should not be equal";
}

/// @test Test util hash library - Check multiple keys and resizing
TEST(TestIBUtilHash, test_hash_resizing)
{
    ib_mpool_t *mp = NULL;
    ib_status_t rc;
    ib_hash_t *ht = NULL;

    char combs[] = "abcdefghij";

    rc = ib_mpool_create(&mp, NULL, NULL);
    ASSERT_TRUE(rc == IB_OK) << "ib_mpool_create() failed - rc != IB_OK";

    rc = ib_hash_create_ex(&ht, mp, 17, ib_hashfunc_djb2_nocase, ib_hashequal_nocase);
    ASSERT_TRUE(rc == IB_OK) << "ib_hash_create_ex() failed - rc != IB_OK";

    int i = 0;
    int j = 0;
    int k = 0;

    /* Insert 1000 keys with value equal to the key used */
    for (i = 0; i < 10; i++) {
        for (j = 0; j < 10; j++) {
            for (k = 0; k < 10; k++) {
                char *c = (char *)ib_mpool_calloc(mp, 1, 4);
                ASSERT_TRUE(c != NULL) << "ib_mpool_calloc() failed "
                                          "- c == NULL";
                c[0] = combs[i];
                c[1] = combs[j];
                c[2] = combs[k];
                c[4] = '\0';
                rc = ib_hash_set_ex(ht, c, 3, (void *)c);
                ASSERT_TRUE(rc == IB_OK) << "ib_hash_set() failed "
                                            "- rc != IB_OK";

                // Search keys now (because it will perform resizing as soon as
                // we reach the ratio, so check it before) and after resizing
                char *val = NULL;
                rc = ib_hash_get_ex((void **) &val, ht, c, 3);
                ASSERT_TRUE(rc == IB_OK) << "ib_hash_get_ex() failed -"
                                            " rc != IB_OK at key '" << c << "'";
                ASSERT_TRUE(strncmp(c, val, 3) == 0) << "ib_hash_get_ex()"
                                                              " failed -";

            }
        }
    }

    // After resizing
    for (i = 0; i < 10; i++) {
        for (j = 0; j < 10; j++) {
            for (k = 0; k < 10; k++) {
                char *c = (char *)ib_mpool_calloc(mp, 1, 4);
                ASSERT_TRUE(c != NULL) << "ib_mpool_calloc() failed "
                                          "- c == NULL";
                c[0] = combs[i];
                c[1] = combs[j];
                c[2] = combs[k];
                c[4] = '\0';
                char *val = NULL;
                rc = ib_hash_get_ex((void **) &val, ht, c, 3);
                ASSERT_TRUE(rc == IB_OK) << "ib_hash_get_ex() failed "
                                            "- rc != IB_OK";
                ASSERT_TRUE(strncmp(c, val, 3) == 0) << "ib_hash_get_ex()"
                                                              " failed -";

            }
        }
    }

    ib_mpool_destroy(mp);
}

/// @test Test util hash library - Check multiple keys and resizing
TEST(TestIBUtilHash, test_hash_getall)
{
    ib_mpool_t *mp = NULL;
    ib_status_t rc;
    ib_hash_t *ht = NULL;
    ib_list_t *list = NULL;
    ib_list_t *list2 = NULL;

    char combs[] = "abcdefghij";

    rc = ib_mpool_create(&mp, NULL, NULL);
    ASSERT_TRUE(rc == IB_OK) << "ib_mpool_create() failed - rc != IB_OK";

    rc = ib_list_create(&list, mp);
    ASSERT_TRUE(rc == IB_OK) << "ib_list_create() failed - rc != IB_OK";

    rc = ib_list_create(&list2, mp);
    ASSERT_TRUE(rc == IB_OK) << "ib_list_create() failed - rc != IB_OK";

    rc = ib_hash_create_ex(&ht, mp, 17, ib_hashfunc_djb2_nocase, ib_hashequal_nocase);
    ASSERT_TRUE(rc == IB_OK) << "ib_hash_create_ex() failed - rc != IB_OK";

    int i = 0;
    int j = 0;
    int k = 0;

    /* Insert 1000 keys with value equal to the key used */
    for (; i < 10; i++) {
        for (j = 0; j < 10; j++) {
            for (k = 0; k < 10; k++) {
                char *c = (char *)ib_mpool_calloc(mp, 1, 4);
                ASSERT_TRUE(c != NULL) << "ib_mpool_calloc() failed "
                                          "- c == NULL";
                c[0] = combs[i];
                c[1] = combs[j];
                c[2] = combs[k];
                c[4] = '\0';

                rc = ib_hash_set_ex(ht, c, 3, (void *)c);
                ASSERT_TRUE(rc == IB_OK) << "ib_hash_set() failed "
                                            "- rc != IB_OK";

                /* Insert it in the list first */
                rc = ib_list_push(list, (void *)c);
                ASSERT_TRUE(rc == IB_OK) << "ib_list_push() failed "
                                            "- rc != IB_OK";

            }
        }
    }

    ASSERT_TRUE(ib_list_elements(list) == 1000) << "ib_list_push() failed"
                                            " - We need to make sure that all"
                                            " the items are inserted. Count is "
                                            << ib_list_elements(list);

    rc = ib_hash_get_all(list2, ht);
    ASSERT_EQ(IB_OK, rc);
    {
        ib_list_node_t *li = NULL;
        ib_list_node_t *li2 = NULL;
        size_t num_found = 0;
        /*
         * We know that all elements of list are unique, so we make sure 
         * every element of list is in list2.
         */
        IB_LIST_LOOP(list, li) {
            IB_LIST_LOOP(list2, li2) {
                if ( memcmp(li->data,*((void**)(li2->data)),4) == 0 ) {
                    ++num_found;
                    break;
                }
            }
        }
        ASSERT_EQ(1000UL, num_found);
    }
    
    ib_mpool_destroy(mp);
}
