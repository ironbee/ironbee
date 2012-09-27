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
 * @brief IronAutomata --- Bits test.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironautomata/bits.h>

#include "gtest/gtest.h"

TEST(TestBits, get)
{
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    uint8_t bytes[10];

    u8 = 0x12;
    for (int i = 0; i < 8; ++i) {
        EXPECT_EQ((u8 & (1 << i)) != 0, ia_bit8(u8, i));
    }

    u16 = 0x1234;
    for (int i = 0; i < 16; ++i) {
        EXPECT_EQ((u16 & (1 << i)) != 0, ia_bit16(u16, i));
    }

    u32 = 0x12345678;
    for (int i = 0; i < 32; ++i) {
        EXPECT_EQ((u32 & (1 << i)) != 0, ia_bit32(u32, i));
    }

    u64 = 0x123456789abcdef0;
    for (int i = 0; i < 32; ++i) {
        EXPECT_EQ((u64 & (1 << i)) != 0, ia_bit64(u64, i));
    }

    bytes[0] = 13;
    for (int i = 1; i < 10; ++i) {
        bytes[i] = (bytes[i-1]+3)*7;
    }
    for (int i = 0; i < 80; ++i) {
        EXPECT_EQ(ia_bit8(bytes[i / 8], i % 8), ia_bitv(bytes, i));
    }
}

TEST(TestBits, set)
{
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    uint8_t bytes[10];
    uint8_t other_bytes[10];

    u8 = 0x12;
    for (int i = 0; i < 8; ++i) {
        u8 |= (1 << i);
        EXPECT_EQ(u8, ia_setbit8(u8, i));
    }

    u16 = 0x1234;
    for (int i = 0; i < 16; ++i) {
        u16 |= (1 << i);
        EXPECT_EQ(u16, ia_setbit16(u16, i));
    }

    u32 = 0x12345678;
    for (int i = 0; i < 32; ++i) {
        u32 |= (1 << i);
        EXPECT_EQ(u32, ia_setbit32(u32, i));
    }

    u64 = 0x123456789abcdef0;
    for (int i = 0; i < 64; ++i) {
        u64 |= (1 << i);
        EXPECT_EQ(u64, ia_setbit64(u64, i));
    }

    bytes[0] = 13;
    for (int i = 1; i < 10; ++i) {
        bytes[i] = (bytes[i-1]+3)*7;
    }
    memcpy(other_bytes, bytes, 10);
    for (int i = 0; i < 80; ++i) {
        bytes[i / 8] |= (1 << (i % 8));
        ia_setbitv(other_bytes, i);
        EXPECT_EQ(bytes[i / 8], other_bytes[i / 8]);
    }
}
