#include <ironbeepp/module_bootstrap.hpp>
#include <ironbeepp/module_delegate.hpp>

#include "fixture.hpp"

#include "gtest/gtest.h"

// The main part of this test is that it compiles.

class TestModuleDelegate : public ::testing::Test, public IBPPTestFixture
{
};

IBPP_BOOTSTRAP_MODULE_DELEGATE(
    "test_module_delegate",
    IronBee::ModuleDelegate
);

TEST_F( TestModuleDelegate, basic )
{
    IB_MODULE_SYM( m_ib_engine );
}
