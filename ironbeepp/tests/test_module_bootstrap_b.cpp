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
 * @brief IronBee++ Internals --- Module Bootstrap Tests (B)
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/module_bootstrap.hpp>

#include "engine_private.h"

#include "fixture.hpp"

#include "gtest/gtest.h"

class TestModuleBootstrapB : public ::testing::Test, public IBPPTestFixture
{
};

static bool                s_delegate_destructed;
static bool                s_delegate_initialized;
static const ib_module_t*  s_ib_module;
static ib_context_t*       s_ib_context;

struct Delegate
{
    Delegate(IronBee::Module m)
    {
        s_delegate_initialized = true;
        s_ib_module = m.ib();
    }

    ~Delegate()
    {
        s_delegate_destructed = true;
    }
};

static const char* s_module_name = "test_module_bootstrap_b";

IBPP_BOOTSTRAP_MODULE_DELEGATE(s_module_name, Delegate);

TEST_F(TestModuleBootstrapB, basic)
{
    s_delegate_destructed      = false;
    s_delegate_initialized     = false;
    s_ib_module                = NULL;
    s_ib_context               = NULL;

    ib_module_t m = *IB_MODULE_SYM(m_engine.ib());

    EXPECT_EQ(s_module_name,         m.name);
    EXPECT_EQ(std::string(__FILE__), m.filename);
    EXPECT_EQ(m_engine.ib(),         m.ib);

    ib_status_t rc;

    s_delegate_initialized = false;
    rc = m.fn_init(
        m_engine.ib(),
        &m,
        m.cbdata_init
    );
    EXPECT_EQ(IB_OK, rc);
    EXPECT_TRUE(s_delegate_initialized);
    EXPECT_EQ(&m, s_ib_module);

    s_delegate_destructed = false;
    rc = m.fn_fini(
        m_engine.ib(),
        &m,
        m.cbdata_fini
    );
    ASSERT_TRUE(s_delegate_destructed);
}

