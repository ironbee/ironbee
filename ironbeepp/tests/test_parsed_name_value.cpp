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
 * @brief IronBee++ Internals --- ParsedNameValue Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/parsed_name_value.hpp>
#include <ironbeepp/transaction.hpp>
#include <ironbeepp/byte_string.hpp>
#include <ironbeepp/memory_pool.hpp>

#include "gtest/gtest.h"

using namespace IronBee;

TEST(TestParsedNameValue, basic)
{
    MemoryPool mp = MemoryPool::create();

    ib_parsed_header_t ib_pnv;

    ParsedNameValue pnv(&ib_pnv);

    ASSERT_TRUE(pnv);

    ib_pnv.name = ByteString::create(mp, "foo").ib();
    EXPECT_EQ(ib_pnv.name, pnv.name().ib());

    ib_pnv.value = ByteString::create(mp, "bar").ib();
    EXPECT_EQ(ib_pnv.value, pnv.value().ib());

    ib_parsed_header_t ib_pnv2;
    ib_pnv.next = &ib_pnv2;
    EXPECT_EQ(ib_pnv.next, pnv.next().ib());
}

TEST(TestParsedNameValue, create)
{
    MemoryPool mp = MemoryPool::create();

    ParsedNameValue pnv = ParsedNameValue::create(
        mp,
        ByteString::create(mp, "foo"),
        ByteString::create(mp, "bar")
    );

    ASSERT_TRUE(pnv);
    EXPECT_EQ("foo", pnv.name().to_s());
    EXPECT_EQ("bar", pnv.value().to_s());
}
