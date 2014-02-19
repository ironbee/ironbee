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
 * @brief IronBee --- Test hex escape code.
 */

#include <ironbee/mm_mpool.h>

#include "ironbee/escape.h"
#include <gtest/gtest.h>

TEST(TestUtilHexEscape, basic) {
    ib_mpool_t *mp;
    ASSERT_EQ(IB_OK, ib_mpool_create(&mp, "", NULL));
    ib_mm_t mm = ib_mm_mpool(mp);

    const uint8_t *S = (const uint8_t *)"escape me: \01\02";
    char *s = ib_util_hex_escape(mm, S, strlen((const char *)S));

    ASSERT_STREQ("escape me: 0x10x2", s);
    ib_mpool_destroy(mp);
}

TEST(TestUtilHexEscape, corners)
{
    char *s;
    ib_mpool_t *mp;
    ASSERT_EQ(IB_OK, ib_mpool_create(&mp, "", NULL));
    ib_mm_t mm = ib_mm_mpool(mp);

    const uint8_t *S1 = (const uint8_t *)"\x00";
    s = ib_util_hex_escape(mm, S1, 1);
    ASSERT_STREQ("0x0", s);

    const uint8_t *S2 = (const uint8_t *)"\x10\x11\x80\xff";
    s = ib_util_hex_escape(mm, S2, 4);
    ASSERT_STREQ("0x100x110x800xff", s);
    ib_mpool_destroy(mp);
}
