/*****************************************************************************
 * Licensed to Qualys, Inc. (QUALYS) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * QUALYS licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ****************************************************************************/

/**
 * @file
 * @brief IronAutomata --- VLS test.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironautomata/vls.h>

#include <algorithm>

#include <stdint.h>

#include "gtest/gtest.h"

using namespace std;

TEST(TestVls, Basic)
{
    static const char c_extra[] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    struct example_t {
        uint32_t a;
        uint32_t b;
        uint32_t d;
        char extra[];
    };

    example_t* example =
        reinterpret_cast<example_t*>(malloc(sizeof(example_t) + 10));

    example->a = 1;
    example->b = 2;
    example->d = 3;
    copy(c_extra, c_extra + 10, example->extra);

    ia_vls_state_t vls;
    /* Hack to point to start. */
    char* just_before_example = reinterpret_cast<char *>(example) - 1;
    IA_VLS_INIT(vls, just_before_example);

    uint32_t vls_a = IA_VLS_IF(vls, uint32_t, 0, true);
    EXPECT_EQ(example->a, vls_a);

    IA_VLS_ADVANCE_IF(vls, uint32_t, false);
    IA_VLS_ADVANCE_IF(vls, uint32_t, true);

    uint32_t vls_c = IA_VLS_IF(vls, uint32_t, 1234, false);
    EXPECT_EQ(1234UL, vls_c);

    uint32_t* vls_d = IA_VLS_IF_PTR(vls, uint32_t, true);
    ASSERT_TRUE(vls_d);
    EXPECT_EQ(example->d, *vls_d);
    *vls_d = 5;
    EXPECT_EQ(example->d, *vls_d);

    char* vls_extra_a = IA_VLS_VARRAY_IF(vls, char, 5, true);
    ASSERT_TRUE(vls_extra_a);
    EXPECT_TRUE(equal(c_extra, c_extra + 5, vls_extra_a));

    char* vls_extra_b = NULL;
    vls_extra_b = IA_VLS_VARRAY_IF(vls, char, 5, false);
    EXPECT_EQ(NULL, vls_extra_b);

    char* vls_extra_c = IA_VLS_VARRAY(vls, char, 2);
    ASSERT_TRUE(vls_extra_c);
    EXPECT_TRUE(equal(c_extra + 5, c_extra + 7, vls_extra_c));

    char* vls_extra_d = IA_VLS_FINAL(vls, char);
    ASSERT_TRUE(vls_extra_d);
    EXPECT_TRUE(equal(c_extra + 7, c_extra + 10, vls_extra_d));

    free(example);
}
