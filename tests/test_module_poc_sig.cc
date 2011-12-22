//////////////////////////////////////////////////////////////////////////////
// Licensed to Qualys, Inc. (QUALYS) under one or more
// contributor license agreements.  See the NOTICE file distributed with
// this work for additional information regarding copyright ownership.
// QUALYS licenses this file to You under the Apache License, Version 2.0
// (the "License"); you may not use this file except in compliance with
// the License.  You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
/// @file
/// @brief IronBee - poc_sig module tests
/// 
/// @author Brian Rectanus <brectanus@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

#define TESTING

#include "ibtest_util.c"

/// @test Test poc_sig module - load module programatically
TEST(TestIronBee, test_module_poc_sig_load_module)
{
    ib_engine_t *ib;
    ib_module_t *m;

    ibtest_engine_create(&ib);
    ibtest_engine_module_load(ib,
                              IB_XSTRINGIFY(MODULE_BASE_PATH) "/ibmod_poc_sig.so",
                              &m);
    ibtest_engine_destroy(ib);
}

// Include the module code with an alternate prefix.
// Without this alternate prefix the symbol names will clash
// with the static core module.
#ifdef IB_MODULE_SYM_PREFIX
#undef IB_MODULE_SYM_PREFIX
#endif
#define IB_MODULE_SYM_PREFIX  ibtest_sym
#include "../modules/poc_sig.c"

/// @test Test poc_sig module - initialize module by including source
TEST(TestIronBee, test_module_poc_sig_init_module)
{
    ib_engine_t *ib;
    ib_module_t *m;

    ibtest_engine_create(&ib);

    // This will initialize the included module code and write the
    // module handle to m for further use.
    ibtest_engine_module_init(ib, &m);

    /// @todo More tests on module (m) here.

    ibtest_engine_destroy(ib);
}

