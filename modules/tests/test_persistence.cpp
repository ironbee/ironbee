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
/// @brief IronBee --- PCRE module tests
///
/// @author Brian Rectanus <brectanus@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "base_fixture.h"
#include <ironbee/operator.h>
#include <ironbee/hash.h>
#include <ironbee/mm.h>
#include <ironbee/field.h>
#include <ironbee/bytestr.h>

#include <boost/filesystem.hpp>

class PersistenceLoadModuleTest :
    public BaseTransactionFixture,
    public ::testing::WithParamInterface<const char *> { };

TEST_P(PersistenceLoadModuleTest, LoadModule) {
    std::string config =
        std::string(
            "LogLevel DEBUG\n"
        )
        + GetParam()
        + std::string(
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname somesite.com\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());
    performTx();
    ASSERT_TRUE(ib_tx);
}

INSTANTIATE_TEST_CASE_P(
    WithConfiguration,
    PersistenceLoadModuleTest,
    ::testing::Values(
        /* Load the framework. */
        "LoadModule \"ibmod_persistence_framework.so\"\n",

        /* Load init_collection. */
        "LoadModule \"ibmod_persistence_framework.so\"\n"
        "LoadModule \"ibmod_init_collection.so\"\n"

        /* Load persist */
        "LoadModule \"ibmod_persistence_framework.so\"\n"
        "LoadModule \"ibmod_persist.so\"\n",

        /* Load everything (just in case there might be conflicts). */
        "LoadModule \"ibmod_persistence_framework.so\"\n"
        "LoadModule \"ibmod_init_collection.so\"\n"
        "LoadModule \"ibmod_persist.so\"\n"
    )
);

/* Make a test for persist.c. */
class PersistencePersistTest : public BaseTransactionFixture {
public:
    boost::filesystem::path m_path;

    virtual void SetUp() {
        BaseTransactionFixture::SetUp();

        using namespace boost::filesystem;

        m_path = unique_path();
        create_directories(m_path);
    }
    virtual void TearDown() {
        using namespace boost::filesystem;
        BaseTransactionFixture::TearDown();
        remove_all(m_path);
    }

};

TEST_F(PersistencePersistTest, LoadStore) {
    std::string config(
        "LogLevel DEBUG\n"
        "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
        "SensorName UnitTesting\n"
        "SensorHostname unit-testing.sensor.tld\n"
        "LoadModule \"ibmod_rules.so\"\n"
        "LoadModule \"ibmod_persistence_framework.so\"\n"
        "LoadModule \"ibmod_persist.so\"\n"
    );

    config += "PersistenceStore ASTORE persist-fs://"+m_path.string()+"\n";
    config +=
        "PersistenceMap A ASTORE\n"
        "<Site test-site>\n"
        "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
        "   Hostname *\n"
        "   Rule ARGS @ne 1 phase:REQUEST id:a1 rev:1 setvar:A=1\n"
        "   RuleEnable all\n"
        "</Site>\n"
    ;
    configureIronBeeByString(config.c_str());
    performTx();
}
