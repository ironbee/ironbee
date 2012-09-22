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
#include "test.h"

class ConnectionParsingTest : public testing::Test {
    
protected:
    
    virtual void SetUp() {
        // Try the current working directory first
        int fd = open("./files/anchor.empty", 0, O_RDONLY);
        if (fd != -1) {
            close(fd);
            home = (char *)"./files";
        } else {
            int fd = open("./test/files/anchor.empty", 0, O_RDONLY);
            if (fd != -1) {
                close(fd);
                home = (char *)"./test/files";
            }
        }
        
        cfg = htp_config_create();
        htp_config_set_server_personality(cfg, HTP_SERVER_APACHE_2_2);
        htp_config_register_urlencoded_parser(cfg);
        htp_config_register_multipart_parser(cfg);
    }
    
    virtual void TearDown() {
        htp_connp_destroy_all(connp);
        htp_config_destroy(cfg);
    }
    
    htp_connp_t *connp;

    htp_cfg_t *cfg;
    
    char *home;
};

TEST_F(ConnectionParsingTest, Get) {
    int rc = test_run(home, "01-get.t", cfg, &connp);
    ASSERT_GE(rc, 0);
    
    ASSERT_EQ(list_size(connp->conn->transactions), 1);
    
    htp_tx_t *tx = (htp_tx_t *)list_get(connp->conn->transactions, 0);
    ASSERT_TRUE(tx != NULL);

    ASSERT_EQ(bstr_cmp_c(tx->request_method, "GET"), 0);
    
    ASSERT_EQ(bstr_cmp_c(tx->request_uri, "/?p=%20"), 0);
    
    ASSERT_TRUE(tx->parsed_uri != NULL);
    
    ASSERT_TRUE(tx->parsed_uri->query != NULL);
    
    ASSERT_TRUE(bstr_cmp_c(tx->parsed_uri->query, "p=%20") == 0);
    
    ASSERT_TRUE(tx->request_params_query != NULL);
    
    bstr *p = (bstr *)table_get_c(tx->request_params_query, "p");
    ASSERT_TRUE(p != NULL);
    
    ASSERT_EQ(bstr_cmp_c(p, " "), 0);
}

TEST_F(ConnectionParsingTest, ApacheHeaderParsing) {
    int rc = test_run(home, "02-header-test-apache2.t", cfg, &connp);
    ASSERT_GE(rc, 0);
    
    ASSERT_EQ(list_size(connp->conn->transactions), 1);
    
    htp_tx_t *tx = (htp_tx_t *)list_get(connp->conn->transactions, 0);
    ASSERT_TRUE(tx != NULL);

    ASSERT_EQ(table_size(tx->request_headers), 9);

    // Check every header
    int count = 0;
    bstr *key = NULL;
    htp_header_t *h = NULL;
    table_iterator_reset(tx->request_headers);
    while ((key = table_iterator_next(tx->request_headers, (void **) & h)) != NULL) {

        switch (count) {
            case 0:
                ASSERT_EQ(bstr_cmp_c(h->name, " Invalid-Folding"), 0);
                ASSERT_EQ(bstr_cmp_c(h->value, "1"), 0);
                break;
            case 1:
                ASSERT_EQ(bstr_cmp_c(h->name, "Valid-Folding"), 0);
                ASSERT_EQ(bstr_cmp_c(h->value, "2 2"), 0);
                break;
            case 2:
                ASSERT_EQ(bstr_cmp_c(h->name, "Normal-Header"), 0);
                ASSERT_EQ(bstr_cmp_c(h->value, "3"), 0);
                break;
            case 3:
                ASSERT_EQ(bstr_cmp_c(h->name, "Invalid Header Name"), 0);
                ASSERT_EQ(bstr_cmp_c(h->value, "4"), 0);
                break;
            case 4:
                ASSERT_EQ(bstr_cmp_c(h->name, "Same-Name-Headers"), 0);
                ASSERT_EQ(bstr_cmp_c(h->value, "5, 6"), 0);
                break;
            case 5:
                ASSERT_EQ(bstr_cmp_c(h->name, "Empty-Value-Header"), 0);
                ASSERT_EQ(bstr_cmp_c(h->value, ""), 0);
                break;
            case 6:
                ASSERT_EQ(bstr_cmp_c(h->name, ""), 0);
                ASSERT_EQ(bstr_cmp_c(h->value, "8, "), 0);
                break;
            case 7:
                ASSERT_EQ(bstr_cmp_c(h->name, "Header-With-LWS-After"), 0);
                ASSERT_EQ(bstr_cmp_c(h->value, "9"), 0);
                break;
            case 8:
                ASSERT_EQ(bstr_cmp_c(h->name, "Header-With-NUL"), 0);
                bstr *b = bstr_dup_mem("BEFORE", 6);
                if (bstr_cmp(h->value, b) != 0)  {
                    bstr_free(&b);
                    FAIL() << "Incorrect value for Header-With-NUL";
                }
                break;
        }

        count++;
    }
}

TEST_F(ConnectionParsingTest, PostUrlencoded) {
    int rc = test_run(home, "03-post-urlencoded.t", cfg, &connp);
    ASSERT_GE(rc, 0);
    
    ASSERT_EQ(list_size(connp->conn->transactions), 2);
    
    htp_tx_t *tx = (htp_tx_t *)list_get(connp->conn->transactions, 0);
    ASSERT_TRUE(tx != NULL);
    
    ASSERT_TRUE(tx->request_params_body != NULL);
    
    bstr *p = (bstr *)table_get_c(tx->request_params_body, "p");
    ASSERT_TRUE(p != NULL);
    
    ASSERT_EQ(bstr_cmp_c(p, "0123456789"), 0);
}

TEST_F(ConnectionParsingTest, PostUrlencodedChunked) {
    int rc = test_run(home, "04-post-urlencoded-chunked.t", cfg, &connp);
    ASSERT_GE(rc, 0);
    
    ASSERT_EQ(list_size(connp->conn->transactions), 1);
    
    htp_tx_t *tx = (htp_tx_t *)list_get(connp->conn->transactions, 0);
    ASSERT_TRUE(tx != NULL);
    
    ASSERT_TRUE(tx->request_params_body != NULL);
    
    bstr *p = (bstr *)table_get_c(tx->request_params_body, "p");
    ASSERT_TRUE(p != NULL);
    
    ASSERT_EQ(bstr_cmp_c(p, "0123456789"), 0);
}

TEST_F(ConnectionParsingTest, Expect) {
    int rc = test_run(home, "05-expect.t", cfg, &connp);
    ASSERT_GE(rc, 0);
    
    ASSERT_EQ(list_size(connp->conn->transactions), 1);
    
    htp_tx_t *tx = (htp_tx_t *)list_get(connp->conn->transactions, 0);
    ASSERT_TRUE(tx != NULL);
}

TEST_F(ConnectionParsingTest, UriNormal) {
    int rc = test_run(home, "06-uri-normal.t", cfg, &connp);
    ASSERT_GE(rc, 0);
    
    ASSERT_EQ(list_size(connp->conn->transactions), 1);
    
    htp_tx_t *tx = (htp_tx_t *)list_get(connp->conn->transactions, 0);
    ASSERT_TRUE(tx != NULL);
}

TEST_F(ConnectionParsingTest, PipelinedConn) {
    int rc = test_run(home, "07-pipelined-connection.t", cfg, &connp);
    ASSERT_GE(rc, 0);
    
    ASSERT_EQ(list_size(connp->conn->transactions), 2);
    
    ASSERT_TRUE(connp->conn->flags & PIPELINED_CONNECTION);
    
    htp_tx_t *tx = (htp_tx_t *)list_get(connp->conn->transactions, 0);
    ASSERT_TRUE(tx != NULL);
}

TEST_F(ConnectionParsingTest, NotPipelinedConn) {
    int rc = test_run(home, "08-not-pipelined-connection.t", cfg, &connp);
    ASSERT_GE(rc, 0);
    
    ASSERT_EQ(list_size(connp->conn->transactions), 2);
    
    ASSERT_FALSE(connp->conn->flags & PIPELINED_CONNECTION);
    
    htp_tx_t *tx = (htp_tx_t *)list_get(connp->conn->transactions, 0);
    ASSERT_TRUE(tx != NULL);
    
    ASSERT_FALSE(tx->flags & HTP_MULTI_PACKET_HEAD);
}

TEST_F(ConnectionParsingTest, MultiPacketRequest) {
    int rc = test_run(home, "09-multi-packet-request-head.t", cfg, &connp);
    ASSERT_GE(rc, 0);
    
    ASSERT_EQ(list_size(connp->conn->transactions), 1);
    
    htp_tx_t *tx = (htp_tx_t *)list_get(connp->conn->transactions, 0);
    ASSERT_TRUE(tx != NULL);
    
    ASSERT_TRUE(tx->flags & HTP_MULTI_PACKET_HEAD);
}

TEST_F(ConnectionParsingTest, HeaderHostParsing) {
    int rc = test_run(home, "10-host-in-headers.t", cfg, &connp);
    ASSERT_GE(rc, 0);
    
    ASSERT_EQ(list_size(connp->conn->transactions), 4);
    
    htp_tx_t *tx1 = (htp_tx_t *)list_get(connp->conn->transactions, 0);
    ASSERT_TRUE(tx1 != NULL);
    ASSERT_TRUE(tx1->parsed_uri->hostname != NULL);
    ASSERT_EQ(bstr_cmp_c(tx1->parsed_uri->hostname, "www.example.com"), 0);
    
    htp_tx_t *tx2 = (htp_tx_t *)list_get(connp->conn->transactions, 1);
    ASSERT_TRUE(tx2 != NULL);
    ASSERT_TRUE(tx2->parsed_uri->hostname != NULL);
    ASSERT_EQ(bstr_cmp_c(tx2->parsed_uri->hostname, "www.example.com"), 0);
    
    htp_tx_t *tx3 = (htp_tx_t *)list_get(connp->conn->transactions, 2);
    ASSERT_TRUE(tx3 != NULL);
    ASSERT_TRUE(tx3->parsed_uri->hostname != NULL);
    ASSERT_EQ(bstr_cmp_c(tx3->parsed_uri->hostname, "www.example.com"), 0);
    
    htp_tx_t *tx4 = (htp_tx_t *)list_get(connp->conn->transactions, 3);
    ASSERT_TRUE(tx4 != NULL);
    ASSERT_TRUE(tx4->parsed_uri->hostname != NULL);
    ASSERT_EQ(bstr_cmp_c(tx4->parsed_uri->hostname, "www.example.com"), 0);
}

TEST_F(ConnectionParsingTest, ResponseWithoutContentLength) {
    int rc = test_run(home, "11-response-stream-closure.t", cfg, &connp);
    ASSERT_GE(rc, 0);
    
    ASSERT_EQ(list_size(connp->conn->transactions), 1);
    
    htp_tx_t *tx = (htp_tx_t *)list_get(connp->conn->transactions, 0);
    ASSERT_TRUE(tx != NULL);
    
    ASSERT_TRUE(tx->progress == TX_PROGRESS_DONE);
}

TEST_F(ConnectionParsingTest, FailedConnectRequest) {
    int rc = test_run(home, "12-connect-request.t", cfg, &connp);
    ASSERT_GE(rc, 0);
    
    ASSERT_EQ(list_size(connp->conn->transactions), 1);
    
    htp_tx_t *tx = (htp_tx_t *)list_get(connp->conn->transactions, 0);
    ASSERT_TRUE(tx != NULL);
    
    ASSERT_TRUE(tx->progress == TX_PROGRESS_DONE);
    
    // TODO Check response status is correct
}

TEST_F(ConnectionParsingTest, CompressedResponseContentType) {
    int rc = test_run(home, "13-compressed-response-gzip-ct.t", cfg, &connp);
    ASSERT_GE(rc, 0);
    
    ASSERT_EQ(list_size(connp->conn->transactions), 1);
    
    htp_tx_t *tx = (htp_tx_t *)list_get(connp->conn->transactions, 0);
    ASSERT_TRUE(tx != NULL);
    
    ASSERT_TRUE(tx->progress == TX_PROGRESS_DONE);
    
    // TODO Check that the response was correctly decompressed (content, length)
}

TEST_F(ConnectionParsingTest, CompressedResponseChunked) {
    int rc = test_run(home, "14-compressed-response-gzip-chunked.t", cfg, &connp);
    ASSERT_GE(rc, 0);
    
    ASSERT_EQ(list_size(connp->conn->transactions), 1);
    
    htp_tx_t *tx = (htp_tx_t *)list_get(connp->conn->transactions, 0);
    ASSERT_TRUE(tx != NULL);
    
    ASSERT_TRUE(tx->progress == TX_PROGRESS_DONE);
    
    // TODO Check that the response was correctly decompressed (content, length)
}

TEST_F(ConnectionParsingTest, SuccessfulConnectRequest) {
    int rc = test_run(home, "15-connect-complete.t", cfg, &connp);
    ASSERT_GE(rc, 0);
    
    ASSERT_EQ(list_size(connp->conn->transactions), 1);
    
    htp_tx_t *tx = (htp_tx_t *)list_get(connp->conn->transactions, 0);
    ASSERT_TRUE(tx != NULL);
    
    ASSERT_TRUE(tx->progress == TX_PROGRESS_DONE);
    
    // TODO Check response status is correct
}

TEST_F(ConnectionParsingTest, ConnectRequestWithExtraData) {
    int rc = test_run(home, "16-connect-extra.t", cfg, &connp);
    ASSERT_GE(rc, 0);
    
    ASSERT_EQ(list_size(connp->conn->transactions), 2);
    
    htp_tx_t *tx1 = (htp_tx_t *)list_get(connp->conn->transactions, 0);
    ASSERT_TRUE(tx1 != NULL);
    
    ASSERT_TRUE(tx1->progress == TX_PROGRESS_DONE);
    
    htp_tx_t *tx2 = (htp_tx_t *)list_get(connp->conn->transactions, 1);
    ASSERT_TRUE(tx2 != NULL);
    
    ASSERT_TRUE(tx2->progress == TX_PROGRESS_DONE);
}

TEST_F(ConnectionParsingTest, Multipart) {
    int rc = test_run(home, "17-multipart-1.t", cfg, &connp);
    ASSERT_GE(rc, 0);
    
    ASSERT_EQ(list_size(connp->conn->transactions), 1);
    
    htp_tx_t *tx = (htp_tx_t *)list_get(connp->conn->transactions, 0);
    ASSERT_TRUE(tx != NULL);
    
    ASSERT_TRUE(tx->progress == TX_PROGRESS_DONE);
    
    ASSERT_TRUE(tx->request_params_body != NULL);
    
    bstr *field1 = (bstr *)table_get_c(tx->request_params_body, "field1");
    // XXX
    // ASSERT_TRUE(field1 != NULL);
    
    bstr *field2 = (bstr *)table_get_c(tx->request_params_body, "field2");
    // XXX
    // ASSERT_TRUE(field2 != NULL);
}

TEST_F(ConnectionParsingTest, CompressedResponseDeflate) {
    int rc = test_run(home, "18-compressed-response-deflate.t", cfg, &connp);
    ASSERT_GE(rc, 0);
    
    ASSERT_EQ(list_size(connp->conn->transactions), 1);
    
    htp_tx_t *tx = (htp_tx_t *)list_get(connp->conn->transactions, 0);
    ASSERT_TRUE(tx != NULL);
    
    ASSERT_TRUE(tx->progress == TX_PROGRESS_DONE);
    
    // TODO More checks
}

TEST_F(ConnectionParsingTest, UrlEncoded) {
    int rc = test_run(home, "19-urlencoded-test.t", cfg, &connp);
    ASSERT_GE(rc, 0);
    
    ASSERT_EQ(list_size(connp->conn->transactions), 1);
    
    htp_tx_t *tx = (htp_tx_t *)list_get(connp->conn->transactions, 0);
    ASSERT_TRUE(tx != NULL);
    
    ASSERT_TRUE(tx->progress == TX_PROGRESS_DONE);
    
    ASSERT_EQ(bstr_cmp_c(tx->request_method, "POST"), 0);
    ASSERT_EQ(bstr_cmp_c(tx->request_uri, "/?p=1&q=2"), 0);
    
    ASSERT_TRUE(tx->request_params_body != NULL);
    
    bstr *body_p = (bstr *)table_get_c(tx->request_params_body, "p");
    ASSERT_TRUE(body_p != NULL);
    ASSERT_EQ(bstr_cmp_c(body_p, "3"), 0);
    
    bstr *body_q = (bstr *)table_get_c(tx->request_params_body, "q");
    ASSERT_TRUE(body_q != NULL);
    ASSERT_EQ(bstr_cmp_c(body_q, "4"), 0);
    
    bstr *body_z = (bstr *)table_get_c(tx->request_params_body, "z");
    ASSERT_TRUE(body_z != NULL);
    ASSERT_EQ(bstr_cmp_c(body_z, "5"), 0);
}

TEST_F(ConnectionParsingTest, AmbiguousHost) {
    int rc = test_run(home, "20-ambiguous-host.t", cfg, &connp);
    ASSERT_GE(rc, 0);
    
    ASSERT_EQ(list_size(connp->conn->transactions), 2);
    
    htp_tx_t *tx1 = (htp_tx_t *)list_get(connp->conn->transactions, 0);
    ASSERT_TRUE(tx1 != NULL);
    ASSERT_TRUE(tx1->progress == TX_PROGRESS_DONE);
    ASSERT_FALSE(tx1->flags & HTP_AMBIGUOUS_HOST);
    
    htp_tx_t *tx2 = (htp_tx_t *)list_get(connp->conn->transactions, 1);
    ASSERT_TRUE(tx2 != NULL);
    ASSERT_TRUE(tx2->progress == TX_PROGRESS_DONE);
    ASSERT_TRUE(tx2->flags & HTP_AMBIGUOUS_HOST);
}

TEST_F(ConnectionParsingTest, Http_0_9) {
    int rc = test_run(home, "21-http09.t", cfg, &connp);
    ASSERT_GE(rc, 0);
    
    ASSERT_EQ(list_size(connp->conn->transactions), 1);
    
    htp_tx_t *tx = (htp_tx_t *)list_get(connp->conn->transactions, 0);
    ASSERT_TRUE(tx != NULL);
}





