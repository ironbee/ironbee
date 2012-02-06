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
#include <ironbee/core.h>
#include "ironbee_private.h"

#include "gtest/gtest.h"

#include <stdexcept>

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

        resetRuleBasePath();
        resetModuleBasePath();
    }

    virtual void resetRuleBasePath()
    {
        setRuleBasePath(IB_XSTRINGIFY(RULE_BASE_PATH));
    }

    virtual void setRuleBasePath(const char* path)
    {
        ib_core_cfg_t *corecfg = NULL;
        ib_context_module_config(ib_engine->ctx,
                                 ib_core_module(),
                                 static_cast<void*>(&corecfg));
        corecfg->rule_base_path = path;
    }

    virtual void resetModuleBasePath()
    {
        setModuleBasePath(IB_XSTRINGIFY(MODULE_BASE_PATH));
    }

    virtual void setModuleBasePath(const char* path)
    {
        ib_core_cfg_t *corecfg = NULL;
        ib_context_module_config(ib_engine->ctx,
                                 ib_core_module(),
                                 static_cast<void*>(&corecfg));
        corecfg->module_base_path = path;
    }

    /**
     * @brief Parse and load the configuration TestName.TestCase.config.
     * @details The given file is sent through the IronBee configuration
     *          parser. It is not expected that modules will be loaded
     *          through this interface, but that they will have
     *          already been initialized using the BaseModuleFixture
     *          class (a child of this class). The parsing of
     *          the configuration file, then, is to setup to test
     *          the loaded module, or other parsing.
     * @paragraph Realize, though, that nothing prevents the tester from
     *            using the LoadModule directive in their configuration.
     */
    virtual void configureIronBee(const std::string& configFile) {

        ib_status_t rc;
        ib_cfgparser_t *p;
        rc = ib_cfgparser_create(&p, ib_engine);

        if (rc != IB_OK) {
            throw std::runtime_error(
                std::string("Failed to create parser: "+rc));
        }

        rc = ib_cfgparser_parse(p, configFile.c_str());

        if (rc != IB_OK) {
            throw std::runtime_error(
                std::string("Failed to parse configuration file."));
        }
    }

    virtual void configureIronBee()
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

    virtual void loadModule(ib_module_t **ib_module,
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

    virtual void TearDown() {
        ib_engine_destroy(ib_engine);
    }

    ib_engine_t *ib_engine;
    ib_plugin_t ibt_ibplugin;
};

/**
 * @brief Testing fixture by which to test IronBee modules.
 * @details Users of this class should extend it and pass in
 *          the name of the module to be tested.
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

    virtual void SetUp()
    {
        BaseFixture::SetUp();

        loadModule(&ib_module, m_module_file);
    }

    virtual void TearDown() {
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
