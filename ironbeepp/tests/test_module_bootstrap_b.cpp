#define IBPP_EXPOSE_C
#include <ironbeepp/module_bootstrap.hpp>

#include "fixture.hpp"

#include "gtest/gtest.h"

class TestModuleBootstrapB : public ::testing::Test, public IBPPTestFixture
{
};

static bool          s_delegate_constructed;
static bool          s_delegate_destructed;
static bool          s_delegate_context_open;
static bool          s_delegate_context_close;
static bool          s_delegate_context_destroy;
static ib_module_t*  s_ib_module;
static ib_context_t* s_ib_context;

struct Delegate
{
    Delegate( IronBee::Module m )
    {
        s_delegate_constructed = true;
        s_ib_module = m.ib();
    }

    ~Delegate()
    {
        s_delegate_destructed = true;
    }

    void context_open( IronBee::Context c )
    {
        s_delegate_context_open = true;
        s_ib_context = c.ib();
    }

    void context_close( IronBee::Context c )
    {
        s_delegate_context_close = true;
        s_ib_context = c.ib();
    }

    void context_destroy( IronBee::Context c )
    {
        s_delegate_context_destroy = true;
        s_ib_context = c.ib();
    }
};

static const char* s_module_name = "test_module_bootstrap_b";

IBPP_BOOTSTRAP_MODULE_DELEGATE( s_module_name, Delegate );

TEST_F( TestModuleBootstrapB, basic )
{
    s_delegate_constructed     = false;
    s_delegate_destructed      = false;
    s_delegate_context_open    = false;
    s_delegate_context_close   = false;
    s_delegate_context_destroy = false;
    s_ib_module                = NULL;
    s_ib_context               = NULL;

    ib_module_t* m = IB_MODULE_SYM( m_ib_engine );

    EXPECT_TRUE( s_delegate_constructed );
    EXPECT_EQ( m,                     s_ib_module   );
    EXPECT_EQ( s_module_name,         m->name       );
    EXPECT_EQ( std::string(__FILE__), m->filename   );
    EXPECT_EQ( m_ib_engine,           m->ib         );

    ib_context_t c;
    ib_status_t rc;

    s_delegate_context_open = false;
    s_ib_context = NULL;
    rc = m->fn_ctx_open(
        m_ib_engine,
        m,
        &c,
        m->cbdata_ctx_open
    );
    EXPECT_EQ( IB_OK, rc );
    EXPECT_TRUE( s_delegate_context_open );
    EXPECT_EQ( &c, s_ib_context );

    s_delegate_context_close = false;
    s_ib_context = NULL;
    rc = m->fn_ctx_close(
        m_ib_engine,
        m,
        &c,
        m->cbdata_ctx_close
    );
    EXPECT_EQ( IB_OK, rc );
    EXPECT_TRUE( s_delegate_context_close );
    EXPECT_EQ( &c, s_ib_context );

    s_delegate_context_destroy = false;
    s_ib_context = NULL;
    rc = m->fn_ctx_destroy(
        m_ib_engine,
        m,
        &c,
        m->cbdata_ctx_destroy
    );
    EXPECT_EQ( IB_OK, rc );
    EXPECT_TRUE( s_delegate_context_destroy );
    EXPECT_EQ( &c, s_ib_context );

    s_delegate_destructed = false;
    rc = m->fn_fini(
        m_ib_engine,
        m,
        m->cbdata_fini
    );
    ASSERT_TRUE( s_delegate_destructed );
}

