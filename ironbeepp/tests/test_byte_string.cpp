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
 * @brief IronBee++ Internals --- Byte String Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/byte_string.hpp>
#include <ironbeepp/memory_pool.hpp>
#include <ironbeepp/exception.hpp>

#include "gtest/gtest.h"

#include <ironbee/bytestr.h>

#include <string>
#include <sstream>

using namespace std;
using IronBee::ByteString;
using IronBee::ConstByteString;
using IronBee::MemoryPool;

class TestByteString : public ::testing::Test
{
public:
    TestByteString()
    {
        m_pool = MemoryPool::create();
    }

protected:
    MemoryPool m_pool;
};

TEST_F(TestByteString, Construction)
{
    ByteString bs;
    bs = ByteString::create(m_pool);
    EXPECT_TRUE(bs.ib());
    EXPECT_EQ(0UL, bs.length());

    bs = ByteString::create(m_pool, "test1");
    EXPECT_EQ("test1", bs.to_s());

    bs = ByteString::create(m_pool, string("test2"));
    EXPECT_EQ("test2", bs.to_s());

    static const char* static_data1 = "foobar1";
    static const char* static_data2 = "foobar2";

    bs = ByteString::create_alias(m_pool, static_data1, 3);
    EXPECT_EQ("foo", bs.to_s());
    EXPECT_EQ(static_data1, bs.const_data());
    EXPECT_TRUE(bs.read_only());

    bs = ByteString::create_alias(m_pool, static_data2);
    EXPECT_EQ("foobar2", bs.to_s());
    EXPECT_EQ(static_data2, bs.const_data());
    EXPECT_TRUE(bs.read_only());

    ByteString bs2 = bs.alias();
    EXPECT_EQ(bs.const_data(), bs2.const_data());
    EXPECT_TRUE(bs2.read_only());

    MemoryPool other_pool = MemoryPool::create();
    bs2 = bs.alias(other_pool);
    EXPECT_EQ(bs.const_data(), bs2.const_data());
    EXPECT_TRUE(bs2.read_only());

    ByteString bs3 = bs.dup();
    EXPECT_EQ(bs.to_s(), bs3.to_s());
    EXPECT_NE(bs.const_data(), bs3.const_data());
    EXPECT_FALSE(bs3.read_only());

    bs3 = bs.dup(other_pool);
    EXPECT_EQ(bs.to_s(), bs3.to_s());
    EXPECT_NE(bs.const_data(), bs3.const_data());
    EXPECT_FALSE(bs3.read_only());
}

TEST_F(TestByteString, Queries)
{
    ByteString bs = ByteString::create(m_pool);

    EXPECT_EQ("", bs.to_s());
    EXPECT_FALSE(bs.read_only());
    EXPECT_EQ(0UL, bs.length());
    EXPECT_EQ(0UL, bs.size());
}

TEST_F(TestByteString, ReadOnly)
{
    ByteString bs = ByteString::create(m_pool, "testdata");
    EXPECT_FALSE(bs.read_only());
    EXPECT_EQ("testdata", bs.to_s());
    EXPECT_TRUE(bs.data());
    EXPECT_TRUE(bs.const_data());

    bs.make_read_only();

    EXPECT_TRUE(bs.read_only());
    EXPECT_FALSE(bs.data());
    EXPECT_TRUE(bs.const_data());

    EXPECT_THROW(bs.append("foobar"), IronBee::einval);
    EXPECT_THROW(bs.append(string("foobar")), IronBee::einval);
    EXPECT_THROW(bs.append("foobar", 6), IronBee::einval);
    EXPECT_THROW(bs.append(bs.dup()), IronBee::einval);

    EXPECT_FALSE(bs.dup().read_only());
    EXPECT_TRUE(bs.alias().read_only());

    char other_data[10] = "other";
    bs.set(other_data);
    EXPECT_EQ("other", bs.to_s());
    EXPECT_FALSE(bs.read_only());
}

TEST_F(TestByteString, Set)
{
    ByteString bs = ByteString::create(m_pool);

    char rwdata[20] = "read-write";
    const char* rdata = "only-read";

    bs.set(rwdata, 4);
    EXPECT_FALSE(bs.read_only());
    EXPECT_EQ("read", bs.to_s());

    bs.set(rdata, 4);
    EXPECT_TRUE(bs.read_only());
    EXPECT_EQ("only", bs.to_s());

    bs.set(rwdata);
    EXPECT_FALSE(bs.read_only());
    EXPECT_EQ("read-write", bs.to_s());

    bs.set(rdata);
    EXPECT_TRUE(bs.read_only());
    EXPECT_EQ("only-read", bs.to_s());

    bs.set(string("foobar"));
    EXPECT_TRUE(bs.read_only());
    EXPECT_EQ("foobar", bs.to_s());
}

TEST_F(TestByteString, Append)
{
    ByteString bs = ByteString::create(m_pool, "Prefix");
    ByteString bs2;

    bs2 = bs.dup();
    bs2.append(ByteString::create(m_pool, "Suffix1"));
    EXPECT_EQ("PrefixSuffix1", bs2.to_s());

    bs2 = bs.dup();
    bs2.append("Suffix2...", 7);
    EXPECT_EQ("PrefixSuffix2", bs2.to_s());

    bs2 = bs.dup();
    bs2.append("Suffix3");
    EXPECT_EQ("PrefixSuffix3", bs2.to_s());

    bs2 = bs.dup();
    bs2.append(string("Suffix4"));
    EXPECT_EQ("PrefixSuffix4", bs2.to_s());
}

TEST_F(TestByteString, IndexOf)
{
    ByteString bs = ByteString::create(m_pool, "FooBar");

    EXPECT_EQ(-1, bs.index_of("hello"));
    EXPECT_EQ(-1, bs.index_of(string("hello")));

    EXPECT_EQ(2, bs.index_of("oBa"));
    EXPECT_EQ(2, bs.index_of(string("oBa")));

    bs.make_read_only();
    EXPECT_EQ(2, bs.index_of("oBa"));
}

TEST_F(TestByteString, Operators)
{
    ByteString singular1;
    ByteString singular2;
    ByteString nonsingular1 = ByteString::create(m_pool);
    ByteString nonsingular2 = ByteString::create(m_pool);

    EXPECT_FALSE(singular1);
    EXPECT_FALSE(singular2);
    EXPECT_TRUE(nonsingular1);
    EXPECT_TRUE(nonsingular2);

    EXPECT_EQ(singular1, singular2);
    EXPECT_NE(nonsingular1, nonsingular2);
    EXPECT_NE(singular1, nonsingular1);

    EXPECT_LT(singular1, nonsingular1);
    EXPECT_FALSE(singular1 < singular2);

    nonsingular1.set("foobar");
    {
        stringstream s;
        s << nonsingular1;
        EXPECT_EQ("IronBee::ByteString[foobar]", s.str());
    }
    {
        stringstream s;
        s << singular1;
        EXPECT_EQ("IronBee::ByteString[!singular!]", s.str());
    }
}

TEST_F(TestByteString, ExposeC)
{
    ib_bytestr_t* ib_bs;
    ib_bytestr_create(&ib_bs, ib_mm_mpool(m_pool.ib()), 10);

    ASSERT_TRUE(ib_bs);

    ByteString bs(ib_bs);
    EXPECT_TRUE(bs);
    EXPECT_EQ(ib_bs, bs.ib());

    const ByteString& cbs = bs;
    EXPECT_EQ(ib_bs, cbs.ib());
}

TEST_F(TestByteString, Const)
{
    ConstByteString cbs = ByteString::create(m_pool, "data");
    EXPECT_TRUE(cbs.ib());
    ByteString bs = cbs.dup();
    EXPECT_EQ(cbs.to_s(),bs.to_s());
    ConstByteString cbs2 = bs;
    EXPECT_EQ(cbs2, bs);
    ByteString bs2 = ByteString::remove_const(cbs2);
    EXPECT_EQ(cbs2, bs2);
}
