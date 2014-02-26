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
 * @brief IronBee++ Internals --- Memory Pool Lite Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/memory_pool_lite.hpp>
#include <ironbeepp/exception.hpp>
#include <ironbeepp/test_fixture.hpp>

#include "gtest/gtest.h"

#include <string>

using namespace std;

class TestMemoryPoolLite : public ::testing::Test, public IronBee::TestFixture
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

TEST_F(TestMemoryPoolLite, create)
{
    IronBee::MemoryPoolLite m;

    m = IronBee::MemoryPoolLite::create();
    EXPECT_TRUE(m);
    m.destroy();
}

TEST_F(TestMemoryPoolLite, basic)
{
    IronBee::MemoryPoolLite m = IronBee::MemoryPoolLite::create();
    char* p;

    p = reinterpret_cast<char*>(m.alloc(20));
    EXPECT_TRUE(p);

    int* ip = m.allocate<int>(2);
    EXPECT_TRUE(ip);

    EXPECT_NO_THROW(m.destroy());
}

TEST_F(TestMemoryPoolLite, callbacks)
{
    bool called_flag = false;

    IronBee::MemoryPoolLite m = IronBee::MemoryPoolLite::create();
    m.register_cleanup(test_callback(called_flag));

    m.destroy();
    EXPECT_TRUE(called_flag);
}

TEST_F(TestMemoryPoolLite, boolness)
{
    IronBee::MemoryPoolLite singular;

    EXPECT_FALSE(singular);

    ib_mpool_lite_t* ib_memory_pool = reinterpret_cast<ib_mpool_lite_t*>(1);
    IronBee::MemoryPoolLite nonsingular(ib_memory_pool);

    EXPECT_TRUE(nonsingular);
}

TEST_F(TestMemoryPoolLite, expose_c)
{
    ib_mpool_lite_t* ib_memory_pool = reinterpret_cast<ib_mpool_lite_t*>(1);
    IronBee::MemoryPoolLite m(ib_memory_pool);

    ASSERT_TRUE(m);
    EXPECT_EQ(ib_memory_pool, m.ib());

    const IronBee::MemoryPoolLite& cm = m;
    ASSERT_TRUE(cm);
    EXPECT_EQ(ib_memory_pool, cm.ib());
}

TEST_F(TestMemoryPoolLite, scoped)
{
    bool called_flag = false;
    {
        IronBee::ScopedMemoryPoolLite scoped;

        IronBee::MemoryPoolLite m = scoped;
        EXPECT_TRUE(m);

        m.register_cleanup(test_callback(called_flag));
    }
    EXPECT_TRUE(called_flag);
}

TEST_F(TestMemoryPoolLite, Const)
{
    IronBee::MemoryPoolLite m = IronBee::MemoryPoolLite::create();
    IronBee::ConstMemoryPoolLite cm = m;

    EXPECT_EQ(cm, m);

    IronBee::MemoryPoolLite m2 = IronBee::MemoryPoolLite::remove_const(cm);

    EXPECT_EQ(cm, m2);
    EXPECT_EQ(m, m2);
}
