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
/// @brief IronBee --- Engine Test Functions
///
/// @author Brian Rectanus <brectanus@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"
#include "base_fixture.h"

#include <fstream>

#include <ironbee/engine_manager.h>

/**
 * Base class for engine manager tests.
 *
 * This may be relocated to a more public .hpp file if necessary.
 */
class EngineManagerFixture : public ::testing::Test
{
    protected:

    ib_server_t   m_server;
    ib_manager_t *m_manager;

    public:
    virtual void SetUp() {
        ASSERT_EQ(IB_OK, ib_initialize());

        m_server.vernum        = IB_VERNUM;
        m_server.abinum        = IB_ABINUM;
        m_server.version       = IB_VERSION;
        m_server.filename      = __FILE__;
        m_server.name          = "engine manager unit tests";

        m_server.hdr_fn        = NULL;
        m_server.hdr_data      = NULL;
        m_server.err_fn        = NULL;
        m_server.err_data      = NULL;
        m_server.err_hdr_fn    = NULL;
        m_server.err_hdr_data  = NULL;
        m_server.err_body_fn   = NULL;
        m_server.err_body_data = NULL;
        m_server.close_fn      = NULL;
        m_server.close_data    = NULL;


        ASSERT_EQ(
            IB_OK,
            ib_manager_create(
                &m_manager,
                &m_server,
                IB_MANAGER_DEFAULT_MAX_ENGINES)
        );
    }

    virtual void TearDown() {
        ASSERT_EQ(IB_OK, ib_shutdown());
    }

    /**
     * Configure IronBee using the file @e testName.@e testCase.config.
     *
     * This is done by using the GTest api to get the current test name
     * and case and building the string @e testName.@e testCase.config and
     * passing that to configureIronBee(string).
     *
     * @throws std::runtime_error(std::string) if any error occurs.
     *
     * @returns path to the file.
     */
    std::string createIronBeeConfig()
    {
        using ::testing::TestInfo;
        using ::testing::UnitTest;

        const TestInfo* const info =
            UnitTest::GetInstance()->current_test_info();

        const std::string configFile =
            std::string(info->test_case_name())+
            "."+
            std::string(info->name()) +
            ".config";

        std::ofstream o(configFile.c_str());
        o << getBasicIronBeeConfig();
        o.close();

        return configFile;
    }

    std::string getBasicIronBeeConfig()
    {
        return std::string(
            "# A basic ironbee configuration\n"
            "# for getting an engine up-and-running.\n"
            "SensorId       B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName     UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"

            "# Disable audit logs\n"
            "AuditEngine Off\n"

            "<Site test-site>\n"
            "    SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "    Hostname somesite.com\n"
            "</Site>\n");
    }

};

/**
 * Engine manager tests dispatch off of this specific fixture.
 */
class EngineManager : public EngineManagerFixture
{
};

TEST_F(EngineManager, MaxEngines)
{
    std::vector<ib_engine_t *> engines(IB_MANAGER_DEFAULT_MAX_ENGINES);

    for(size_t i = 0; i < IB_MANAGER_DEFAULT_MAX_ENGINES; ++i) {
        ASSERT_EQ(
            IB_OK,
            ib_manager_engine_create(
                m_manager,
                IB_MANAGER_ENGINE_NAME_DEFAULT,
                createIronBeeConfig().c_str()
            )
        );

        ASSERT_EQ(
            IB_OK,
            ib_manager_engine_acquire(
                m_manager,
                IB_MANAGER_ENGINE_NAME_DEFAULT, &(engines[i])));

        ASSERT_EQ(i+1U, ib_manager_engine_count(m_manager));
    }

    ASSERT_EQ(
        IB_DECLINED,
        ib_manager_engine_create(
            m_manager,
            IB_MANAGER_ENGINE_NAME_DEFAULT,
            createIronBeeConfig().c_str()
        )
    );

    /* Return an engine, and try to get a new one. */
    ASSERT_EQ(IB_OK, ib_manager_engine_release(m_manager, engines[0]));
    ASSERT_EQ(
        IB_OK,
        ib_manager_engine_create(
            m_manager,
            IB_MANAGER_ENGINE_NAME_DEFAULT,
            createIronBeeConfig().c_str()
        )
    );
    ASSERT_EQ(IB_OK,
        ib_manager_engine_acquire(
            m_manager,
            IB_MANAGER_ENGINE_NAME_DEFAULT,
            &(engines[0])));

    /* And we should fail again. */
    ASSERT_EQ(
        IB_DECLINED,
        ib_manager_engine_create(
            m_manager,
            IB_MANAGER_ENGINE_NAME_DEFAULT,
            createIronBeeConfig().c_str()
        )
    );

    /* Now clean up the mess we've made. */
    for(size_t i = 0; i < IB_MANAGER_DEFAULT_MAX_ENGINES; ++i) {
        ASSERT_EQ(IB_OK, ib_manager_engine_release(m_manager, engines[i]));
    }

    ib_manager_destroy(m_manager);
}
