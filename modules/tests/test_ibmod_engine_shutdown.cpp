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
/// @brief IronBee --- IB Engine Shutdown
///
/// @author Sam Baskinger <sbaskinger@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "base_fixture.h"
#include <ironbee/operator.h>
#include <ironbee/hash.h>
#include <ironbee/mm.h>
#include <ironbee/field.h>
#include <ironbee/bytestr.h>

class IbmodEngineShutdown : public BaseTransactionFixture {
public:
    void SetUp() {
        BaseFixture::SetUp();
    }

    virtual ~IbmodEngineShutdown()
    {
    }
};

TEST_F(IbmodEngineShutdown, start_stop)
{
    /* Not much more to check right now. */
    /* If we do not crash, we are generally OK. */
    BaseFixture::configureIronBeeByString(
        "# A basic ironbee configuration\n"
        "# for getting an engine up-and-running.\n"
        "LogLevel 9\n"

        "LoadModule \"ibmod_engine_shutdown.so\"\n"

        "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
        "SensorName UnitTesting\n"
        "SensorHostname unit-testing.sensor.tld\n"

        "# Disable audit logs\n"
        "AuditEngine Off\n"

        "<Site test-site>\n"
        "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
        "   Hostname *\n"
        "   Service *\n"
        "</Site>\n"
    );

    performTx();
}
