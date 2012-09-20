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
        htp_config_destroy(cfg);
    }

    htp_cfg_t *cfg;
    
    char *home;
};

TEST_F(ConnectionParsingTest, Get) {
    htp_connp_t *connp = NULL;

    int rc = test_run(home, "01-get.t", cfg, &connp);
    if (rc < 0) {
        if (connp != NULL) {
            htp_connp_destroy_all(connp);
        }
        
        FAIL();
    }

    htp_connp_destroy_all(connp);

    SUCCEED();
}
