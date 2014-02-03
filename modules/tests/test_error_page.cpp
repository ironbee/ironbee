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
/// @brief IronBee --- Error Page module tests
///
/// @author Sam Baskinger <sbaskinger@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "base_fixture.h"

#include <engine/engine_private.h>
#include <ironbee/engine_types.h>

#include <fstream>

class ErrorPage : public BaseTransactionFixture
{
};

/* Simply test if the module loads and unloads without error. */
TEST_F(ErrorPage, load_module) {
}

/* Test the case where the error file does not exist. */
TEST_F(ErrorPage, file_not_found) {
    std::string config =
        std::string(
            "LogLevel DEBUG\n"
            "LoadModule \"ibmod_error_page.so\"\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "BlockingMethod status=500\n"
            "HttpStatusCodeContents 500 missing_file.html\n"
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname somesite.com\n"
            "</Site>\n"
        );

    ASSERT_THROW(
        configureIronBeeByString(config.c_str()),
        std::runtime_error);
}

/* Test the case where the error file does not exist. */
TEST_F(ErrorPage, relative_filename) {
    std::string config =
        std::string(
            "LogLevel DEBUG\n"
            "LoadModule \"ibmod_error_page.so\"\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "BlockingMethod status=500\n"
            "HttpStatusCodeContents 500 ../tests/error_page.html\n\n"
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname somesite.com\n"
            "</Site>\n"
        );
}

/* File-local resources for following tests. */
namespace {

int         mock_error_status;
std::string mock_error_body;

extern "C" {
/**
 * Impl for ib_server_t::err_body_fn.
 *
 * This captures a copy of the provided error page.
 * Users of this function should clear the shared state variable,
 * @ref mock_error_body.
 *
 * @param[in] tx Transaction.
 * @param[in] data The error page data.
 * @param[in] len The length of @a data.
 * @param[in] cbdata Unused callback data.
 *
 * @returns IB_OK.
 */
ib_status_t mock_error_body_fn(
    ib_tx_t    *tx,
    const char *data,
    size_t      len,
    void       *cbdata
)
{
    mock_error_body = std::string(data, len);

    return IB_OK;
}

/**
 * Impl for ib_server_t::err_fn.
 *
 * This captures @a status into the shared state variable
 * @ref mock_error_status. This value should be cleared before
 * this function is used.
 *
 * @param[in] tx Transaction.
 * @param[in] status The HTTP error status.
 * @param[in] cbdata Unused callback data.
 *
 * @returns IB_OK.
 */
ib_status_t mock_error_fn(
    ib_tx_t *tx,
    int status,
    void *cbdata
)
{
    mock_error_status = status;
    return IB_OK;
}
} /* Close extern "C". */
} /* Close namespace. */

/* Test that the custom error page file is served. */
TEST_F(ErrorPage, basic_file) {
    std::string config =
        std::string(
            "LogLevel DEBUG\n"
            "LoadModule \"ibmod_error_page.so\"\n"
            "LoadModule \"ibmod_rules.so\"\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "HttpStatusCodeContents 500 error_page.html\n"
            "BlockingMethod status=500\n"
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Service *:*\n"
            "   Hostname *\n"
            "   Action id:action01 rev:1 phase:request block:phase\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());

    /* Use the engine_private.h to mock the server error callbacks. */
    const_cast<ib_server_t *>(ib_engine->server)->err_fn = mock_error_fn;
    const_cast<ib_server_t *>(ib_engine->server)->err_body_fn =
        mock_error_body_fn;

    /* Clear the mock values. */
    mock_error_body = "";
    mock_error_status = 0;

    /* Perform the transaction (which populates the mock values). */
    performTx();

    /* Check the status code. */
    ASSERT_EQ(500, mock_error_status);

    /* Check if we got ANY data. */
    ASSERT_TRUE(mock_error_body.size() > 0);

    /* Read in the whole file and compare it against what the module did. */
    std::stringbuf buf;
    std::ifstream("error_page.html").get(buf, 0);
    ASSERT_EQ(
        buf.str(),
        mock_error_body);
}
