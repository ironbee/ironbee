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
 * @brief IronBee++ Internals -- Module Bootstrap Tests (A)
 * @internal
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#define IBPP_EXPOSE_C
#include <ironbeepp/module_bootstrap.hpp>

#include "fixture.hpp"

#include "gtest/gtest.h"

class TestModuleBootstrapA : public ::testing::Test, public IBPPTestFixture
{
};

static ib_module_t* g_test_module;

void on_load(IronBee::Module m)
{
    g_test_module = m.ib();
}

static const char* s_module_name = "test_module_bootstrap_a";

IBPP_BOOTSTRAP_MODULE(s_module_name, on_load);

TEST_F(TestModuleBootstrapA, basic)
{
    g_test_module = NULL;

    ib_module_t* m = IB_MODULE_SYM(m_ib_engine);

    ASSERT_EQ(m,                     g_test_module);
    ASSERT_EQ(s_module_name,         m->name);
    ASSERT_EQ(std::string(__FILE__), m->filename);
    ASSERT_EQ(m_ib_engine,           m->ib);
}
