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
/// @brief IronBee --- TxLog module tests
///
/// @author Sam Baskinger <sbaskinger@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "base_fixture.h"

#include "../txlog.h"

#include <string>

class TxLogTest :
    public BaseTransactionFixture
{
public:
    virtual void sendRequestLine()
    {
        BaseTransactionFixture::sendRequestLine("GET", "/foo/bar", "HTTP/1.1");
    }
};

extern "C" {
    std::ostringstream test_log;

    void test_record_writer(void *element, void *cbdata)
    {
        ib_logger_standard_msg_t *stdmsg =
            reinterpret_cast<ib_logger_standard_msg_t *>(element);

        if (stdmsg->prefix != NULL) {
            test_log << stdmsg->prefix;
        }

        if (stdmsg->msg != NULL) {
            test_log << std::string(
                reinterpret_cast<char *>(stdmsg->msg),
                stdmsg->msg_sz);
        }

        test_log << "\n";

        ib_logger_standard_msg_free(stdmsg);

    }
    ib_status_t test_record_handler(
        ib_logger_t        *logger,
        ib_logger_writer_t *writer,
        void               *data
    )
    {
        ib_logger_dequeue(logger, writer, test_record_writer, NULL);
        return IB_OK;
    }
}

TEST_F(TxLogTest, Load) {

    const ib_txlog_module_cfg_t *cfg;

    std::string config =
        std::string(
            "LogLevel INFO\n"
            "LoadModule \"ibmod_rules.so\"\n"
            "LoadModule \"ibmod_txlog.so\"\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname somesite.com\n"
            "   Action id:1 rev:1  phase:request_header event\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());


    ASSERT_EQ(
        IB_OK,
        ib_txlog_get_config(ib_engine, ib_context_main(ib_engine), &cfg));

    ASSERT_FALSE(cfg->logger_format_fn == NULL);
    ASSERT_EQ(
        IB_OK,
        ib_logger_writer_add(
            ib_engine_logger_get(ib_engine),
            NULL, NULL,                  /* Open. */
            NULL, NULL,                  /* Close. */
            NULL, NULL,                  /* Reopen. */
            cfg->logger_format_fn, NULL, /* Format. */
            test_record_handler,   NULL  /* Record. */
        )
    );


    performTx();
    ASSERT_TRUE(ib_tx);
    std::cout << "Log string is: " << test_log.str();
}
