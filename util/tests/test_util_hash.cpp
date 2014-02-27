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
/// @brief IronBee --- Hash Test
///
/// @author Pablo Rincon <pablo.rincon.crespo@gmail.com>
/// @author Christopher Alfeld <calfeld@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include <ironbee/hash.h>

#include "ironbee_config_auto.h"

#include "gtest/gtest.h"
#include "simple_fixture.hpp"

#include <ironbee/mm.h>

#include <stdexcept>

class TestIBUtilHash : public SimpleFixture
{
};

TEST_F(TestIBUtilHash, test_hash_create)
{
    ib_hash_t *hash = NULL;

    ASSERT_EQ(IB_OK, ib_hash_create(&hash, MM()));
    ASSERT_TRUE(hash);
    EXPECT_EQ(0UL, ib_hash_size(hash));
    ib_hash_clear(hash);
}

TEST_F(TestIBUtilHash, test_hash_set_and_get)
{
    ib_hash_t  *hash  = NULL;
    const char *value = NULL;

    ASSERT_EQ(IB_OK, ib_hash_create(&hash, MM()));
    ASSERT_EQ(IB_OK, ib_hash_set(hash, "Key", (void *)"value"));
    EXPECT_EQ(1UL, ib_hash_size(hash));

    EXPECT_EQ(IB_OK, ib_hash_get(hash, &value, "Key"));
    EXPECT_STREQ("value", value);

    ASSERT_EQ(IB_OK,ib_hash_set(hash, "Key2", (void *)"value2"));
    EXPECT_EQ(2UL, ib_hash_size(hash));

    value = NULL;
    EXPECT_EQ(IB_OK, ib_hash_get(hash, &value, "Key"));
    EXPECT_STREQ("value", value);

    value = NULL;
    EXPECT_EQ(IB_OK, ib_hash_get(hash, &value, "Key2"));
    EXPECT_STREQ("value2", value);

    value = NULL;
    EXPECT_EQ(IB_ENOENT, ib_hash_get(hash, &value, "noKey"));
}

TEST_F(TestIBUtilHash, test_hash_nocase)
{
    ib_hash_t *hash = NULL;

    ASSERT_EQ(IB_OK, ib_hash_create_nocase(&hash, MM()));

    ASSERT_EQ(IB_OK, ib_hash_set(hash, "Key", (void *)"value"));

    char *val = NULL;
    EXPECT_EQ(IB_OK, ib_hash_get(hash, &val, "kEY"));

    EXPECT_STREQ("value", val);
    ASSERT_EQ(IB_OK, ib_hash_set(hash, "KeY2", (void *)"value2"));

    val = NULL;
    EXPECT_EQ(IB_OK, ib_hash_get(hash, &val, "KeY"));

    EXPECT_STREQ("value", val);
    val = NULL;
    EXPECT_EQ(IB_OK, ib_hash_get(hash, &val, "KEY2"));

    EXPECT_STREQ("value2", val);

    val = NULL;
    EXPECT_EQ(IB_ENOENT, ib_hash_get(hash, &val, "noKey"));
}

TEST_F(TestIBUtilHash, test_hash_ex)
{
    ib_hash_t         *hash = NULL;
    static const char  key1[] = "Key1";
    static const char  key2[] = "Key2";
    static const char  key3[] = "kEY1";
    static const char  key4[] = "kEY2";

    ASSERT_EQ(IB_OK, ib_hash_create_ex(
        &hash,
        MM(),
        32,
        ib_hashfunc_djb2, NULL,
        ib_hashequal_default, NULL
    ));

    ASSERT_EQ(IB_OK, ib_hash_set_ex(hash, key1, 2, (void *)"value"));

    char *val = NULL;
    EXPECT_EQ(IB_OK, ib_hash_get_ex(hash, &val, key1, 2));
    EXPECT_STREQ("value", val);

    ASSERT_EQ(IB_OK, ib_hash_set_ex(hash, key2, 2, (void *)"other"));

    val = NULL;
    EXPECT_EQ(IB_OK, ib_hash_get_ex(hash, &val, key2, 2));
    EXPECT_STREQ("other", val);

    val = NULL;
    EXPECT_EQ(IB_OK, ib_hash_get_ex(hash, &val, key1, 2));
    EXPECT_STREQ("other", val);

    val = NULL;
    EXPECT_EQ(
        IB_ENOENT,
        ib_hash_get_ex(hash, &val, key3, 2)
    );

    val = NULL;
    EXPECT_EQ(
        IB_ENOENT,
        ib_hash_get_ex(hash, &val, key4, 2)
    );
}

TEST_F(TestIBUtilHash, test_hashfunc_djb2)
{
    uint32_t hash1 = 0;
    uint32_t hash2 = 0;

    // Test with no case sensitive
    hash1 = ib_hashfunc_djb2_nocase("Key", 3, 17, NULL);
    hash2 = ib_hashfunc_djb2_nocase("kEY", 3, 17, NULL);
    EXPECT_EQ(hash2, hash1);
    // Test with case sensitive
    hash1 = ib_hashfunc_djb2("Key", 3, 17, NULL);
    hash2 = ib_hashfunc_djb2("kEY", 3, 17, NULL);
    EXPECT_NE(hash2, hash1);
}

TEST_F(TestIBUtilHash, test_hashfunc_randomizer)
{
    uint32_t hash1 = 0;
    uint32_t hash2 = 0;

    // Different randomizers means different values.
    hash1 = ib_hashfunc_djb2_nocase("Key", 3, 17, NULL);
    hash2 = ib_hashfunc_djb2_nocase("Key", 3, 23, NULL);
    EXPECT_NE(hash2, hash1);
    hash1 = ib_hashfunc_djb2("Key", 3, 17, NULL);
    hash2 = ib_hashfunc_djb2("Key", 3, 23, NULL);
    EXPECT_NE(hash2, hash1);
}

TEST_F(TestIBUtilHash, test_hashequal)
{
    EXPECT_EQ(1, ib_hashequal_default("key",3,"key",3, NULL));
    EXPECT_EQ(0, ib_hashequal_default("key",3,"kEy",3, NULL));
    EXPECT_EQ(0, ib_hashequal_default("key",3,"keys",4, NULL));
    EXPECT_EQ(1, ib_hashequal_nocase("key",3,"key",3, NULL));
    EXPECT_EQ(1, ib_hashequal_nocase("key",3,"kEy",3, NULL));
    EXPECT_EQ(0, ib_hashequal_nocase("key",3,"kEys",4, NULL));
}

TEST_F(TestIBUtilHash, test_hash_resizing)
{
    ib_hash_t *hash = NULL;

    static const char combs[] = "abcdefghij";

    ASSERT_EQ(IB_OK, ib_hash_create(&hash, MM()));

    // Insert 1000 keys with value equal to the key used.
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            for (int k = 0; k < 10; k++) {
                char *c = (char *)ib_mm_calloc(MM(), 1, 4);
                ASSERT_TRUE(c);
                c[0] = combs[i];
                c[1] = combs[j];
                c[2] = combs[k];
                c[3] = '\0';
                ASSERT_EQ(IB_OK, ib_hash_set_ex(hash, c, 3, (void *)c));

                // Check now (pre-resizing) and later (after resizing).
                char *val = NULL;
                EXPECT_EQ(IB_OK, ib_hash_get_ex(hash, &val, c, 3));
                EXPECT_STREQ(c, val);

            }
        }
    }

    EXPECT_EQ(1000UL, ib_hash_size(hash));

    // After resizing
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            for (int k = 0; k < 10; k++) {
                char *c = (char *)ib_mm_calloc(MM(), 1, 4);
                ASSERT_TRUE(c);
                c[0] = combs[i];
                c[1] = combs[j];
                c[2] = combs[k];
                c[3] = '\0';
                char *val = NULL;
                EXPECT_EQ(IB_OK, ib_hash_get_ex(hash, &val, c, 3));
                EXPECT_STREQ(c, val);

            }
        }
    }
}

TEST_F(TestIBUtilHash, test_hash_getall)
{
    ib_hash_t *hash  = NULL;
    ib_list_t *list  = NULL;
    ib_list_t *list2 = NULL;

    static const char combs[] = "abcdefghij";

    ASSERT_EQ(IB_OK, ib_list_create(&list, MM()));
    ASSERT_EQ(IB_OK, ib_list_create(&list2, MM()));
    ASSERT_EQ(IB_OK, ib_hash_create(&hash, MM()));

    // Insert 1000 keys with value equal to the key used.
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            for (int k = 0; k < 10; k++) {
                char *c = (char *)ib_mm_calloc(MM(), 1, 4);
                EXPECT_TRUE(c);
                c[0] = combs[i];
                c[1] = combs[j];
                c[2] = combs[k];
                c[3] = '\0';

                ASSERT_EQ(IB_OK, ib_hash_set_ex(hash, c, 3, (void *)c));
                ASSERT_EQ(IB_OK, ib_list_push(list, (void *)c));
            }
        }
    }

    EXPECT_EQ(1000UL, ib_list_elements(list));
    EXPECT_EQ(1000UL, ib_hash_size(hash));

    EXPECT_EQ(IB_OK, ib_hash_get_all(hash, list2));
    EXPECT_EQ(1000UL, ib_list_elements(list2));
    {
        ib_list_node_t *li  = NULL;
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
        EXPECT_EQ(1000UL, num_found);
    }
}

TEST_F(TestIBUtilHash, test_hash_clear)
{
    ib_hash_t *hash  = NULL;

    static const char combs[] = "abcdefghij";

    ASSERT_EQ(IB_OK, ib_hash_create(&hash, MM()));

    // Insert 1000 keys with value equal to the key used.
    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 10; ++j) {
            for (int k = 0; k < 10; ++k) {
                char *v;
                char *c = (char *)ib_mm_calloc(MM(), 1, 4);
                EXPECT_TRUE(c);
                c[0] = combs[i];
                c[1] = combs[j];
                c[2] = combs[k];
                c[3] = '\0';

                ASSERT_EQ(IB_OK, ib_hash_set_ex(hash, c, 3, (void *)c));
                EXPECT_EQ(IB_OK, ib_hash_get_ex(hash, &v, c, 3));
                EXPECT_EQ(v, (void *)c);
            }
        }
    }

    EXPECT_EQ(1000UL, ib_hash_size(hash));
    ib_hash_clear(hash);
    EXPECT_EQ(0UL, ib_hash_size(hash));

    for (int i = 9; i >= 0; --i) {
        for (int j = 9; j >= 0; --j) {
            for (int k = 9; k >= 0; --k) {
                char *v;
                char *c = (char *)ib_mm_calloc(MM(), 1, 4);
                EXPECT_TRUE(c);
                c[0] = combs[i];
                c[1] = combs[j];
                c[2] = combs[k];
                c[3] = '\0';

                ASSERT_EQ(IB_OK, ib_hash_set_ex(hash, c, 3, (void *)c));
                EXPECT_EQ(IB_OK, ib_hash_get_ex(hash, &v, c, 3));
                EXPECT_EQ(v, (void *)c);
            }
        }
    }
}

static uint32_t test_hash_delete_hashfunc(
    const char* key,
    size_t      key_length,
    uint32_t    randomzier,
    void*       cbdata
)
{
    return 1234;
}

TEST_F(TestIBUtilHash, test_hash_collision_delete)
{
    ib_hash_t *hash          = NULL;
    static const char* a     = "abc";
    static const char* b     = "def";
    static const char* c     = "ghi";
    const char*        value = NULL;

    // Make sure we have collisions.
    ASSERT_EQ(IB_OK, ib_hash_create_ex(
        &hash,
        MM(),
        32,
        test_hash_delete_hashfunc, NULL,
        ib_hashequal_default, NULL
    ));

    ASSERT_EQ(IB_OK, ib_hash_set(hash, a, (void *)a));
    ASSERT_EQ(IB_OK, ib_hash_set(hash, b, (void *)b));
    ASSERT_EQ(IB_OK, ib_hash_set(hash, c, (void *)c));
    EXPECT_EQ(IB_OK, ib_hash_get(hash, &value, a));
    EXPECT_EQ(a, value);
    EXPECT_EQ(IB_OK, ib_hash_get(hash, &value, b));
    EXPECT_EQ(b, value);
    EXPECT_EQ(IB_OK, ib_hash_get(hash, &value, c));
    EXPECT_EQ(c, value);

    EXPECT_EQ(IB_OK, ib_hash_set(hash, a, NULL));

    EXPECT_EQ(IB_ENOENT, ib_hash_get(hash, &value, a));
    EXPECT_EQ(IB_OK, ib_hash_get(hash, &value, b));
    EXPECT_EQ(b, value);
    EXPECT_EQ(IB_OK, ib_hash_get(hash, &value, c));
    EXPECT_EQ(c, value);
}

TEST_F(TestIBUtilHash, test_hash_remove)
{
    ib_hash_t *hash          = NULL;
    static const char* a     = "abc";
    static const char* b     = "def";
    static const char* c     = "ghi";
    const char*        value = NULL;

    ASSERT_EQ(IB_OK, ib_hash_create(&hash, MM()));
    ASSERT_EQ(IB_OK, ib_hash_set(hash, a, (void *)a));
    ASSERT_EQ(IB_OK, ib_hash_set(hash, b, (void *)b));
    ASSERT_EQ(IB_OK, ib_hash_set(hash, c, (void *)c));
    EXPECT_EQ(IB_OK, ib_hash_get(hash, &value, a));
    EXPECT_EQ(a, value);
    EXPECT_EQ(IB_OK, ib_hash_get(hash, &value, b));
    EXPECT_EQ(b, value);
    EXPECT_EQ(IB_OK, ib_hash_get(hash, &value, c));
    EXPECT_EQ(c, value);
    EXPECT_EQ(3UL, ib_hash_size(hash));

    EXPECT_EQ(IB_OK, ib_hash_remove(hash, &value, a));
    EXPECT_EQ(a, value);
    EXPECT_EQ(2UL, ib_hash_size(hash));
    EXPECT_EQ(IB_ENOENT, ib_hash_get(hash, &value, a));

    EXPECT_EQ(IB_OK, ib_hash_remove(hash, NULL, c));
    EXPECT_EQ(1UL, ib_hash_size(hash));
    EXPECT_EQ(IB_ENOENT, ib_hash_get(hash, &value, c));

    ASSERT_EQ(IB_OK, ib_hash_set(hash, a, (void *)7));
    EXPECT_EQ(2UL, ib_hash_size(hash));

    EXPECT_EQ(IB_ENOENT, ib_hash_remove(hash, NULL, c));

    EXPECT_EQ(
        IB_OK,
        ib_hash_remove_ex(hash, &value, b, 3)
    );
    EXPECT_EQ(b, value);
    EXPECT_EQ(1UL, ib_hash_size(hash));
}

TEST_F(TestIBUtilHash, bad_size) {
    ib_hash_t *hash = NULL;
    ASSERT_EQ(IB_EINVAL, ib_hash_create_ex(
        &hash,
        MM(),
        3,
        ib_hashfunc_djb2, NULL,
        ib_hashequal_default, NULL
    ));
}

TEST_F(TestIBUtilHash, iterator) {
    ib_hash_t *hash          = NULL;
    static const char* a     = "abc";
    static const char* b     = "def";
    static const char* c     = "ghi";
    const char*        value = NULL;

    ASSERT_EQ(IB_OK, ib_hash_create(&hash, MM()));
    ASSERT_EQ(IB_OK, ib_hash_set(hash, a, (void *)a));
    ASSERT_EQ(IB_OK, ib_hash_set(hash, b, (void *)b));
    ASSERT_EQ(IB_OK, ib_hash_set(hash, c, (void *)c));

    bool found_a = false;
    bool found_b = false;
    bool found_c = false;

    ib_hash_iterator_t *i = ib_hash_iterator_create(MM());
    ib_hash_iterator_first(i, hash);
    while (! ib_hash_iterator_at_end(i)) {
        const char *key = NULL;
        size_t key_length = 0;

        ib_hash_iterator_fetch(&key, &key_length, (void *)&value, i);

        if (key == std::string(a)) {
            ASSERT_FALSE(found_a);
            found_a = true;
            ASSERT_EQ(std::string(a), (const char *)value);
        }
        else if (key == std::string(b)) {
            ASSERT_FALSE(found_b);
            found_b = true;
            ASSERT_EQ(std::string(b), (const char *)value);
        }
        else if (key == std::string(c)) {
            ASSERT_FALSE(found_c);
            found_c = true;
            ASSERT_EQ(std::string(c), (const char *)value);
        }
        else {
            FAIL();
        }

        ib_hash_iterator_next(i);
    }

    ASSERT_TRUE(found_a);
    ASSERT_TRUE(found_b);
    ASSERT_TRUE(found_c);
}

TEST_F(TestIBUtilHash, non_printable_keys) {
    ib_hash_t *hash;
    const char *key = "\xff\xfe\xfd\xed\xee\xef";
    const char *data = "Some data.";
    char       *hash_data;
    ASSERT_EQ(IB_OK, ib_hash_create_nocase(&hash, MM()));
    ASSERT_EQ(
        IB_OK,
        ib_hash_set(
            hash,
            key,
            const_cast<void *>(static_cast<const void *>(data)))
    );

    hash_data = NULL;
    ASSERT_EQ(IB_OK, ib_hash_get(hash, &hash_data, key));
    ASSERT_EQ(data, hash_data);
    ASSERT_STREQ(data, hash_data);

    hash_data = NULL;
    ASSERT_EQ(IB_OK, ib_hash_remove(hash, &hash_data, key));
    ASSERT_EQ(data, hash_data);
    ASSERT_STREQ(data, hash_data);

    hash_data = NULL;
    ASSERT_EQ(IB_ENOENT, ib_hash_get(hash, &hash_data, key));
}
