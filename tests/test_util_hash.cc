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
/// @brief IronBee - Hash Test
///
/// @author Pablo Rincon <pablo.rincon.crespo@gmail.com>
/// @author Christopher Alfeld <calfeld@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include <ironbee/hash.h>

#include "ironbee_config_auto.h"

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

#include <ironbee/mpool.h>

#include <stdexcept>

class TestIBUtilHash : public testing::Test
{
public:
    TestIBUtilHash() 
    {
        ib_status_t rc = ib_mpool_create(&m_pool, NULL, NULL);
        if (rc != IB_OK) {
            throw std::runtime_error("Could not initialize mpool.");
        }
    }
    
    ~TestIBUtilHash()
    {
        ib_mpool_destroy(m_pool);
    }
    
protected:
    ib_mpool_t* m_pool;
};

TEST_F(TestIBUtilHash, test_hash_create)
{
    ib_hash_t   *hash = NULL;
    ib_status_t  rc;

    rc = ib_hash_create(&hash, m_pool);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(hash);
    ib_hash_clear(hash);
}

TEST_F(TestIBUtilHash, test_hash_set_and_get)
{
    ib_hash_t   *hash = NULL;
    char        *val = NULL;
    ib_status_t  rc;

    rc = ib_hash_create(&hash, m_pool);
    ASSERT_EQ(IB_OK, rc);
    rc = ib_hash_set(hash, "Key", (void*)"value");
    ASSERT_EQ(IB_OK, rc);

    rc = ib_hash_get((void **)&val, hash, "Key");
    ASSERT_EQ(IB_OK, rc);
    ASSERT_STREQ("value", val);

    rc = ib_hash_set(hash, "Key2", (void*)"value2");
    ASSERT_EQ(IB_OK, rc);

    val = NULL;
    rc = ib_hash_get((void **)&val, hash, "Key");
    ASSERT_EQ(IB_OK, rc);
    ASSERT_STREQ("value", val);

    val = NULL;
    rc = ib_hash_get((void **)&val, hash, "Key2");
    ASSERT_EQ(IB_OK, rc);
    ASSERT_STREQ("value2", val);

    val = NULL;
    rc = ib_hash_get((void **)&val, hash, "noKey");
    ASSERT_EQ(IB_ENOENT, rc);
}

TEST_F(TestIBUtilHash, test_hash_nocase)
{
    ib_hash_t   *hash = NULL;
    ib_status_t  rc;

    rc = ib_hash_create_nocase(&hash, m_pool);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_hash_set(hash, "Key", (void*)"value");
    ASSERT_EQ(IB_OK, rc);

    char *val = NULL;
    rc = ib_hash_get((void **)&val, hash, "kEY");
    ASSERT_EQ(IB_OK, rc);

    ASSERT_STREQ("value", val);
    rc = ib_hash_set(hash, "KeY2", (void*)"value2");
    ASSERT_EQ(IB_OK, rc);

    val = NULL;
    rc = ib_hash_get((void **)&val, hash, "KeY");
    ASSERT_EQ(IB_OK, rc);

    ASSERT_STREQ("value", val);
    val = NULL;
    rc = ib_hash_get((void **)&val, hash, "KEY2");
    ASSERT_EQ(IB_OK, rc);

    ASSERT_STREQ("value2", val);

    val = NULL;
    rc = ib_hash_get((void **)&val, hash, "noKey");
    ASSERT_EQ(IB_ENOENT, rc);
}

TEST_F(TestIBUtilHash, test_hash_ex)
{
    ib_hash_t         *hash = NULL;
    ib_status_t        rc;
    static const char  key1[] = "Key1";
    static const char  key2[] = "Key2";
    static const char  key3[] = "kEY1";
    static const char  key4[] = "kEY2";

    rc = ib_hash_create_ex(
        &hash, 
        m_pool, 
        17, 
        ib_hashfunc_djb2,
        ib_hashequal_default
    );
    ASSERT_EQ(IB_OK, rc);

    rc = ib_hash_set_ex(hash, key1, 2, (void*)"value");
    ASSERT_EQ(IB_OK, rc);

    char *val = NULL;
    rc = ib_hash_get_ex((void **)&val, hash, (void *)key1, 2);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_STREQ("value", val);

    rc = ib_hash_set_ex(hash, (void *)key2, 2, (void*)"other");
    ASSERT_EQ(IB_OK, rc);

    val = NULL;
    rc = ib_hash_get_ex((void **)&val, hash, (void *)key2, 2);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_STREQ("other", val);

    val = NULL;
    rc = ib_hash_get_ex((void **)&val, hash, (void *)key1, 2);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_STREQ("other", val);

    val = NULL;
    rc = ib_hash_get_ex((void **)&val, hash, (void *)key3, 2);
    ASSERT_EQ(IB_ENOENT, rc);

    val = NULL;
    rc = ib_hash_get_ex((void **)&val, hash, (void *)key4, 2);
    ASSERT_EQ(IB_ENOENT, rc);
}

TEST_F(TestIBUtilHash, test_hashfunc_djb2)
{
    unsigned int hash1 = 0;
    unsigned int hash2 = 0;

    // Test with no case sensitive
    hash1 = ib_hashfunc_djb2_nocase("Key", 3);
    hash2 = ib_hashfunc_djb2_nocase("kEY", 3);
    ASSERT_EQ(hash2, hash1);
    // Test with case sensitive
    hash1 = hash2 = 0;
    hash1 = ib_hashfunc_djb2("Key", 3);
    hash2 = ib_hashfunc_djb2("kEY", 3);
    ASSERT_NE(hash2, hash1);
}

TEST_F(TestIBUtilHash, test_hashequal)
{
    EXPECT_EQ(1, ib_hashequal_default("key",3,"key",3));
    EXPECT_EQ(0, ib_hashequal_default("key",3,"kEy",3));
    EXPECT_EQ(0, ib_hashequal_default("key",3,"keys",4));
    EXPECT_EQ(1, ib_hashequal_nocase("key",3,"key",3));
    EXPECT_EQ(1, ib_hashequal_nocase("key",3,"kEy",3));
    EXPECT_EQ(0, ib_hashequal_nocase("key",3,"kEys",4));
}

TEST_F(TestIBUtilHash, test_hash_resizing)
{
    ib_status_t  rc;
    ib_hash_t   *hash = NULL;

    static const char combs[] = "abcdefghij";

    rc = ib_hash_create(&hash, m_pool);
    ASSERT_EQ(IB_OK, rc);

    // Insert 1000 keys with value equal to the key used.
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            for (int k = 0; k < 10; k++) {
                char *c = (char *)ib_mpool_calloc(m_pool, 1, 4);
                ASSERT_TRUE(c);
                c[0] = combs[i];
                c[1] = combs[j];
                c[2] = combs[k];
                c[4] = '\0';
                rc = ib_hash_set_ex(hash, c, 3, (void *)c);
                ASSERT_EQ(IB_OK, rc);

                // Check now (pre-resizing) and later (after resizing).
                char *val = NULL;
                rc = ib_hash_get_ex((void **)&val, hash, c, 3);
                ASSERT_EQ(IB_OK, rc);
                ASSERT_STREQ(c, val);

            }
        }
    }

    // After resizing
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            for (int k = 0; k < 10; k++) {
                char *c = (char *)ib_mpool_calloc(m_pool, 1, 4);
                ASSERT_TRUE(c);
                c[0] = combs[i];
                c[1] = combs[j];
                c[2] = combs[k];
                c[4] = '\0';
                char *val = NULL;
                rc = ib_hash_get_ex((void **)&val, hash, c, 3);
                ASSERT_EQ(IB_OK, rc);
                ASSERT_STREQ(c, val);

            }
        }
    }
}

TEST_F(TestIBUtilHash, test_hash_getall)
{
    ib_status_t  rc;
    ib_hash_t   *hash  = NULL;
    ib_list_t   *list  = NULL;
    ib_list_t   *list2 = NULL;

    static const char combs[] = "abcdefghij";

    rc = ib_list_create(&list, m_pool);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_list_create(&list2, m_pool);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_hash_create(&hash, m_pool);
    ASSERT_EQ(IB_OK, rc);

    // Insert 1000 keys with value equal to the key used.
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            for (int k = 0; k < 10; k++) {
                char *c = (char *)ib_mpool_calloc(m_pool, 1, 4);
                EXPECT_TRUE(c);
                c[0] = combs[i];
                c[1] = combs[j];
                c[2] = combs[k];
                c[4] = '\0';

                rc = ib_hash_set_ex(hash, c, 3, (void *)c);
                ASSERT_EQ(IB_OK, rc);

                rc = ib_list_push(list, (void *)c);
                ASSERT_EQ(IB_OK, rc);

            }
        }
    }

    ASSERT_EQ(1000UL, ib_list_elements(list));

    rc = ib_hash_get_all(list2, hash);
    ASSERT_EQ(IB_OK, rc);
    {
        ib_list_node_t *li = NULL;
        ib_list_node_t *li2 = NULL;
        size_t num_found = 0;
        // We know that all elements of list are unique, so we make sure 
        //every element of list is in list2.
        IB_LIST_LOOP(list, li) {
            IB_LIST_LOOP(list2, li2) {
                if ( memcmp(li->data, li2->data,4) == 0 ) {
                    ++num_found;
                    break;
                }
            }
        }
        ASSERT_EQ(1000UL, num_found);
    }
}

TEST_F(TestIBUtilHash, test_hash_clear)
{
    ib_status_t  rc;
    ib_hash_t   *hash  = NULL;

    static const char combs[] = "abcdefghij";

    rc = ib_hash_create(&hash, m_pool);
    ASSERT_EQ(IB_OK, rc);

    // Insert 1000 keys with value equal to the key used.
    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 10; ++j) {
            for (int k = 0; k < 10; ++k) {
                char *v;
                char *c = (char *)ib_mpool_calloc(m_pool, 1, 4);
                EXPECT_TRUE(c);
                c[0] = combs[i];
                c[1] = combs[j];
                c[2] = combs[k];
                c[4] = '\0';

                ASSERT_EQ(IB_OK, ib_hash_set_ex(hash, c, 3, (void *)c));
                ASSERT_EQ(IB_OK, ib_hash_get_ex((void **)&v, hash, c, 3));
                ASSERT_EQ(v, (void*)c);
            }
        }
    }

    ib_hash_clear(hash);

    for (int i = 9; i >= 0; --i) {
        for (int j = 9; j >= 0; --j) {
            for (int k = 9; k >= 0; --k) {
                char *v;
                char *c = (char *)ib_mpool_calloc(m_pool, 1, 4);
                EXPECT_TRUE(c);
                c[0] = combs[i];
                c[1] = combs[j];
                c[2] = combs[k];
                c[4] = '\0';

                ASSERT_EQ(IB_OK, ib_hash_set_ex(hash, c, 3, (void *)c));
                ASSERT_EQ(IB_OK, ib_hash_get_ex((void **)&v, hash, c, 3));
                ASSERT_EQ(v, (void*)c);
            }
        }
    }
}

static unsigned int test_hash_delete_hashfunc(
    const void* key,
    size_t      key_length
)
{
    return 1234;
}

TEST_F(TestIBUtilHash, test_hash_collision_delete)
{
    ib_status_t  rc;
    ib_hash_t   *hash  = NULL;
    int          value;
    
    // Make sure we have collisions.
    rc = ib_hash_create_ex(
        &hash, 
        m_pool,
        17,
        test_hash_delete_hashfunc,
        ib_hashequal_default
    );
    ASSERT_EQ(IB_OK, rc);
    
    EXPECT_EQ(IB_OK, ib_hash_set(hash, "abc", (void*)7));
    EXPECT_EQ(IB_OK, ib_hash_set(hash, "def", (void*)8));
    EXPECT_EQ(IB_OK, ib_hash_set(hash, "ghi", (void*)9));
    
    EXPECT_EQ(IB_OK, ib_hash_get((void**)&value, hash, "abc"));
    EXPECT_EQ(7, value);
    EXPECT_EQ(IB_OK, ib_hash_get((void**)&value, hash, "def"));
    EXPECT_EQ(8, value);
    EXPECT_EQ(IB_OK, ib_hash_get((void**)&value, hash, "ghi"));
    EXPECT_EQ(9, value);
    
    EXPECT_EQ(IB_OK, ib_hash_set(hash, "abc", NULL));
    
    EXPECT_EQ(IB_ENOENT, ib_hash_get((void**)&value, hash, "abc"));
    EXPECT_EQ(IB_OK, ib_hash_get((void**)&value, hash, "def"));
    EXPECT_EQ(8, value);
    EXPECT_EQ(IB_OK, ib_hash_get((void**)&value, hash, "ghi"));
    EXPECT_EQ(9, value);    
}
