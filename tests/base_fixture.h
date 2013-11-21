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

//////////////////////////////////////////////////////////////////////////////
/// @file
/// @brief IronBee --- Base fixture for Ironbee tests
///
/// @author Craig Forbes <cforbes@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#ifndef __BASE_FIXTURE_H__
#define __BASE_FIXTURE_H__

#include <ironbee/release.h>
#include <ironbee/context.h>
#include <ironbee/core.h>
#include <ironbee/state_notify.h>
#include <ironbee/string.h>
#include <ironbee/util.h>

#include "gtest/gtest.h"

#include <stdexcept>
#include <string>

#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>

#include <vector>

#define ASSERT_IB_OK(x) ASSERT_EQ(IB_OK, (x))
static const size_t EXCEPTION_BUF_SIZE = 128;

class BaseFixture : public ::testing::Test
{
public:
    virtual void SetUp()
    {
        ib_status_t rc;
        char buf[EXCEPTION_BUF_SIZE+1];

        /* Setup IronBee server object. */
        memset(&ibt_ibserver, 0, sizeof(ibt_ibserver));
        ibt_ibserver.vernum = IB_VERNUM;
        ibt_ibserver.abinum = IB_ABINUM;
        ibt_ibserver.version = IB_VERSION;
        ibt_ibserver.filename = __FILE__;
        ibt_ibserver.name = "unit_tests";

        /* Call global initialization routine. */
        rc = ib_initialize();
        if (rc != IB_OK) {
            snprintf(buf, EXCEPTION_BUF_SIZE,
                     "Failed to initialize IronBee: %s",
                     ib_status_to_string(rc));
            throw std::runtime_error(buf);
        }

        /* Create the engine. */
        rc = ib_engine_create(&ib_engine, &ibt_ibserver);
        if (rc != IB_OK) {
            snprintf(buf, EXCEPTION_BUF_SIZE,
                     "Failed to create IronBee Engine: %s",
                     ib_status_to_string(rc));
            throw std::runtime_error(buf);
        }

        /* Set/reset the rules base path and modules base path.*/
        resetRuleBasePath();
        resetModuleBasePath();
    }

    /**
     * Reset the rule base path configuration in this IronBee engine to a
     * default for testing.
     */
    void resetRuleBasePath()
    {
        setRuleBasePath(IB_XSTRINGIFY(RULE_BASE_PATH));
    }

    /**
     * Set the rules base path in ib_engine to be @a path.
     * @param[in] path The path to the rules.
     */
    void setRuleBasePath(const char* path)
    {
        ib_core_cfg_t *corecfg = NULL;
        ib_core_context_config(ib_context_main(ib_engine), &corecfg);
        corecfg->rule_base_path = path;
    }

    /**
     * Reset the module base path configuration in this IronBee engine to a
     * default for testing.
     */
    void resetModuleBasePath()
    {
        setModuleBasePath(IB_XSTRINGIFY(MODULE_BASE_PATH));
    }

    /**
     * Set the module base path in ib_engine to be @a path.
     * @param[in] path The path to the modules.
     */
    void setModuleBasePath(const char* path)
    {
        ib_core_cfg_t *corecfg = NULL;
        ib_core_context_config(ib_context_main(ib_engine), &corecfg);
        corecfg->module_base_path = path;
    }

    std::string getBasicIronBeeConfig()
    {
        return std::string(
            "# A basic ironbee configuration\n"
            "# for getting an engine up-and-running.\n"
            "LogLevel 9\n"

            "LoadModule \"ibmod_htp.so\"\n"
            "LoadModule \"ibmod_pcre.so\"\n"
            "LoadModule \"ibmod_rules.so\"\n"
            "LoadModule \"ibmod_user_agent.so\"\n"

            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"

            "# Disable audit logs\n"
            "AuditEngine Off\n"

            "<Site test-site>\n"
            "SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "Hostname somesite.com\n"
            "</Site>\n");
    }

    /**
     * Create a temporary configuration file and have IronBee read it in.
     */
    void configureIronBeeByString(const std::string& configurationText)
    {
        static const std::string fileNameTemplate = "ironbee_gtest.conf_XXXXXX";
        std::vector<char> configFile;
        int fileFd;
        ssize_t writeSize;

        configFile.assign(fileNameTemplate.begin(), fileNameTemplate.end());
        configFile.push_back('\0');

        fileFd = mkstemp(&configFile.front());

        if (fileFd < 0) {
            throw std::runtime_error("Failed to open tmp ironbee conf file.");
        }

        writeSize = write(
            fileFd,
            configurationText.c_str(),
            configurationText.size());
        close(fileFd);
        if (writeSize < 0) {
            throw std::runtime_error("Failed to write whole config file.");
        }

        configureIronBee(&configFile.front());
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
    void configureIronBee(const std::string& configFile)
    {
        ib_status_t rc;
        ib_cfgparser_t *cp;

        rc = ib_cfgparser_create(&cp, ib_engine);
        if (rc != IB_OK) {
            throw std::runtime_error(
                std::string("Failed to create parser: ") +
                    boost::lexical_cast<std::string>(rc)
            );
        }

        rc = ib_engine_config_started(ib_engine, cp);
        if (rc != IB_OK) {
            throw std::runtime_error(
                std::string("Failed to start configuration: ") +
                    boost::lexical_cast<std::string>(rc)
            );
        }

        rc = ib_cfgparser_parse(cp, configFile.c_str());
        if (rc != IB_OK) {
            throw std::runtime_error(
                std::string("Failed to parse configuration file."));
        }

        rc = ib_engine_config_finished(ib_engine);
        if (rc != IB_OK) {
            throw std::runtime_error(
                std::string("Failed to start configuration: ") +
                    boost::lexical_cast<std::string>(rc)
            );
        }

        rc = ib_cfgparser_destroy(cp);
        if (rc != IB_OK) {
            throw std::runtime_error(
                std::string("Failed to destroy parser"));
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

        if ( boost::filesystem::exists(boost::filesystem::path(configFile)) )
        {
            std::cout << "Using " << configFile << "." << std::endl;
            configureIronBee(configFile);
        }
        else
        {
            std::cout << "Could not open config "
                      << "\"" << configFile << "\""
                      << ". Using default BasicIronBee.config."
                      << std::endl;
            configureIronBeeByString(getBasicIronBeeConfig());
        }
    }

    /**
     * Build an IronBee connection and call ib_state_notify_conn_opened() on it.
     *
     * You should call ib_state_notify_conn_closed() when done.
     *
     * The connection will be initialized with a local address of
     * 1.0.0.1:80 and a remote address of 1.0.0.2:65534.
     *
     * @returns The Initialized IronbeeConnection.
     */
    ib_conn_t *buildIronBeeConnection()
    {
        ib_status_t rc;
        ib_conn_t *ib_conn;

        rc = ib_conn_create(ib_engine, &ib_conn, NULL);
        if (rc != IB_OK) {
            throw std::runtime_error("Failed to create IronBee connection.");
        }
        ib_conn->local_ipstr = "1.0.0.1";
        ib_conn->remote_ipstr = "1.0.0.2";
        ib_conn->remote_port = 65534;
        ib_conn->local_port = 80;
        rc = ib_state_notify_conn_opened(ib_engine, ib_conn);
        if (rc != IB_OK) {
            throw std::runtime_error("Failed to open IronBee connection.");
        }

        return ib_conn;
    }

    /**
     * Build an IronBee transaction for a connection
     *
     * @param[in] conn IronBee connection
     *
     * @returns The initialized Ironbee transaction.
     */
    ib_tx_t *buildIronBeeTransaction(ib_conn_t *conn)
    {
        ib_status_t rc;
        ib_tx_t *tx;

        rc = ib_tx_create( &tx, conn, NULL );
        if (rc != IB_OK) {
            throw std::runtime_error("Failed to create IronBee transaction.");
        }
        return tx;
    }

    /**
     * Throw generic notify error
     *
     * @param[in] msg Notify error message
     */
    void notifyError(const char *msg)
    {
        std::string err;
        err += "failed to notify ";
        err += msg;
        throw std::runtime_error(err);
    }

    /**
     * Add a name/value to request/response header
     *
     * @param[in] parsed Parsed name/value pair list
     * @param[in] name Header name
     * @param[in] value Header value
     */
    void addHeader(ib_parsed_header_wrapper_t *parsed,
                   const char *name,
                   const char *value)
    {
        ib_status_t            rc;

        rc = ib_parsed_name_value_pair_list_add(parsed,
                                                name,
                                                strlen(name),
                                                value,
                                                strlen(value));
        if (rc != IB_OK) {
            throw std::runtime_error("Failed to add name/value to header.");
        }
    }

    /**
     * Send a request line
     *
     * @param[in] tx IronBee transaction
     * @param[in] method Method portion of request line
     * @param[in] uri URI portion of request line
     * @param[in] proto Protocol portion of request line
     */
    void sendRequestLine(ib_tx_t *tx,
                         const char *method,
                         const char *uri,
                         const char *proto)
    {
        ib_status_t            rc;
        std::string            line;
        ib_parsed_req_line_t  *parsed;

        line += method;
        line += " ";
        line += uri;
        line += " ";
        line += proto;
        line += "\r\n";

        rc = ib_parsed_req_line_create(&parsed, tx,
                                       line.data(), line.length(),
                                       method, strlen(method),
                                       uri, strlen(uri),
                                       proto, strlen(proto));
        if (rc != IB_OK) {
            throw std::runtime_error("Failed to create parsed request line");
        }

        rc = ib_state_notify_request_started(ib_engine, tx, parsed);
        if (rc != IB_OK) {
            notifyError("request start");
        }
    }

    /**
     * Start request header for an IronBee transaction
     *
     * @param[in] tx IronBee transaction
     * @param[out] pparsed Pointer to new parsed name / value pair
     *
     * @returns Status code
     */
    void startRequestHeader(ib_tx_t *tx,
                            ib_parsed_header_wrapper_t **pparsed)
    {
        ib_status_t rc;
        rc = ib_parsed_name_value_pair_list_wrapper_create(pparsed, tx);
        if (rc != IB_OK) {
            notifyError("request header");
        }
    }

    /**
     * Send a request header
     *
     * @param[in] tx IronBee transaction
     * @param[in] parsed Parsed header
     */
    void sendRequestHeader(ib_tx_t *tx,
                           ib_parsed_header_wrapper_t *parsed)
    {
        ib_status_t rc;

        rc = ib_state_notify_request_header_data(ib_engine, tx, parsed);

        if (rc != IB_OK) {
            notifyError("request header data.");
        }

        rc = ib_state_notify_request_header_finished(ib_engine, tx);
        if (rc != IB_OK) {
            notifyError("request header finished");
        }
    }

    /**
     * Finish request
     *
     * @param[in] tx IronBee transaction
     */
    void finishRequest(ib_tx_t *tx)
    {
        ib_status_t rc;

        rc = ib_state_notify_request_finished(ib_engine, tx);
        if (rc != IB_OK) {
            notifyError("request finished");
        }
    }

    /**
     * Send a response line
     *
     * @param[in] tx IronBee transaction
     * @param[in] proto Protocol portion of request line
     * @param[in] status Status code (as a string)
     * @param[in] message Status message
     */
    void sendResponseLine(ib_tx_t *tx,
                          const char *proto,
                          const char *status,
                          const char *message)
    {
        ib_status_t            rc;
        std::string            line;
        ib_parsed_resp_line_t *parsed;

        line += proto;
        line += " ";
        line += status;
        if ( (message != NULL) && (*message != '\0') ) {
            line += " ";
            line += message;
        }
        line += "\r\n";

        rc = ib_parsed_resp_line_create(&parsed, tx,
                                        line.data(), line.length(),
                                        proto, strlen(proto),
                                        status, strlen(status),
                                        message, strlen(message));
        if (rc != IB_OK) {
            notifyError("response header");
        }

        rc = ib_state_notify_response_started(ib_engine, tx, parsed);
        if (rc != IB_OK) {
            notifyError("response started");
        }
    }

    /**
     * Start response header for an IronBee transaction
     *
     * @param[in] tx IronBee transaction
     * @param[out] pparsed Pointer to new parsed name / value pair
     */
    void startResponseHeader(ib_tx_t *tx,
                             ib_parsed_header_wrapper_t **pparsed)
    {
        ib_status_t rc;
        rc = ib_parsed_name_value_pair_list_wrapper_create(pparsed, tx);
        if (rc != IB_OK) {
            notifyError("response header");
        }
    }

    /**
     * Send a response header
     *
     * @param[in] tx IronBee transaction
     * @param[in] parsed Parsed header
     */
    void sendResponseHeader(ib_tx_t *tx,
                            ib_parsed_header_wrapper_t *parsed)
    {
        ib_status_t rc;
        rc = ib_state_notify_response_header_data(ib_engine, tx, parsed);
        if (rc != IB_OK) {
            notifyError("response header data");
        }

        rc = ib_state_notify_response_header_finished(ib_engine, tx);
        if (rc != IB_OK) {
            notifyError("response header finished");
        }
    }

    /**
     * Finish response
     *
     * @param[in] tx IronBee transaction
     */
    void finishResponse(ib_tx_t *tx)
    {
        ib_status_t rc;

        rc = ib_state_notify_response_finished(ib_engine, tx);
        if (rc != IB_OK) {
            notifyError("response finished.");
        }
    }

    /**
     * Perform post-processing
     *
     * @param[in] tx IronBee transaction
     */
    void postProcess(ib_tx_t *tx)
    {
        ib_status_t rc;

        if (! ib_tx_flags_isset(tx, IB_TX_FPOSTPROCESS)) {
            rc = ib_state_notify_postprocess(ib_engine, tx);
            if (rc != IB_OK) {
                notifyError("post process.");
            }
        }
    }

    void loadModule(const std::string& module_file)
    {
        ib_status_t rc;

        std::string module_path =
            std::string(IB_XSTRINGIFY(MODULE_BASE_PATH)) +
            "/" +
            module_file;

        rc = ib_module_load(ib_engine, module_path.c_str());

        if (rc != IB_OK) {
            throw std::runtime_error("Failed to load module " + module_file);
        }
    }

    virtual void TearDown()
    {
        ib_engine_destroy(ib_engine);
        ib_shutdown();
    }

    virtual ~BaseFixture(){}

    ib_mpool_t *MainPool(void)
    {
        return ib_engine_pool_main_get(ib_engine);
    }

    ib_engine_t *ib_engine;
    ib_server_t ibt_ibserver;
};

/**
 * Testing fixture which runs a simple transaction
 *
 * Users of this class can extend it if required.
 */
class BaseTransactionFixture : public BaseFixture
{
public:
    virtual ~BaseTransactionFixture(){}

    virtual void SetUp(void)
    {
        BaseFixture::SetUp();
    }
    virtual void configureIronBee(void)
    {
        BaseFixture::configureIronBee();
    }
    void configureIronBee(const char *filename)
    {
        BaseFixture::configureIronBee(filename);
    }
    void performTx(void)
    {
        ib_conn = buildIronBeeConnection();
        ib_tx = buildIronBeeTransaction(ib_conn);

        sendRequest();
        sendResponse();
        postProcess(ib_tx);
    }

    /* Request related function */
    void sendRequest(void)
    {
        sendRequestLine();
        BaseFixture::startRequestHeader(ib_tx, &ib_reqhdr);
        generateRequestHeader();
        BaseFixture::sendRequestHeader(ib_tx, ib_reqhdr);
        sendRequestBody();
        BaseFixture::finishRequest(ib_tx);
    }
    virtual void sendRequestLine(const char *method,
                         const char *uri,
                         const char *proto)
    {
        BaseFixture::sendRequestLine(ib_tx, method, uri, proto);
    }
    void addRequestHeader(const char *name,
                          const char *value)
    {
        BaseFixture::addHeader(ib_reqhdr, name, value);
    }

    /* Response related functions */
    void sendResponse(void)
    {
        sendResponseLine();
        BaseFixture::startResponseHeader(ib_tx, &ib_rsphdr);
        generateResponseHeader();
        BaseFixture::sendResponseHeader(ib_tx, ib_rsphdr);
        sendResponseBody();
        BaseFixture::finishResponse(ib_tx);
    }
    void sendResponseLine(const char *proto,
                          const char *status,
                          const char *message)
    {
        BaseFixture::sendResponseLine(ib_tx, proto, status, message);
    }
    void addResponseHeader(const char *name,
                           const char *value)
    {
        BaseFixture::addHeader(ib_rsphdr, name, value);
    }

    /* Request functions to overload */
    virtual void sendRequestLine()
    {
        sendRequestLine("GET", "/", "HTTP/1.1");
    }

    virtual void generateRequestHeader()
    {
        addRequestHeader("Host", "UnitTest");
        addRequestHeader("Content-Type", "text/html");
        addRequestHeader("X-MyHeader", "header1");
        addRequestHeader("X-MyHeader", "header2");
    }

    virtual void sendRequestBody()
    {
    };

    /* Request functions to overload */
    virtual void sendResponseLine()
    {
        sendResponseLine("HTTP/1.1", "200", "OK");
    }

    virtual void generateResponseHeader()
    {
        addResponseHeader("Content-Type", "text/html");
        addResponseHeader("X-MyHeader", "header3");
        addResponseHeader("X-MyHeader", "header4");
    }

    virtual void sendResponseBody()
    {
    };

    ib_var_source_t *acquireSource(const char *name)
    {
        ib_status_t rc;
        ib_var_source_t *source;

        rc = ib_var_source_acquire(
            &source,
            MainPool(),
            ib_engine_var_config_get(ib_engine),
            IB_S2SL(name)
        );
        if (rc != IB_OK) {
            throw std::runtime_error("Failed to acquire source.");
        }

        return source;
    }

    ib_var_target_t *acquireTarget(const char *str)
    {
        ib_status_t rc;
        ib_var_target_t *target;

        rc = ib_var_target_acquire_from_string(
            &target,
            MainPool(),
            ib_engine_var_config_get(ib_engine),
            IB_S2SL(str),
            NULL, NULL
        );
        if (rc != IB_OK) {
            throw std::runtime_error("Failed to acquire target.");
        }

        return target;
    }

    ib_field_t *getVar(const char *name)
    {
        ib_status_t rc;
        ib_field_t *f;

        rc = ib_var_source_get(acquireSource(name), &f, ib_tx->var_store);
        if (rc != IB_OK) {
            throw std::runtime_error("Failed to get field from source.");
        }

        return f;
    }

    void setVar(const char *name, ib_field_t *f)
    {
        ib_status_t rc;

        rc = ib_var_source_set(acquireSource(name), ib_tx->var_store, f);
        if (rc != IB_OK) {
            throw std::runtime_error("Failed to set field from source.");
        }
    }

    ib_field_t *getTarget1(const char *str)
    {
        const ib_list_t *result;
        ib_status_t rc;

        rc = ib_var_target_get(
            acquireTarget(str),
            &result,
            MainPool(),
            ib_tx->var_store
        );
        if (rc == IB_ENOENT) {
            return NULL;
        }
        if (rc != IB_OK) {
            throw std::runtime_error("Failed to get target.");
        }

        if (ib_list_elements(result) == 0) {
            return NULL;
        }

        if (ib_list_elements(result) > 1) {
            throw std::runtime_error("Got more than 1 value for target.");
        }

        return (ib_field_t *)ib_list_node_data_const(
            ib_list_first_const(result));
    }

    const ib_list_t *getTargetN(const char *str)
    {
        const ib_list_t *result;
        ib_status_t rc;

        rc = ib_var_target_get(
            acquireTarget(str),
            &result,
            MainPool(),
            ib_tx->var_store
        );
        if (rc == IB_ENOENT) {
            return NULL;
        }
        if (rc != IB_OK) {
            throw std::runtime_error("Failed to get target.");
        }

        return result;
    }

protected:
    ib_conn_t                  *ib_conn;
    ib_tx_t                    *ib_tx;
    ib_parsed_header_wrapper_t *ib_reqhdr;
    ib_parsed_header_wrapper_t *ib_rsphdr;
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
class BaseModuleFixture : public BaseTransactionFixture
{
protected:
    //! The file name of the module.
    std::string m_module_file;

public:
    explicit
    BaseModuleFixture(const std::string& module_file) :
        m_module_file(module_file)
    {}

    virtual void SetUp()
    {
        BaseTransactionFixture::SetUp();
        configureIronBee();
        loadModule(m_module_file);
    }

    virtual ~BaseModuleFixture(){}
};

#endif /* __BASE_FIXTURE_H__ */
