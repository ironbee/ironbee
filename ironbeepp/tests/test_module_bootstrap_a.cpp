#define IBPP_EXPOSE_C
#include <ironbeepp/module_bootstrap.hpp>

#include "fixture.hpp"

#include "gtest/gtest.h"

class TestModuleBootstrapA : public ::testing::Test, public IBPPTestFixture
{
};

static ib_module_t* g_test_module;

void on_load( IronBee::Module m )
{
    g_test_module = m.ib();
}

static const char* s_module_name = "test_module_bootstrap_a";

IBPP_BOOTSTRAP_MODULE( s_module_name, on_load );

TEST_F( TestModuleBootstrapA, basic )
{
    g_test_module = NULL;

    ib_module_t* m = IB_MODULE_SYM( m_ib_engine );

    ASSERT_EQ( m,                     g_test_module );
    ASSERT_EQ( s_module_name,         m->name       );
    ASSERT_EQ( std::string(__FILE__), m->filename   );
    ASSERT_EQ( m_ib_engine,           m->ib         );
}
