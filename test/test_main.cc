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
        home = (char *)"./files";
        
        cfg = htp_config_create();
        htp_config_set_server_personality(cfg, HTP_SERVER_APACHE_2_2);
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

    if (bstr_cmp_c(tx->request_method, "GET") != 0) {
        FAIL();
    }
    
    if (bstr_cmp_c(tx->request_uri, "/?p=%20") != 0) {
        FAIL();
    }
    
    ASSERT_TRUE(tx->parsed_uri != NULL);
    
    ASSERT_TRUE(tx->parsed_uri->query != NULL);
    
    ASSERT_TRUE(bstr_cmp_c(tx->parsed_uri->query, "p=%20"));
    
    ASSERT_TRUE(tx->request_params_query != NULL);
    
    bstr *p = (bstr *)table_get_c(tx->request_params_query, "p");
    ASSERT_TRUE(p != NULL);
    
    ASSERT_EQ(bstr_cmp_c(p, " "), 0);

    SUCCEED();
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

    SUCCEED();
}

TEST_F(ConnectionParsingTest, PostUrlencoded) {
    int rc = test_run(home, "03-post-urlencoded.t", cfg, &connp);
    ASSERT_GE(rc, 0);
    
    ASSERT_EQ(list_size(connp->conn->transactions), 2);
    
    htp_tx_t *tx1 = (htp_tx_t *)list_get(connp->conn->transactions, 0);
    ASSERT_TRUE(tx1 != NULL);
}