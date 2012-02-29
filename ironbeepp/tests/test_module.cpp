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
 * @brief IronBee++ Internals -- Module Tests
 * @internal
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#define IBPP_EXPOSE_C
#include <ironbeepp/module.hpp>
#include <ironbeepp/context.hpp>
#include <ironbeepp/internal/builder.hpp>
#include "fixture.hpp"

#include "gtest/gtest.h"

#include <ironbee/debug.h>

class TestModule : public ::testing::Test, public IBPPTestFixture
{
};

struct no_throw_tag {};

void test_callback_helper( no_throw_tag )
{
    // nop
}

template <typename ExceptionClass>
void test_callback_helper( ExceptionClass )
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
    test_callback(
        ib_module_t*&  out_ib_module,
        ib_context_t*& out_ib_context
    ) :
        m_out_ib_module( out_ib_module ),
        m_out_ib_context( out_ib_context )
    {
        // nop
    }

    void operator()( IronBee::Module module, IronBee::Context context )
    {
        m_out_ib_module  = module.ib();
        m_out_ib_context = context.ib();
        test_callback_helper( ExceptionClass() );
    }

    void operator()( IronBee::Module module )
    {
        m_out_ib_module  = module.ib();
        m_out_ib_context = NULL;
        test_callback_helper( ExceptionClass() );
    }

private:
    ib_module_t*&  m_out_ib_module;
    ib_context_t*& m_out_ib_context;
};

TEST_F( TestModule, basic )
{
    ib_module_t ib_module;
    ib_module.ib = m_ib_engine;
    IronBee::Module module = IronBee::Internal::Builder::module( &ib_module );

    ASSERT_EQ( &ib_module, module.ib() );
    ASSERT_EQ( m_ib_engine, module.engine().ib() );

    ib_module.vernum   = 1;
    ib_module.abinum   = 2;
    ib_module.version  = "hello";
    ib_module.filename = "foobar";
    ib_module.idx      = 3;
    ib_module.name     = "IAmModule";

    ASSERT_EQ( ib_module.vernum,   module.version_number() );
    ASSERT_EQ( ib_module.abinum,   module.abi_number()     );
    ASSERT_EQ( ib_module.version,  module.version()        );
    ASSERT_EQ( ib_module.filename, module.filename()       );
    ASSERT_EQ( ib_module.idx,      module.index()          );
    ASSERT_EQ( ib_module.name,     module.name()           );
}

TEST_F( TestModule, equality )
{
    ib_module_t ib_module;
    IronBee::Module a = IronBee::Internal::Builder::module( &ib_module );
    IronBee::Module b = IronBee::Internal::Builder::module( &ib_module );

    ASSERT_EQ( a, b );
}

TEST_F( TestModule, callbacks )
{
    ib_module_t ib_module;
    ib_module.ib = m_ib_engine;
    IronBee::Module module = IronBee::Internal::Builder::module( &ib_module );

    ib_module_t*  out_ib_module;
    ib_context_t* out_ib_context;

    ib_context_t ib_context;
    ib_status_t rc;

    module.set_initialize(
        test_callback<>( out_ib_module, out_ib_context )
    );
    out_ib_module = NULL;
    out_ib_context = NULL;
    rc = ib_module.fn_init(
        ib_module.ib,
        &ib_module,
        ib_module.cbdata_init
    );
    EXPECT_EQ( IB_OK, rc );
    EXPECT_EQ( &ib_module, out_ib_module );
    EXPECT_FALSE( out_ib_context );

    module.set_initialize(
        test_callback<IronBee::einval>( out_ib_module, out_ib_context )
    );
    out_ib_module = NULL;
    out_ib_context = NULL;
    rc = ib_module.fn_init(
        ib_module.ib,
        &ib_module,
        ib_module.cbdata_init
    );
    EXPECT_EQ( IB_EINVAL, rc );
    EXPECT_EQ( &ib_module, out_ib_module );
    EXPECT_FALSE( out_ib_context );

    module.set_finalize(
        test_callback<>( out_ib_module, out_ib_context )
    );
    out_ib_module = NULL;
    out_ib_context = NULL;
    rc = ib_module.fn_fini(
        ib_module.ib,
        &ib_module,
        ib_module.cbdata_fini
    );
    EXPECT_EQ( IB_OK, rc );
    EXPECT_EQ( &ib_module, out_ib_module );
    EXPECT_FALSE( out_ib_context );

    module.set_finalize(
        test_callback<IronBee::einval>( out_ib_module, out_ib_context )
    );
    out_ib_module = NULL;
    out_ib_context = NULL;
    rc = ib_module.fn_fini(
        ib_module.ib,
        &ib_module,
        ib_module.cbdata_fini
    );
    EXPECT_EQ( IB_EINVAL, rc );
    EXPECT_EQ( &ib_module, out_ib_module );
    EXPECT_FALSE( out_ib_context );

    module.set_context_open(
        test_callback<>( out_ib_module, out_ib_context )
    );
    out_ib_module = NULL;
    out_ib_context = NULL;
    rc = ib_module.fn_ctx_open(
        ib_module.ib,
        &ib_module,
        &ib_context,
        ib_module.cbdata_ctx_open
    );
    EXPECT_EQ( IB_OK, rc );
    EXPECT_EQ( &ib_module,  out_ib_module  );
    EXPECT_EQ( &ib_context, out_ib_context );

    module.set_context_open(
        test_callback<IronBee::einval>( out_ib_module, out_ib_context )
    );
    out_ib_module = NULL;
    out_ib_context = NULL;
    rc = ib_module.fn_ctx_open(
        ib_module.ib,
        &ib_module,
        &ib_context,
        ib_module.cbdata_ctx_open
    );
    EXPECT_EQ( IB_EINVAL, rc );
    EXPECT_EQ( &ib_module,  out_ib_module  );
    EXPECT_EQ( &ib_context, out_ib_context );

    module.set_context_close(
        test_callback<>( out_ib_module, out_ib_context )
    );
    out_ib_module = NULL;
    out_ib_context = NULL;
    rc = ib_module.fn_ctx_close(
        ib_module.ib,
        &ib_module,
        &ib_context,
        ib_module.cbdata_ctx_close
    );
    EXPECT_EQ( IB_OK, rc );
    EXPECT_EQ( &ib_module,  out_ib_module  );
    EXPECT_EQ( &ib_context, out_ib_context );

    module.set_context_close(
        test_callback<IronBee::einval>( out_ib_module, out_ib_context )
    );
    out_ib_module = NULL;
    out_ib_context = NULL;
    rc = ib_module.fn_ctx_close(
        ib_module.ib,
        &ib_module,
        &ib_context,
        ib_module.cbdata_ctx_close
    );
    EXPECT_EQ( IB_EINVAL, rc );
    EXPECT_EQ( &ib_module,  out_ib_module  );
    EXPECT_EQ( &ib_context, out_ib_context );

    module.set_context_destroy(
        test_callback<>( out_ib_module, out_ib_context )
    );
    out_ib_module = NULL;
    out_ib_context = NULL;
    rc = ib_module.fn_ctx_destroy(
        ib_module.ib,
        &ib_module,
        &ib_context,
        ib_module.cbdata_ctx_destroy
    );
    EXPECT_EQ( IB_OK, rc );
    EXPECT_EQ( &ib_module,  out_ib_module  );
    EXPECT_EQ( &ib_context, out_ib_context );

    module.set_context_destroy(
        test_callback<IronBee::einval>(
            out_ib_module,
            out_ib_context
        )
    );
    out_ib_module = NULL;
    out_ib_context = NULL; // prevent logging
    rc = ib_module.fn_ctx_destroy(
        ib_module.ib,
        &ib_module,
        &ib_context,
        ib_module.cbdata_ctx_destroy
    );
    EXPECT_EQ( IB_EINVAL, rc );
    EXPECT_EQ( &ib_module,  out_ib_module  );
    EXPECT_EQ( &ib_context, out_ib_context );
}

TEST_F( TestModule, boolness )
{
    IronBee::Module singular;

    EXPECT_FALSE( singular );

    ib_module_t ib_module;
    ib_module.ib = m_ib_engine;
    IronBee::Module nonsingular =
        IronBee::Internal::Builder::module( &ib_module );

    EXPECT_TRUE( nonsingular );
}

TEST_F( TestModule, expose_c )
{
    ib_module_t ib_module;
    IronBee::Module m = IronBee::Module::from_ib( &ib_module );

    ASSERT_TRUE( m );
    EXPECT_EQ( &ib_module, m.ib() );

    const IronBee::Module& cm = m;
    ASSERT_TRUE( cm );
    EXPECT_EQ( &ib_module, cm.ib() );
}
