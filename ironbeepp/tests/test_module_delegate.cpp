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
 * @brief IronBee++ Internals --- Module Delegate Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

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

TEST_F(TestModuleDelegate, basic)
{
    IB_MODULE_SYM(m_engine.ib());
}
