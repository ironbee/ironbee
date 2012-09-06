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
/// @brief IronBee &mdash; Flag utility tests
///
/// @author Nick LeRoy <nleroy@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include <ironbee/util.h>

#include "ironbee_config_auto.h"

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

#include <ironbee/types.h>

#include <stdexcept>

const ib_flags_t FLAG_01 = (1 <<  0);
const ib_flags_t FLAG_02 = (1 <<  1);
const ib_flags_t FLAG_03 = (1 <<  2);
const ib_flags_t FLAG_04 = (1 <<  3);
const ib_flags_t FLAG_05 = (1 <<  4);
const ib_flags_t FLAG_06 = (1 <<  5);
const ib_flags_t FLAG_07 = (1 <<  6);
const ib_flags_t FLAG_08 = (1 <<  7);
const ib_flags_t FLAG_09 = (1 <<  8);
const ib_flags_t FLAG_10 = (1 <<  9);
const ib_flags_t FLAG_11 = (1 << 10);
const ib_flags_t FLAG_12 = (1 << 11);
const ib_flags_t FLAG_13 = (1 << 12);
const ib_flags_t FLAG_14 = (1 << 13);
const ib_flags_t FLAG_15 = (1 << 14);
const ib_flags_t FLAG_16 = (1 << 15);

TEST(TestFlags, test_flags)
{
    ib_flags_t flags;

    flags = 0x0;
    ib_flags_set(flags, FLAG_01);
    ASSERT_EQ(FLAG_01, flags);

    ib_flags_set(flags, FLAG_02);
    ASSERT_EQ(FLAG_01 | FLAG_02, flags);

    ib_flags_set(flags, FLAG_03);
    ASSERT_EQ(FLAG_01 | FLAG_02 | FLAG_03, flags);

    ib_flags_clear(flags, FLAG_01);
    ASSERT_EQ(FLAG_02 | FLAG_03, flags);

    ib_flags_set(flags, FLAG_04 | FLAG_05);
    ASSERT_EQ(FLAG_02 | FLAG_03 | FLAG_04 | FLAG_05, flags);

    ib_flags_clear(flags, FLAG_02 | FLAG_03);
    ASSERT_EQ(FLAG_04 | FLAG_05, flags);

    flags = (FLAG_01 | FLAG_02 | FLAG_03 | FLAG_04);
    ASSERT_TRUE (ib_flags_any(flags, FLAG_01 | FLAG_02));
    ASSERT_TRUE (ib_flags_any(flags, FLAG_01 | FLAG_02 | FLAG_03 | FLAG_04));
    ASSERT_TRUE (ib_flags_any(flags, FLAG_01 | FLAG_05));
    ASSERT_FALSE(ib_flags_any(flags, FLAG_05 | FLAG_06));

    ASSERT_TRUE (ib_flags_all(flags, FLAG_01));
    ASSERT_TRUE (ib_flags_all(flags, FLAG_01 | FLAG_02));
    ASSERT_TRUE (ib_flags_all(flags, FLAG_01 | FLAG_02 | FLAG_03 | FLAG_04));
    ASSERT_FALSE(ib_flags_all(flags, FLAG_01 | FLAG_05));
    ASSERT_FALSE(ib_flags_all(flags, FLAG_05 | FLAG_06));
}
