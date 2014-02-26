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
 * @brief IronBee++ Internals --- Engine Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/engine.hpp>
#include <ironbeepp/ironbee.hpp>
#include <ironbeepp/server.hpp>
#include <ironbeepp/memory_pool.hpp>

#include <ironbeepp/test_fixture.hpp>
#include "gtest/gtest.h"

using namespace IronBee;

class TestEngine : public ::testing::Test, public TestFixture
{
};

TEST(TestEngineNoFixture, create)
{
    IronBee::initialize();
    IronBee::ServerValue server_value("filename", "name");

    Engine engine = Engine::create(server_value.get());

    ASSERT_TRUE(engine);
    engine.destroy();

    IronBee::shutdown();
}

TEST_F(TestEngine, memory_pools)
{
    ASSERT_TRUE(m_engine.main_memory_mm());
    ASSERT_TRUE(m_engine.configuration_memory_mm());
    ASSERT_TRUE(m_engine.temporary_memory_mm());
}
