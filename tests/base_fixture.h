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
 *****************************************************************************/

//////////////////////////////////////////////////////////////////////////////
/// @file
/// @brief IronBee &mdash; Base fixture for Ironbee tests
///
/// @author Craig Forbes <cforbes@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#ifndef __BASE_FIXTURE_H__
#define __BASE_FIXTURE_H__

#include <ironbee/release.h>
#include <ironbee/core.h>
#include <ironbee/state_notify.h>
#include "ironbee_private.h"

#include "gtest/gtest.h"

#include <stdexcept>
#include <string>

#include <boost/lexical_cast.hpp>

#define ASSERT_IB_OK(x) ASSERT_EQ(IB_OK, (x))

class BaseFixture : public ::testing::Test {
public:
    void SetUp() {
        ibt_ibserver.vernum = IB_VERNUM;
        ibt_ibserver.abinum = IB_ABINUM;
        ibt_ibserver.version = IB_VERSION;
        ibt_ibserver.filename = __FILE__;
        ibt_ibserver.name = "unit_tests";

        ib_initialize();
        ib_engine_create(&ib_engine, &ibt_ibserver);
        ib_engine_init(ib_engine);

        resetRuleBasePath();
        resetModuleBasePath();
    }

    void resetRuleBasePath()
    {
        setRuleBasePath(IB_XSTRINGIFY(RULE_BASE_PATH));
    }

    void setRuleBasePath(const char* path)
    {
        ib_core_cfg_t *corecfg = NULL;
        ib_context_module_config(ib_engine->ctx,
                                 ib_core_module(),
                                 static_cast<void*>(&corecfg));
        corecfg->rule_base_path = path;
    }

    void resetModuleBasePath()
    {
        setModuleBasePath(IB_XSTRINGIFY(MODULE_BASE_PATH));
    }

    void setModuleBasePath(const char* path)
    {
        ib_core_cfg_t *corecfg = NULL;
        ib_context_module_config(ib_engine->ctx,
                                 ib_core_module(),
                                 static_cast<void*>(&corecfg));
        corecfg->module_base_path = path;
    }

    /**
     * Parse and load the configuration \c TestName.TestCase.config.
     *
     * The given file is sent through the IronBee configuration parser. It is
     * not expected that modules will be loaded through this interface, but
     * that they will have already been initialized using the
     * \c BaseModuleFixture class (a child of this class). The parsing of
     * the configuration file, then, is to setup to test the loaded module,
     * or other parsing.
     *
     * Realize, though, that nothing prevents the tester from using the
     * LoadModule directive in their configuration.
     */
    void configureIronBee(const std::string& configFile) {

        ib_status_t rc;
        ib_cfgparser_t *p;
        rc = ib_cfgparser_create(&p, ib_engine);

        if (rc != IB_OK) {
            throw std::runtime_error(
                std::string("Failed to create parser: ") +
                    boost::lexical_cast<std::string>(rc)
            );
        }

        rc = ib_cfgparser_parse(p, configFile.c_str());

        if (rc != IB_OK) {
            throw std::runtime_error(
                std::string("Failed to parse configuration file."));
        }
    }

    /**
     * Configure IronBee using the file @e testName.@e testCase.config.
     *
     * This is done by using the GTest api to get the current test name
     * and case and building the string @e testName.@e testCase.config and
     * passing that to configureIronBee(string).
     *
     * @throws std::runtime_error(std::string) if any error occurs.
     */
    void configureIronBee()
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

        configureIronBee(configFile);
    }

    void sendDataIn(ib_conn_t *ib_conn, const std::string& req)
    {
        ib_conndata_t *ib_conndata;
        ib_status_t rc;

        rc = ib_conn_data_create(ib_conn, &ib_conndata, req.size());
        if (rc != IB_OK) {
            throw std::runtime_error(
                std::string("ib_conn_data_create failed"));
        }
        ib_conndata->dlen = req.size();
        memcpy(ib_conndata->data, req.data(), req.size());
        rc = ib_state_notify_conn_data_in(ib_engine, ib_conndata);
        if (rc != IB_OK) {
            throw std::runtime_error(
                std::string("ib_notify_conn_data_in failed"));
        }
    }

    void sendDataOut(ib_conn_t *ib_conn, const std::string& req)
    {
        ib_conndata_t *ib_conndata;
        ib_status_t rc;

        rc = ib_conn_data_create(ib_conn, &ib_conndata, req.size());
        if (rc != IB_OK) {
            throw std::runtime_error(
                std::string("ib_conn_data_create failed"));
        }
        ib_conndata->dlen = req.size();
        memcpy(ib_conndata->data, req.data(), req.size());
        rc = ib_state_notify_conn_data_out(ib_engine, ib_conndata);
        if (rc != IB_OK) {
            throw std::runtime_error(
                std::string("ib_notify_conn_data_in failed"));
        }
    }


    /**
     * Build an IronBee connection and call ib_state_notify_conn_opened on it.
     *
     * You should call ib_state_notify_conn_closed(ib_engine, ib_conn)
     * when done.
     *
     * The connection will be initialized with a local address of
     * 1.0.0.1:80 and a remote address of 1.0.0.2:65534.
     *
     * @returns The Initialized IronbeeConnection.
     */
    ib_conn_t* buildIronBeeConnection()
    {
        ib_conn_t* ib_conn;
        ib_conn_create(ib_engine, &ib_conn, NULL);
        ib_conn->local_ipstr = "1.0.0.1";
        ib_conn->remote_ipstr = "1.0.0.2";
        ib_conn->remote_port = 65534;
        ib_conn->local_port = 80;
        ib_state_notify_conn_opened(ib_engine, ib_conn);

        return ib_conn;
    }

    void loadModule(ib_module_t **ib_module,
                            const std::string& module_file)
    {
        ib_status_t rc;

        std::string module_path =
            std::string(IB_XSTRINGIFY(MODULE_BASE_PATH)) +
            "/" +
            module_file;

        rc = ib_module_load(ib_module, ib_engine, module_path.c_str());

        if (rc != IB_OK) {
            throw std::runtime_error("Failed to load module " + module_file);
        }

        rc = ib_module_init(*ib_module, ib_engine);

        if (rc != IB_OK) {
            throw std::runtime_error("Failed to init module " + module_file);
        }
    }

    void TearDown() {
        ib_engine_destroy(ib_engine);
        ib_shutdown();
    }

    ib_engine_t *ib_engine;
    ib_server_t ibt_ibserver;
};

/**
 * Testing fixture by which to test IronBee modules.
 *
 * Users of this class should extend it and pass in the name of the module to
 * be tested.
 *
 * @code
 * class MyModTest : public BaseModuleFixture {
 *     public:
 *     MyModTest() : BaseModuleFixture("mymodule.so") { }
 * };
 *
 * TEST_F(MyModTest, test001) {
 *   // Test the module!
 * }
 * @endcode
 */
class BaseModuleFixture : public BaseFixture {
protected:
    //! The file name of the module.
    std::string m_module_file;

    //! The setup module is stored here.
    ib_module_t *ib_module;


public:
    BaseModuleFixture(const std::string& module_file) :
        m_module_file(module_file),
        ib_module(NULL)
    {}

    void SetUp()
    {
        BaseFixture::SetUp();

        loadModule(&ib_module, m_module_file);
    }

    void TearDown() {
        ib_status_t rc;
        rc = ib_module_unload(ib_module);

        if (rc != IB_OK) {
            std::cerr<<"Failed to unload module "
                     <<m_module_file
                     <<" with ib_status of "
                     <<rc
                     <<std::endl;
        }

        BaseFixture::TearDown();
    }
};

#endif /* __BASE_FIXTURE_H__ */
