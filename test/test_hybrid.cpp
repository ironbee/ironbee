/***************************************************************************
 * Copyright (c) 2009-2010 Open Information Security Foundation
 * Copyright (c) 2010-2013 Qualys, Inc.
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
 * @author Ivan Ristic <ivanr@webkreator.com>
 */

#include <iostream>
#include <gtest/gtest.h>
#include <htp/htp.h>
#include <htp/htp_transaction.h>
#include <htp/htp_base64.h>
#include "test.h"

class HybridParsing : public testing::Test {
protected:

    virtual void SetUp() {
        cfg = htp_config_create();
        htp_config_set_server_personality(cfg, HTP_SERVER_APACHE_2);
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

struct HybridParsing_Get_User_Data {
    // Request callback indicators
    int callback_TRANSACTION_START_invoked;
    int callback_REQUEST_LINE_invoked;
    int callback_REQUEST_HEADERS_invoked;
    int callback_REQUEST_COMPLETE_invoked;

    // Response callback indicators
    int callback_RESPONSE_START_invoked;
    int callback_HTP_RESPONSE_LINE_invoked;
    int callback_RESPONSE_HEADERS_invoked;
    int callback_HTP_RESPONSE_COMPLETE_invoked;

    // Response body handling fields
    int response_body_chunks_seen;
    int response_body_correctly_received;
};

static int HybridParsing_Get_Callback_TRANSACTION_START(htp_connp_t *connp) {
    struct HybridParsing_Get_User_Data *user_data = (struct HybridParsing_Get_User_Data *) htp_tx_get_user_data(connp->in_tx);
    user_data->callback_TRANSACTION_START_invoked = 1;
    return HTP_OK;
}

static int HybridParsing_Get_Callback_REQUEST_LINE(htp_connp_t *connp) {
    struct HybridParsing_Get_User_Data *user_data = (struct HybridParsing_Get_User_Data *) htp_tx_get_user_data(connp->in_tx);
    user_data->callback_REQUEST_LINE_invoked = 1;
    return HTP_OK;
}

static int HybridParsing_Get_Callback_REQUEST_HEADERS(htp_connp_t *connp) {
    struct HybridParsing_Get_User_Data *user_data = (struct HybridParsing_Get_User_Data *) htp_tx_get_user_data(connp->in_tx);
    user_data->callback_REQUEST_HEADERS_invoked = 1;
    return HTP_OK;
}

static int HybridParsing_Get_Callback_REQUEST_COMPLETE(htp_connp_t *connp) {
    struct HybridParsing_Get_User_Data *user_data = (struct HybridParsing_Get_User_Data *) htp_tx_get_user_data(connp->in_tx);
    user_data->callback_REQUEST_COMPLETE_invoked = 1;
    return HTP_OK;
}

static int HybridParsing_Get_Callback_RESPONSE_START(htp_connp_t *connp) {
    struct HybridParsing_Get_User_Data *user_data = (struct HybridParsing_Get_User_Data *) htp_tx_get_user_data(connp->in_tx);
    user_data->callback_RESPONSE_START_invoked = 1;
    return HTP_OK;
}

static int HybridParsing_Get_Callback_HTP_RESPONSE_LINE(htp_connp_t *connp) {
    struct HybridParsing_Get_User_Data *user_data = (struct HybridParsing_Get_User_Data *) htp_tx_get_user_data(connp->in_tx);
    user_data->callback_HTP_RESPONSE_LINE_invoked = 1;
    return HTP_OK;
}

static int HybridParsing_Get_Callback_RESPONSE_HEADERS(htp_connp_t *connp) {
    struct HybridParsing_Get_User_Data *user_data = (struct HybridParsing_Get_User_Data *) htp_tx_get_user_data(connp->in_tx);
    user_data->callback_RESPONSE_HEADERS_invoked = 1;
    return HTP_OK;
}

static int HybridParsing_Get_Callback_RESPONSE_BODY_DATA(htp_tx_data_t *d) {
    struct HybridParsing_Get_User_Data *user_data = (struct HybridParsing_Get_User_Data *) htp_tx_get_user_data(d->tx);   

    // Don't do anything if in errored state
    if (user_data->response_body_correctly_received == -1) return HTP_ERROR;

    switch (user_data->response_body_chunks_seen) {
        case 0:
            if ((d->len == 9)&&(memcmp(d->data, "<h1>Hello", 9) == 0)) {
                user_data->response_body_chunks_seen++;
            } else {
                SCOPED_TRACE("Mismatch in 1st chunk");
                user_data->response_body_correctly_received = -1;
            }
            break;
        case 1:
            if ((d->len == 1)&&(memcmp(d->data, " ", 1) == 0)) {
                user_data->response_body_chunks_seen++;
            } else {
                SCOPED_TRACE("Mismatch in 2nd chunk");
                user_data->response_body_correctly_received = -1;
            }
            break;
        case 2:
            if ((d->len == 11)&&(memcmp(d->data, "World!</h1>", 11) == 0)) {
                user_data->response_body_chunks_seen++;
                user_data->response_body_correctly_received = 1;
            } else {
                SCOPED_TRACE("Mismatch in 3rd chunk");
                user_data->response_body_correctly_received = -1;
            }
            break;
        default:
            SCOPED_TRACE("Seen more than 3 chunks");
            user_data->response_body_correctly_received = -1;
            break;
    }

    return HTP_OK;
}

static int HybridParsing_Get_Callback_HTP_RESPONSE_COMPLETE(htp_connp_t *connp) {
    struct HybridParsing_Get_User_Data *user_data = (struct HybridParsing_Get_User_Data *) htp_tx_get_user_data(connp->in_tx);
    user_data->callback_HTP_RESPONSE_COMPLETE_invoked = 1;
    return HTP_OK;
}

/**
 * Test hybrid mode with one complete GET transaction; request then response
 * with a body. Most features are tested, including query string parameters and callbacks.
 */
TEST_F(HybridParsing, GetTest) {
    // Create a new LibHTP transaction
    htp_tx_t *tx = htp_connp_tx_create(connp);
    ASSERT_TRUE(tx != NULL);

    // Configure user data and callbacks
    struct HybridParsing_Get_User_Data user_data;
    user_data.callback_TRANSACTION_START_invoked = 0;
    user_data.callback_REQUEST_LINE_invoked = 0;
    user_data.callback_REQUEST_HEADERS_invoked = 0;
    user_data.callback_REQUEST_COMPLETE_invoked = 0;
    user_data.callback_RESPONSE_START_invoked = 0;
    user_data.callback_HTP_RESPONSE_LINE_invoked = 0;
    user_data.callback_RESPONSE_HEADERS_invoked = 0;
    user_data.callback_HTP_RESPONSE_COMPLETE_invoked = 0;
    user_data.response_body_chunks_seen = 0;
    user_data.response_body_correctly_received = 0;
    htp_tx_set_user_data(tx, &user_data);

    // Request callbacks
    htp_config_register_request_start(cfg, HybridParsing_Get_Callback_TRANSACTION_START);
    htp_config_register_request_line(cfg, HybridParsing_Get_Callback_REQUEST_LINE);
    htp_config_register_request_headers(cfg, HybridParsing_Get_Callback_REQUEST_HEADERS);
    htp_config_register_request_complete(cfg, HybridParsing_Get_Callback_REQUEST_COMPLETE);

    // Response callbacks
    htp_config_register_response_start(cfg, HybridParsing_Get_Callback_RESPONSE_START);
    htp_config_register_response_line(cfg, HybridParsing_Get_Callback_HTP_RESPONSE_LINE);
    htp_config_register_response_headers(cfg, HybridParsing_Get_Callback_RESPONSE_HEADERS);
    htp_config_register_response_body_data(cfg, HybridParsing_Get_Callback_RESPONSE_BODY_DATA);
    htp_config_register_response_complete(cfg, HybridParsing_Get_Callback_HTP_RESPONSE_COMPLETE);

    // Request begins
    htp_tx_state_request_start(tx);
    ASSERT_EQ(user_data.callback_TRANSACTION_START_invoked, 1);

    // Request line data
    htp_tx_req_set_method_c(tx, "GET", HTP_ALLOC_COPY);
    htp_tx_req_set_method_number(tx, HTP_M_GET);
    htp_tx_req_set_uri_c(tx, "/", HTP_ALLOC_COPY);
    htp_tx_req_set_query_string_c(tx, "p=1&q=2", HTP_ALLOC_COPY);
    htp_tx_req_set_protocol_c(tx, "HTTP/1.1", HTP_ALLOC_COPY);
    htp_tx_req_set_protocol_number(tx, HTP_PROTOCOL_1_1);
    htp_tx_req_set_protocol_0_9(tx, 0);

    // Request line complete
    htp_tx_state_request_line(tx);
    ASSERT_EQ(user_data.callback_REQUEST_LINE_invoked, 1);

    // Check request line data
    ASSERT_EQ(bstr_cmp_c(tx->request_method, "GET"), 0);
    ASSERT_EQ(bstr_cmp_c(tx->request_uri, "/"), 0);
    ASSERT_EQ(bstr_cmp_c(tx->request_protocol, "HTTP/1.1"), 0);

    ASSERT_TRUE(tx->parsed_uri != NULL);
    ASSERT_EQ(bstr_cmp_c(tx->parsed_uri->query, "p=1&q=2"), 0);

    // Check parameters
    htp_param_t *param_p = htp_tx_req_get_param_c(tx, "p");
    ASSERT_TRUE(param_p != NULL);
    ASSERT_EQ(bstr_cmp_c(param_p->value, "1"), 0);

    htp_param_t *param_q = htp_tx_req_get_param_c(tx, "q");
    ASSERT_TRUE(param_q != NULL);
    ASSERT_EQ(bstr_cmp_c(param_q->value, "2"), 0);

    // Request headers
    htp_tx_req_set_header_c(tx, "Host", "www.example.com", HTP_ALLOC_COPY);
    htp_tx_req_set_header_c(tx, "Connection", "keep-alive", HTP_ALLOC_COPY);
    htp_tx_req_set_header_c(tx, "User-Agent", "Mozilla/5.0", HTP_ALLOC_COPY);

    // Request headers complete
    htp_tx_state_request_headers(tx);

    // Check headers
    ASSERT_EQ(user_data.callback_REQUEST_HEADERS_invoked, 1);

    htp_header_t *h_host = (htp_header_t *) htp_table_get_c(tx->request_headers, "host");
    ASSERT_TRUE(h_host != NULL);
    ASSERT_EQ(bstr_cmp_c(h_host->value, "www.example.com"), 0);

    htp_header_t *h_connection = (htp_header_t *) htp_table_get_c(tx->request_headers, "connection");
    ASSERT_TRUE(h_connection != NULL);
    ASSERT_EQ(bstr_cmp_c(h_connection->value, "keep-alive"), 0);

    htp_header_t *h_ua = (htp_header_t *) htp_table_get_c(tx->request_headers, "user-agent");
    ASSERT_TRUE(h_ua != NULL);
    ASSERT_EQ(bstr_cmp_c(h_ua->value, "Mozilla/5.0"), 0);

    // Request complete
    htp_tx_state_request_complete(tx);
    ASSERT_EQ(user_data.callback_REQUEST_COMPLETE_invoked, 1);

    // Response begins
    htp_tx_state_response_start(tx);
    ASSERT_EQ(user_data.callback_RESPONSE_START_invoked, 1);

    // Response line data
    htp_tx_res_set_status_line_c(tx, "HTTP/1.1 200 OK", HTP_ALLOC_COPY);
    ASSERT_EQ(bstr_cmp_c(tx->response_protocol, "HTTP/1.1"), 0);
    ASSERT_EQ(tx->response_protocol_number, HTP_PROTOCOL_1_1);
    ASSERT_EQ(tx->response_status_number, 200);
    ASSERT_EQ(bstr_cmp_c(tx->response_message, "OK"), 0);

    htp_tx_res_set_protocol_number(tx, HTP_PROTOCOL_1_0);
    ASSERT_EQ(tx->response_protocol_number, HTP_PROTOCOL_1_0);

    htp_tx_res_set_status_code(tx, 500);
    ASSERT_EQ(tx->response_status_number, 500);

    htp_tx_res_set_status_message(tx, "Internal Server Error", HTP_ALLOC_COPY);
    ASSERT_EQ(bstr_cmp_c(tx->response_message, "Internal Server Error"), 0);

    // Response line complete
    htp_tx_state_response_line(tx);
    ASSERT_EQ(user_data.callback_HTP_RESPONSE_LINE_invoked, 1);

    // Response header data
    htp_tx_res_set_header_c(tx, "Content-Type", "text/html", HTP_ALLOC_COPY);
    htp_tx_res_set_header_c(tx, "Server", "Apache", HTP_ALLOC_COPY);

    // Response headers complete
    htp_tx_state_response_headers(tx);
    ASSERT_EQ(user_data.callback_RESPONSE_HEADERS_invoked, 1);

    // Check response headers
    htp_header_t *h_content_type = (htp_header_t *) htp_table_get_c(tx->response_headers, "content-type");
    ASSERT_TRUE(h_content_type != NULL);
    ASSERT_EQ(bstr_cmp_c(h_content_type->value, "text/html"), 0);

    htp_header_t *h_server = (htp_header_t *) htp_table_get_c(tx->response_headers, "server");
    ASSERT_TRUE(h_server != NULL);
    ASSERT_EQ(bstr_cmp_c(h_server->value, "Apache"), 0);

    // Request body data   
    htp_tx_res_process_body_data(tx, (const unsigned char *)"<h1>Hello", 9);
    htp_tx_res_process_body_data(tx, (const unsigned char *)" ", 1);
    htp_tx_res_process_body_data(tx, (const unsigned char *)"World!</h1>", 11);
    ASSERT_EQ(user_data.response_body_correctly_received, 1);

    // Trailing response headers
    htp_tx_res_set_headers_clear(tx);
    ASSERT_EQ(htp_table_size(tx->response_headers), 0);

    htp_tx_res_set_header_c(tx, "Content-Type", "text/html", HTP_ALLOC_COPY);
    htp_tx_res_set_header_c(tx, "Server", "Apache", HTP_ALLOC_COPY);

    // Check trailing response headers
    h_content_type = (htp_header_t *) htp_table_get_c(tx->response_headers, "content-type");
    ASSERT_TRUE(h_content_type != NULL);
    ASSERT_EQ(bstr_cmp_c(h_content_type->value, "text/html"), 0);

    h_server = (htp_header_t *) htp_table_get_c(tx->response_headers, "server");
    ASSERT_TRUE(h_server != NULL);
    ASSERT_EQ(bstr_cmp_c(h_server->value, "Apache"), 0);

    htp_tx_state_response_complete(tx);
    ASSERT_EQ(user_data.callback_HTP_RESPONSE_COMPLETE_invoked, 1);
}

/**
 * Use a POST request in order to test request body processing and parameter parsing.
 */
TEST_F(HybridParsing, PostUrlecodedTest) {
    // Create a new LibHTP transaction
    htp_tx_t *tx = htp_connp_tx_create(connp);
    ASSERT_TRUE(tx != NULL);

    // Request begins
    htp_tx_state_request_start(tx);

    // Request line data
    htp_tx_req_set_method_c(tx, "POST", HTP_ALLOC_COPY);
    htp_tx_req_set_method_number(tx, HTP_M_GET);
    htp_tx_req_set_uri_c(tx, "/", HTP_ALLOC_COPY);
    htp_tx_req_set_protocol_c(tx, "HTTP/1.1", HTP_ALLOC_COPY);
    htp_tx_req_set_protocol_number(tx, HTP_PROTOCOL_1_1);
    htp_tx_req_set_protocol_0_9(tx, 0);

    // Configure headers to trigger the URLENCODED parser
    htp_tx_req_set_header_c(tx, "Content-Type", HTP_URLENCODED_MIME_TYPE, HTP_ALLOC_COPY);
    htp_tx_req_set_header_c(tx, "Content-Length", "7", HTP_ALLOC_COPY);

    // Request headers complete
    htp_tx_state_request_headers(tx);

    // Send request body
    htp_tx_req_process_body_data(tx, (const unsigned char *) "p=1", 3);
    htp_tx_req_process_body_data(tx, (const unsigned char *) "&", 1);
    htp_tx_req_process_body_data(tx, (const unsigned char *) "q=2", 3);

    // Trailing request headers
    htp_tx_req_set_headers_clear(tx);
    ASSERT_EQ(htp_table_size(tx->request_headers), 0);

    htp_tx_req_set_header_c(tx, "Host", "www.example.com", HTP_ALLOC_COPY);
    htp_tx_req_set_header_c(tx, "Connection", "keep-alive", HTP_ALLOC_COPY);
    htp_tx_req_set_header_c(tx, "User-Agent", "Mozilla/5.0", HTP_ALLOC_COPY);

    htp_header_t *h_host = (htp_header_t *) htp_table_get_c(tx->request_headers, "host");
    ASSERT_TRUE(h_host != NULL);
    ASSERT_EQ(bstr_cmp_c(h_host->value, "www.example.com"), 0);

    htp_header_t *h_connection = (htp_header_t *) htp_table_get_c(tx->request_headers, "connection");
    ASSERT_TRUE(h_connection != NULL);
    ASSERT_EQ(bstr_cmp_c(h_connection->value, "keep-alive"), 0);

    htp_header_t *h_ua = (htp_header_t *) htp_table_get_c(tx->request_headers, "user-agent");
    ASSERT_TRUE(h_ua != NULL);
    ASSERT_EQ(bstr_cmp_c(h_ua->value, "Mozilla/5.0"), 0);

    // Request complete
    htp_tx_state_request_complete(tx);

    // Check parameters

    htp_param_t *param_p = htp_tx_req_get_param_c(tx, "p");
    ASSERT_TRUE(param_p != NULL);
    ASSERT_EQ(bstr_cmp_c(param_p->value, "1"), 0);

    htp_param_t *param_q = htp_tx_req_get_param_c(tx, "q");
    ASSERT_TRUE(param_q != NULL);
    ASSERT_EQ(bstr_cmp_c(param_q->value, "2"), 0);
}

static char HybridParsing_CompressedResponse[] =
"H4sIAAAAAAAAAG2PwQ6CMBBE73xFU++tXk2pASliAiEhPegRYUOJYEktEP5eqB6dy2ZnJ5O3LJFZ"
"yj2WiCBah7zKVPBMT1AjCf2gTWnabmH0e/AY/QXDPLqj8HLO07zw8S52wkiKm1zXvRPeeg//2lbX"
"kwpQrauxh5dFqnyj3uVYgJJCxD5W1g5HSud5Jo3WTQek0mR8UgNlDYZOLcz0ZMuH3y+YKzDAaMDJ"
"SrihOVL32QceVXUy4QAAAA==";

static void HybridParsing_CompressedResponse_Setup(htp_tx_t *tx) {
    htp_tx_state_request_start(tx);

    htp_tx_req_set_method_c(tx, "GET", HTP_ALLOC_COPY);
    htp_tx_req_set_method_number(tx, HTP_M_GET);
    htp_tx_req_set_uri_c(tx, "/", HTP_ALLOC_COPY);
    htp_tx_req_set_query_string_c(tx, "p=1&q=2", HTP_ALLOC_COPY);
    htp_tx_req_set_protocol_c(tx, "HTTP/1.1", HTP_ALLOC_COPY);
    htp_tx_req_set_protocol_number(tx, HTP_PROTOCOL_1_1);
    htp_tx_req_set_protocol_0_9(tx, 0);

    htp_tx_state_request_headers(tx);
    htp_tx_state_request_complete(tx);

    htp_tx_state_response_start(tx);

    htp_tx_res_set_status_line_c(tx, "HTTP/1.1 200 OK", HTP_ALLOC_COPY);
    htp_tx_res_set_header_c(tx, "Content-Encoding", "gzip", HTP_ALLOC_COPY);
    htp_tx_res_set_header_c(tx, "Content-Length", "187", HTP_ALLOC_COPY);

    htp_tx_state_response_headers(tx);

    bstr *body = htp_base64_decode_mem(HybridParsing_CompressedResponse, strlen(HybridParsing_CompressedResponse));
    ASSERT_TRUE(body != NULL);

    htp_tx_res_process_body_data(tx, bstr_ptr(body), bstr_len(body));
    htp_tx_state_response_complete(tx);
}

/**
 * Test with a compressed response body and decompression enabled.
 */
TEST_F(HybridParsing, CompressedResponseTest) {
    // Create a new LibHTP transaction
    htp_tx_t *tx = htp_connp_tx_create(connp);
    ASSERT_TRUE(tx != NULL);

    HybridParsing_CompressedResponse_Setup(tx);

    ASSERT_EQ(tx->response_message_len, 187);    
    ASSERT_EQ(tx->response_entity_len, 225);
}

/**
 * Test with a compressed response body and decompression disabled.
 */
TEST_F(HybridParsing, CompressedResponseNoDecompressionTest) {
    // Disable decompression
    htp_config_set_response_decompression(cfg, 0);

    // Create a new LibHTP transaction
    htp_tx_t *tx = htp_connp_tx_create(connp);
    ASSERT_TRUE(tx != NULL);

    HybridParsing_CompressedResponse_Setup(tx);

    ASSERT_EQ(tx->response_message_len, 187);
    ASSERT_EQ(tx->response_entity_len, 187);
}

static int HybridParsing_ForcedDecompressionTest_Callback_RESPONSE_HEADERS(htp_connp_t *connp) {
    connp->out_tx->response_content_encoding = COMPRESSION_GZIP;
    return HTP_OK;
}

/**
 * Test forced decompression.
 */
TEST_F(HybridParsing, ForcedDecompressionTest) {
    // Disable decompression
    htp_config_set_response_decompression(cfg, 0);

    // Register a callback that will force decompression
    htp_config_register_response_headers(cfg, HybridParsing_ForcedDecompressionTest_Callback_RESPONSE_HEADERS);

    // Create a new LibHTP transaction
    htp_tx_t *tx = htp_connp_tx_create(connp);
    ASSERT_TRUE(tx != NULL);

    HybridParsing_CompressedResponse_Setup(tx);

    ASSERT_EQ(tx->response_message_len, 187);
    ASSERT_EQ(tx->response_entity_len, 225);
}

static int HybridParsing_DisableDecompressionTest_Callback_RESPONSE_HEADERS(htp_connp_t *connp) {
    connp->out_tx->response_content_encoding = COMPRESSION_NONE;
    return HTP_OK;
}

/**
 * Test disabling decompression from a callback.
 */
TEST_F(HybridParsing, DisableDecompressionTest) {
    // Disable decompression
    htp_config_set_response_decompression(cfg, 0);

    // Register a callback that will force decompression
    htp_config_register_response_headers(cfg, HybridParsing_DisableDecompressionTest_Callback_RESPONSE_HEADERS);

    // Create a new LibHTP transaction
    htp_tx_t *tx = htp_connp_tx_create(connp);
    ASSERT_TRUE(tx != NULL);

    HybridParsing_CompressedResponse_Setup(tx);

    ASSERT_EQ(tx->response_message_len, 187);
    ASSERT_EQ(tx->response_entity_len, 187);
}
