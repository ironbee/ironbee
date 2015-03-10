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
 * @brief IronBee++ Internals --- Module Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/module.hpp>
#include <ironbeepp/test_fixture.hpp>

#include "gtest/gtest.h"

#include "engine_private.h"

class TestModule : public ::testing::Test, public IronBee::TestFixture
{
};

struct no_throw_tag {};

void test_callback_helper(no_throw_tag)
{
    // nop
}

template <typename ExceptionClass>
void test_callback_helper(ExceptionClass)
{
    BOOST_THROW_EXCEPTION(
        ExceptionClass() << IronBee::errinfo_what(
            "Intentional test exception."
        )
    );
}

template <typename ExceptionClass = no_throw_tag>
class test_callback
{
public:
    explicit
    test_callback(
        ib_module_t*&  out_ib_module
    ) :
        m_out_ib_module(out_ib_module)
    {
        // nop
    }

    void operator()(IronBee::Module module)
    {
        m_out_ib_module  = module.ib();
        test_callback_helper(ExceptionClass());
    }

private:
    ib_module_t*&  m_out_ib_module;
};

TEST_F(TestModule, init)
{
    ib_module_t* ib_module;
    ASSERT_EQ(IB_OK, ib_module_create(&ib_module, m_engine.ib()));
    ib_module->vernum = IB_VERNUM;
    ib_module->abinum = IB_ABINUM;
    ASSERT_EQ(IB_OK, ib_module_register(ib_module, m_engine.ib()));
}

TEST_F(TestModule, basic)
{
    ib_module_t ib_module;
    ib_module.ib = m_engine.ib();
    IronBee::Module module(&ib_module);

    ASSERT_EQ(&ib_module, module.ib());
    ASSERT_EQ(m_engine, module.engine());

    ib_module.vernum   = 1;
    ib_module.abinum   = 2;
    ib_module.version  = "hello";
    ib_module.filename = "foobar";
    ib_module.idx      = 3;
    ib_module.name     = "IAmModule";

    ASSERT_EQ(ib_module.vernum,   module.version_number());
    ASSERT_EQ(ib_module.abinum,   module.abi_number());
    ASSERT_EQ(ib_module.version,  module.version());
    ASSERT_EQ(ib_module.filename, module.filename());
    ASSERT_EQ(ib_module.idx,      module.index());
    ASSERT_EQ(ib_module.name,     module.name());
}

TEST_F(TestModule, equality)
{
    ib_module_t ib_module;
    IronBee::Module a(&ib_module);
    IronBee::Module b(&ib_module);

    ASSERT_EQ(a, b);
}

TEST_F(TestModule, callbacks)
{
    ib_module_t ib_module;
    ib_module.ib = m_engine.ib();
    IronBee::Module module(&ib_module);

    ib_module_t*  out_ib_module;

    ib_status_t rc;

    module.set_initialize(
        test_callback<>(out_ib_module)
    );
    out_ib_module = NULL;
    rc = ib_module.fn_init(
        ib_module.ib,
        &ib_module,
        ib_module.cbdata_init
    );
    EXPECT_EQ(IB_OK, rc);
    EXPECT_EQ(&ib_module, out_ib_module);

    module.set_initialize(
        test_callback<IronBee::einval>(out_ib_module)
    );
    out_ib_module = NULL;
    rc = ib_module.fn_init(
        ib_module.ib,
        &ib_module,
        ib_module.cbdata_init
    );
    EXPECT_EQ(IB_EINVAL, rc);
    EXPECT_EQ(&ib_module, out_ib_module);

    module.set_finalize(
        test_callback<>(out_ib_module)
    );
    out_ib_module = NULL;
    rc = ib_module.fn_fini(
        ib_module.ib,
        &ib_module,
        ib_module.cbdata_fini
    );
    EXPECT_EQ(IB_OK, rc);
    EXPECT_EQ(&ib_module, out_ib_module);

    module.set_finalize(
        test_callback<IronBee::einval>(out_ib_module)
    );
    out_ib_module = NULL;
    rc = ib_module.fn_fini(
        ib_module.ib,
        &ib_module,
        ib_module.cbdata_fini
    );
    EXPECT_EQ(IB_EINVAL, rc);
    EXPECT_EQ(&ib_module, out_ib_module);

    module.set_initialize(IronBee::Module::module_callback_t());
    EXPECT_FALSE(ib_module.fn_init);
    EXPECT_FALSE(ib_module.cbdata_init);
    module.set_finalize(IronBee::Module::module_callback_t());
    EXPECT_FALSE(ib_module.fn_fini);
    EXPECT_FALSE(ib_module.cbdata_fini);
}

TEST_F(TestModule, operators)
{
    IronBee::Module singular1;
    IronBee::Module singular2;

    ib_module_t ib_module1;
    ib_module_t ib_module2;
    ib_module1.ib = m_engine.ib();
    ib_module2.ib = m_engine.ib();
    IronBee::Module nonsingular1(&ib_module1);
    IronBee::Module nonsingular2(&ib_module2);

    EXPECT_FALSE(singular1);
    EXPECT_FALSE(singular2);
    EXPECT_TRUE(nonsingular1);
    EXPECT_TRUE(nonsingular2);

    EXPECT_EQ(singular1, singular2);
    EXPECT_NE(nonsingular1, nonsingular2);
    EXPECT_NE(singular1, nonsingular1);

    EXPECT_LT(singular1, nonsingular1);
    EXPECT_FALSE(singular1 < singular2);
}

TEST_F(TestModule, expose_c)
{
    ib_module_t ib_module;
    IronBee::Module m(&ib_module);

    ASSERT_TRUE(m);
    EXPECT_EQ(&ib_module, m.ib());

    const IronBee::Module& cm = m;
    ASSERT_TRUE(cm);
    EXPECT_EQ(&ib_module, cm.ib());
}

class SimpleTestCallback
{
public:
    SimpleTestCallback(int& id) :
        m_id( id )
    {
        m_id = -1;
    }

    void operator()(IronBee::Module)
    {
        m_id = s_next_id;
        ++s_next_id;
    }

    void operator()(IronBee::Module, IronBee::Context)
    {
        (*this)(IronBee::Module());
    }

    static void reset()
    {
        s_next_id = 0;
    }

private:
    static int s_next_id;
    int& m_id;
};
int SimpleTestCallback::s_next_id = 0;

TEST_F(TestModule, chain)
{
    int a;
    int b;
    int c;

    ib_module_t ib_module;
    memset(&ib_module, 0, sizeof(ib_module));
    ib_module.ib = m_engine.ib();
    IronBee::Module module(&ib_module);

    ib_status_t rc;

    SimpleTestCallback::reset();
    module.chain_initialize(SimpleTestCallback(a));
    module.chain_initialize(SimpleTestCallback(b));
    module.chain_initialize(SimpleTestCallback(c));
    ASSERT_EQ(-3, a + b + c);
    rc = ib_module.fn_init(
        ib_module.ib,
        &ib_module,
        ib_module.cbdata_init
    );
    EXPECT_EQ(IB_OK, rc);
    ASSERT_EQ(0, a);
    ASSERT_EQ(1, b);
    ASSERT_EQ(2, c);

    SimpleTestCallback::reset();
    module.set_initialize(SimpleTestCallback(a));
    module.prechain_initialize(SimpleTestCallback(b));
    module.chain_initialize(SimpleTestCallback(c));
    ASSERT_EQ(-3, a + b + c);
    rc = ib_module.fn_init(
        ib_module.ib,
        &ib_module,
        ib_module.cbdata_init
    );
    EXPECT_EQ(IB_OK, rc);
    ASSERT_EQ(1, a);
    ASSERT_EQ(0, b);
    ASSERT_EQ(2, c);
}

struct test_data_t
{
    int x;
};

void test_data_copier(
    IronBee::Module,
    test_data_t&       dst,
    const test_data_t& src
)
{
    dst.x = src.x + 1;
}

TEST_F(TestModule, DataPOD)
{
    test_data_t data;
    data.x = 17;

    ib_module_t* ib_module;        /* The module template. */
    ib_module_t* ib_engine_module; /* The module in the engine. */
    ASSERT_EQ(IB_OK, ib_module_create(&ib_module, m_engine.ib()));
    ib_module->name = "A NAME";
    ib_module->vernum = IB_VERNUM;
    ib_module->abinum = IB_ABINUM;
    ASSERT_EQ(IB_OK, ib_module_register(ib_module, m_engine.ib()));

    ib_engine_module_get(m_engine.ib(), ib_module->name, &ib_engine_module);
    IronBee::Module module(ib_engine_module);

    module.set_configuration_data_pod(data, test_data_copier);

    test_data_t* other = reinterpret_cast<test_data_t*>(ib_engine_module->gcdata);
    ASSERT_EQ(data.x, other->x);
    ASSERT_EQ(sizeof(data), ib_engine_module->gclen);

    ib_status_t rc;
    test_data_t other2;
    rc = ib_engine_module->fn_cfg_copy(
        ib_engine_module->ib,
        ib_engine_module,
        &other2,
        ib_engine_module->gcdata,
        ib_engine_module->gclen,
        ib_engine_module->cbdata_cfg_copy
    );
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(data.x + 1, other2.x);
}

TEST_F(TestModule, DataPOD2)
{
    test_data_t data;
    ib_module_t* ib_module;        /* The module template. */
    ib_module_t* ib_engine_module; /* The module in the engine. */
    ASSERT_EQ(IB_OK, ib_module_create(&ib_module, m_engine.ib()));
    ib_module->name = "A NAME";
    ib_module->vernum = IB_VERNUM;
    ib_module->abinum = IB_ABINUM;
    ASSERT_EQ(IB_OK, ib_module_register(ib_module, m_engine.ib()));
    ib_engine_module_get(m_engine.ib(), ib_module->name, &ib_engine_module);
    IronBee::Module module(ib_engine_module);

    module.set_configuration_data_pod(data);
    ASSERT_FALSE(ib_engine_module->fn_cfg_copy);
    ASSERT_FALSE(ib_engine_module->cbdata_cfg_copy);
}

struct test_data_cpp_t
{
    test_data_cpp_t() : x(17) {}
    test_data_cpp_t(const test_data_cpp_t& other) : x(other.x+1) {}

    int x;
};

TEST_F(TestModule, DataCPP)
{
    test_data_cpp_t data;

    ib_module_t* ib_module;        /* The module template. */
    ib_module_t* ib_engine_module; /* The module in the engine. */
    ASSERT_EQ(IB_OK, ib_module_create(&ib_module, m_engine.ib()));
    ib_module->name = "A NAME";
    ib_module->vernum = IB_VERNUM;
    ib_module->abinum = IB_ABINUM;
    ASSERT_EQ(IB_OK, ib_module_register(ib_module, m_engine.ib()));
    ib_engine_module_get(m_engine.ib(), ib_module->name, &ib_engine_module);
    IronBee::Module module(ib_engine_module);

    module.set_configuration_data(data);

    test_data_cpp_t* other =
        *reinterpret_cast<test_data_cpp_t**>(ib_engine_module->gcdata);
    ASSERT_EQ(data.x+1, other->x);

    ib_status_t rc;
    test_data_cpp_t* other2;
    rc = ib_engine_module->fn_cfg_copy(
        ib_engine_module->ib,
        ib_engine_module,
        &other2,
        ib_engine_module->gcdata,
        ib_engine_module->gclen,
        ib_engine_module->cbdata_cfg_copy
    );
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(data.x + 2, other2->x);
}

TEST_F(TestModule, Const)
{
    ib_module_t ib_module;
    ib_module.ib = m_engine.ib();
    IronBee::Module module(&ib_module);

    IronBee::ConstModule cmodule = module;

    EXPECT_EQ(cmodule, module);

    IronBee::Module module2 = IronBee::Module::remove_const(cmodule);

    EXPECT_EQ(cmodule, module2);
    EXPECT_EQ(module, module2);
}

// Complete ConfigurationMap tests are in test_configuration_map.
TEST_F(TestModule, ConfigurationMap)
{
    ib_module_t* ib_module;        /* The module template. */
    ib_module_t* ib_engine_module; /* The module in the engine. */
    ASSERT_EQ(IB_OK, ib_module_create(&ib_module, m_engine.ib()));
    ib_module->name = "A NAME";
    ib_module->vernum = IB_VERNUM;
    ib_module->abinum = IB_ABINUM;
    ASSERT_EQ(IB_OK, ib_module_register(ib_module, m_engine.ib()));
    ib_engine_module_get(m_engine.ib(), ib_module->name, &ib_engine_module);
    IronBee::Module module(ib_engine_module);

    test_data_t data;

    module.set_configuration_data_pod(data)
          .number("x", &test_data_t::x)
          ;

    ASSERT_TRUE(ib_engine_module->cm_init);
    EXPECT_FALSE(ib_engine_module->cm_init[1].name);
    EXPECT_EQ(std::string("x"), ib_engine_module->cm_init[0].name);
}
