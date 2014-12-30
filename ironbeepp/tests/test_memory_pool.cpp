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
 * @brief IronBee++ Internals --- Memory Pool Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/memory_pool.hpp>
#include <ironbeepp/exception.hpp>
#include <ironbeepp/test_fixture.hpp>

#include "gtest/gtest.h"

#include <string>

using namespace std;

class TestMemoryPool : public ::testing::Test, public IronBee::TestFixture
{
};

class test_callback
{
public:
    test_callback(bool& called_flag) :
        m_called_flag(called_flag)
    {
        // nop
    }

    void operator()()
    {
        m_called_flag = true;
    }

private:
    bool& m_called_flag;
};

TEST_F(TestMemoryPool, create)
{
    IronBee::MemoryPool m;

    m = IronBee::MemoryPool::create();
    EXPECT_EQ(string("MemoryPool"), m.name());
    m.destroy();

    EXPECT_NO_THROW(m = IronBee::MemoryPool::create("Hello World"));
    EXPECT_EQ(string("Hello World"), m.name());

    IronBee::MemoryPool m2;
    EXPECT_NO_THROW(m2 = IronBee::MemoryPool::create("WorldChild", m));
    EXPECT_EQ(string("WorldChild"), m2.name());
    m2.destroy();

    EXPECT_NO_THROW(m2 = m.create_subpool());
    EXPECT_EQ(string("SubPool"), m2.name());
    m2.destroy();

    EXPECT_NO_THROW(m2 = m.create_subpool("WorldChild2"));
    EXPECT_EQ(string("WorldChild2"), m2.name());
    m2.destroy();
    m.destroy();
}

TEST_F(TestMemoryPool, basic)
{
    IronBee::MemoryPool m = IronBee::MemoryPool::create();
    char* p;

    p = reinterpret_cast<char*>(m.alloc(20));
    EXPECT_TRUE(p);

    int* ip = m.allocate<int>(2);
    EXPECT_TRUE(ip);

    // With no introspection, testing is limited.
    EXPECT_NO_THROW(m.clear());
    EXPECT_NO_THROW(m.destroy());
}

TEST_F(TestMemoryPool, callbacks)
{
    bool called_flag = false;

    IronBee::MemoryPool m = IronBee::MemoryPool::create();
    m.register_cleanup(test_callback(called_flag));

    ASSERT_FALSE(called_flag);
    m.clear();
    EXPECT_TRUE(called_flag);
    m.destroy();
    EXPECT_TRUE(called_flag);
}

TEST_F(TestMemoryPool, boolness)
{
    IronBee::MemoryPool singular;

    EXPECT_FALSE(singular);

    ib_mpool_t* ib_memory_pool = reinterpret_cast<ib_mpool_t*>(1);
    IronBee::MemoryPool nonsingular(ib_memory_pool);

    EXPECT_TRUE(nonsingular);
}

TEST_F(TestMemoryPool, expose_c)
{
    ib_mpool_t* ib_memory_pool = reinterpret_cast<ib_mpool_t*>(1);
    IronBee::MemoryPool m(ib_memory_pool);

    ASSERT_TRUE(m);
    EXPECT_EQ(ib_memory_pool, m.ib());

    const IronBee::MemoryPool& cm = m;
    ASSERT_TRUE(cm);
    EXPECT_EQ(ib_memory_pool, cm.ib());
}

TEST_F(TestMemoryPool, scoped)
{
    bool called_flag = false;
    {
        IronBee::ScopedMemoryPool scoped;

        IronBee::MemoryPool m = scoped;
        EXPECT_TRUE(m);
        EXPECT_EQ(string("ScopedMemoryPool"), m.name());

        m.register_cleanup(test_callback(called_flag));
    }
    EXPECT_TRUE(called_flag);

    called_flag = false;
    {
        IronBee::ScopedMemoryPool scoped("Hello World");

        IronBee::MemoryPool m = scoped;
        EXPECT_TRUE(m);
        EXPECT_EQ(string("Hello World"), m.name());

        m.register_cleanup(test_callback(called_flag));
    }
    EXPECT_TRUE(called_flag);
}

TEST_F(TestMemoryPool, Const)
{
    IronBee::MemoryPool m = IronBee::MemoryPool::create();
    IronBee::ConstMemoryPool cm = m;

    EXPECT_EQ(cm, m);

    IronBee::MemoryPool m2 = IronBee::MemoryPool::remove_const(cm);

    EXPECT_EQ(cm, m2);
    EXPECT_EQ(m, m2);

    m.destroy();
}
