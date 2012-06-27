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
 * @brief IronBee++ Internals &mdash; Module Bootstrap Tests (B)
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

static bool          s_delegate_constructed;
static bool          s_delegate_destructed;
static bool          s_delegate_initialized;
static bool          s_delegate_context_open;
static bool          s_delegate_context_close;
static bool          s_delegate_context_destroy;
static ib_module_t*  s_ib_module;
static ib_context_t* s_ib_context;

struct Delegate
{
    Delegate(IronBee::Module m)
    {
        s_delegate_constructed = true;
        s_ib_module = m.ib();
    }

    ~Delegate()
    {
        s_delegate_destructed = true;
    }

    void initialize()
    {
        s_delegate_initialized = true;
    }

    void context_open(IronBee::Context c)
    {
        s_delegate_context_open = true;
        s_ib_context = c.ib();
    }

    void context_close(IronBee::Context c)
    {
        s_delegate_context_close = true;
        s_ib_context = c.ib();
    }

    void context_destroy(IronBee::Context c)
    {
        s_delegate_context_destroy = true;
        s_ib_context = c.ib();
    }
};

static const char* s_module_name = "test_module_bootstrap_b";

IBPP_BOOTSTRAP_MODULE_DELEGATE(s_module_name, Delegate);

TEST_F(TestModuleBootstrapB, basic)
{
    s_delegate_constructed     = false;
    s_delegate_destructed      = false;
    s_delegate_initialized     = false;
    s_delegate_context_open    = false;
    s_delegate_context_close   = false;
    s_delegate_context_destroy = false;
    s_ib_module                = NULL;
    s_ib_context               = NULL;

    ib_module_t* m = IB_MODULE_SYM(m_engine.ib());

    EXPECT_TRUE(s_delegate_constructed);
    EXPECT_EQ(m, s_ib_module);
    EXPECT_EQ(s_module_name,         m->name);
    EXPECT_EQ(std::string(__FILE__), m->filename);
    EXPECT_EQ(m_engine.ib(),           m->ib);

    ib_context_t c;
    ib_status_t rc;

    s_delegate_initialized = false;
    rc = m->fn_init(
        m_engine.ib(),
        m,
        m->cbdata_init
    );
    EXPECT_EQ(IB_OK, rc);
    EXPECT_TRUE(s_delegate_initialized);

    s_delegate_context_open = false;
    s_ib_context = NULL;
    rc = m->fn_ctx_open(
        m_engine.ib(),
        m,
        &c,
        m->cbdata_ctx_open
    );
    EXPECT_EQ(IB_OK, rc);
    EXPECT_TRUE(s_delegate_context_open);
    EXPECT_EQ(&c, s_ib_context);

    s_delegate_context_close = false;
    s_ib_context = NULL;
    rc = m->fn_ctx_close(
        m_engine.ib(),
        m,
        &c,
        m->cbdata_ctx_close
    );
    EXPECT_EQ(IB_OK, rc);
    EXPECT_TRUE(s_delegate_context_close);
    EXPECT_EQ(&c, s_ib_context);

    s_delegate_context_destroy = false;
    s_ib_context = NULL;
    rc = m->fn_ctx_destroy(
        m_engine.ib(),
        m,
        &c,
        m->cbdata_ctx_destroy
    );
    EXPECT_EQ(IB_OK, rc);
    EXPECT_TRUE(s_delegate_context_destroy);
    EXPECT_EQ(&c, s_ib_context);

    s_delegate_destructed = false;
    rc = m->fn_fini(
        m_engine.ib(),
        m,
        m->cbdata_fini
    );
    ASSERT_TRUE(s_delegate_destructed);
}

