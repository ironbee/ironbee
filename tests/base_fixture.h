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
/// @brief IronBee - Base fixture for Ironbee tests
///
/// @author Craig Forbes <cforbes@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#ifndef __BASE_FIXTURE_H__
#define __BASE_FIXTURE_H__

#include <ironbee/release.h>
#include "ironbee_private.h"

#include "gtest/gtest.h"

class BaseFixture : public ::testing::Test {
public:
    virtual void SetUp() {
        ibt_ibplugin.vernum = IB_VERNUM;
        ibt_ibplugin.abinum = IB_ABINUM;
        ibt_ibplugin.version = IB_VERSION;
        ibt_ibplugin.filename = __FILE__;
        ibt_ibplugin.name = "unit_tests";

        atexit(ib_shutdown);
        ib_initialize();
        ib_engine_create(&ib_engine, &ibt_ibplugin);
        ib_engine_init(ib_engine);
    }

    virtual void TearDown() {
        ib_engine_destroy(ib_engine);
    }

    ib_engine_t *ib_engine;
    ib_plugin_t ibt_ibplugin;
};

#endif /* __BASE_FIXTURE_H__ */
