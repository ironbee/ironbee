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
/// @brief IronBee &mdash; PCRE module tests
/// 
/// @author Brian Rectanus <brectanus@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

#include "base_fixture.h"
#include <ironbee/operator.h>
#include <ironbee/hash.h>
#include <ironbee/mpool.h>
#include <ironbee/field.h>
#include <ironbee/bytestr.h>

class DfaModuleTest : public BaseModuleFixture {
public:

    ib_conn_t *ib_conn;
    ib_tx_t *ib_tx;

    DfaModuleTest() : BaseModuleFixture("ibmod_pcre.so") 
    {
    }

    virtual void SetUp() {
        BaseModuleFixture::SetUp();

        configureIronBee();

        ib_conn = buildIronBeeConnection();

        // Create the transaction.
        sendDataIn(ib_conn,
                   "GET / HTTP/1.1\r\n"
                   "Host: UnitTest\r\n"
                   "X-MyHeader: header1\r\n"
                   "X-MyHeader: header2\r\n"
                   "\r\n");

        sendDataOut(ib_conn,
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/html\r\n"
                    "X-MyHeader: header3\r\n"
                    "X-MyHeader: header4\r\n"
                    "\r\n");

        assert(ib_conn->tx!=NULL);
        ib_tx = ib_conn->tx;

    }
};

TEST_F(DfaModuleTest, matches)
{
    /* Not much more to check right now. */
    /* If we do not crash, we are generally OK. */
    ASSERT_TRUE(ib_tx);
}

