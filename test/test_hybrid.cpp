/***************************************************************************
 * Copyright (c) 2011-2012, Qualys, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * * Neither the name of the Qualys, Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ***************************************************************************/

/**
 * @file
 *
 * @author Ivan Ristic <ivanr@webkreator.com>
 */

#include <iostream>
#include <gtest/gtest.h>
#include <htp/htp.h>
#include <htp/htp_hybrid.h>
#include "test.h"

class HybridParsing : public testing::Test {
protected:

    virtual void SetUp() {
        cfg = htp_config_create();
        htp_config_set_server_personality(cfg, HTP_SERVER_APACHE_2_2);
        htp_config_register_urlencoded_parser(cfg);
        htp_config_register_multipart_parser(cfg);

        connp = htp_connp_create(cfg);
    }

    virtual void TearDown() {
        htp_connp_destroy_all(connp);
        htp_config_destroy(cfg);
    }

    htp_connp_t *connp;

    htp_cfg_t *cfg;
};

TEST_F(HybridParsing, Get) {
    // Create a new LibHTP transaction
    htp_tx_t *tx = htp_txh_create(connp);
    ASSERT_TRUE(tx != NULL);

    // Request begins
    htp_txh_state_request_start(tx);

    // Request line data
    htp_txh_req_set_method_c(tx, "GET", ALLOC_COPY);
    htp_txh_req_set_method_number(tx, HTP_M_GET);
    htp_txh_req_set_uri_c(tx, "/", ALLOC_COPY);
    htp_txh_req_set_query_string_c(tx, "p=1&q=2", ALLOC_COPY);
    htp_txh_req_set_protocol_c(tx, "HTTP/1.1", ALLOC_COPY);
    htp_txh_req_set_protocol_number(tx, HTTP_1_1);
    htp_txh_req_set_protocol_http_0_9(tx, 0);

    // Request line complete
    htp_txh_state_request_line(tx);

    // Check request line
    ASSERT_EQ(bstr_cmp_c(tx->request_method, "GET"), 0);
    ASSERT_EQ(bstr_cmp_c(tx->request_uri, "/"), 0);
    ASSERT_EQ(bstr_cmp_c(tx->request_protocol, "HTTP/1.1"), 0);

    ASSERT_TRUE(tx->parsed_uri != NULL);
    ASSERT_EQ(bstr_cmp_c(tx->parsed_uri->query, "p=1&q=2"), 0);

    // Check parameters
    bstr *param_p = (bstr *)table_get_c(tx->request_params_query, "p");
    ASSERT_TRUE(param_p != NULL);
    ASSERT_EQ(bstr_cmp_c(param_p, "1"), 0);

    bstr *param_q = (bstr *)table_get_c(tx->request_params_query, "q");
    ASSERT_TRUE(param_q != NULL);
    ASSERT_EQ(bstr_cmp_c(param_q, "2"), 0);

    // Request headers
    htp_txh_req_set_header_c(tx, "Host", "www.example.com", ALLOC_COPY);
    htp_txh_req_set_header_c(tx, "Connection", "keep-alive", ALLOC_COPY);
    htp_txh_req_set_header_c(tx, "User-Agent", "Mozilla/5.0", ALLOC_COPY);

    // Check headers
    htp_header_t *h_host = (htp_header_t *)table_get_c(tx->request_headers, "host");
    ASSERT_TRUE(h_host != NULL);
    ASSERT_EQ(bstr_cmp_c(h_host->value, "www.example.com"), 0);

    htp_header_t *h_connection = (htp_header_t *)table_get_c(tx->request_headers, "connection");
    ASSERT_TRUE(h_connection != NULL);
    ASSERT_EQ(bstr_cmp_c(h_connection->value, "keep-alive"), 0);

    htp_header_t *h_ua = (htp_header_t *)table_get_c(tx->request_headers, "user-agent");
    ASSERT_TRUE(h_ua != NULL);
    ASSERT_EQ(bstr_cmp_c(h_ua->value, "Mozilla/5.0"), 0);

    // Request headers complete
    htp_txh_state_request_headers(tx);
}
