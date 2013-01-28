/***************************************************************************
 * Copyright (c) 2009-2010 Open Information Security Foundation
 * Copyright (c) 2010-2013 Qualys, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.

 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.

 * - Neither the name of the Qualys, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
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

class Multipart : public testing::Test {
protected:

    virtual void SetUp() {
        cfg = htp_config_create();
        htp_config_set_server_personality(cfg, HTP_SERVER_APACHE_2);
    }

    virtual void TearDown() {
        htp_config_destroy(cfg);
    }

    htp_cfg_t *cfg;
};

TEST_F(Multipart, Test1) {
    char boundary[] = "---------------------------41184676334";

    htp_mpartp_t *mpartp = htp_mpartp_create(cfg);
    htp_mpartp_init_boundary_ex(mpartp, boundary);

    char *parts[999];

    size_t i = 0;
    parts[i++] = (char *) "-----------------------------41184676334\r\n";
    parts[i++] = (char *) "Content-Disposition: form-data;\n name=\"field1\"\r\n";
    parts[i++] = (char *) "\r\n";
    parts[i++] = (char *) "0123456789\r\n-";
    parts[i++] = (char *) "-------------";
    parts[i++] = (char *) "---------------41184676334\r\n";
    parts[i++] = (char *) "Content-Disposition: form-data;\n name=\"field2\"\r\n";
    parts[i++] = (char *) "\r\n";
    parts[i++] = (char *) "0123456789\r\n-";
    parts[i++] = (char *) "-------------";
    parts[i++] = (char *) "--------------X\r\n";
    parts[i++] = (char *) "-----------------------------41184676334\r\n";
    parts[i++] = (char *) "Content-Disposition: form-data;\n";
    parts[i++] = (char *) " ";
    parts[i++] = (char *) "name=\"field3\"\r\n";
    parts[i++] = (char *) "\r\n";
    parts[i++] = (char *) "9876543210\r\n";
    parts[i++] = (char *) "-----------------------------41184676334\r\n";
    parts[i++] = (char *) "Content-Disposition: form-data; name=\"file1\"; filename=\"New Text Document.txt\"\r\nContent-Type: text/plain\r\n\r\n";
    parts[i++] = (char *) "1FFFFFFFFFFFFFFFFFFFFFFFFFFF\r\n";
    parts[i++] = (char *) "2FFFFFFFFFFFFFFFFFFFFFFFFFFE\r";
    parts[i++] = (char *) "3FFFFFFFFFFFFFFFFFFFFFFFFFFF\r\n4FFFFFFFFFFFFFFFFFFFFFFFFF123456789";
    parts[i++] = (char *) "\r\n";
    parts[i++] = (char *) "-----------------------------41184676334\r\n";
    parts[i++] = (char *) "Content-Disposition: form-data; name=\"file2\"; filename=\"New Text Document.txt\"\r\n";
    parts[i++] = (char *) "Content-Type: text/plain\r\n";
    parts[i++] = (char *) "\r\n";
    parts[i++] = (char *) "FFFFFFFFFFFFFFFFFFFFFFFFFFFZ";
    // The final boundary should not go here
    parts[i++] = NULL;

    i = 0;
    for (;;) {
        if (parts[i] == NULL) break;
        htp_mpartp_parse(mpartp, (const unsigned char *) parts[i], strlen(parts[i]));
        i++;
    }

    // Send the final boundary
    htp_mpartp_parse(mpartp, (const unsigned char *) &"\r\n", 2);
    htp_mpartp_parse(mpartp, (const unsigned char *) boundary, strlen(boundary));
    htp_mpartp_parse(mpartp, (const unsigned char *) &"--", 2);

    htp_mpartp_finalize(mpartp);

    // Examine the result
    htp_multipart_t *body = htp_mpartp_get_multipart(mpartp);
    ASSERT_TRUE(body != NULL);

    for (size_t i = 0, n = htp_list_size(body->parts); i < n; i++) {
        htp_multipart_part_t *part = (htp_multipart_part_t *) htp_list_get(body->parts, i);

        switch (i) {
            case 0:
                ASSERT_TRUE(bstr_cmp_c(part->name, "field1") == 0);
                ASSERT_EQ(part->type, MULTIPART_PART_TEXT);
                ASSERT_TRUE(part->value != NULL);
                ASSERT_TRUE(bstr_cmp_c(part->value, "0123456789") == 0);
                break;
            case 1:
                ASSERT_TRUE(bstr_cmp_c(part->name, "field2") == 0);
                ASSERT_EQ(part->type, MULTIPART_PART_TEXT);
                ASSERT_TRUE(bstr_cmp_c(part->value, "0123456789\r\n----------------------------X") == 0);
                break;
            case 2:
                ASSERT_TRUE(bstr_cmp_c(part->name, "field3") == 0);
                ASSERT_EQ(part->type, MULTIPART_PART_TEXT);
                ASSERT_TRUE(part->value != NULL);
                ASSERT_TRUE(bstr_cmp_c(part->value, "9876543210") == 0);
                break;
            case 3:
                ASSERT_TRUE(bstr_cmp_c(part->name, "file1") == 0);
                ASSERT_EQ(part->type, MULTIPART_PART_FILE);
                break;
            case 4:
                ASSERT_TRUE(bstr_cmp_c(part->name, "file2") == 0);
                ASSERT_EQ(part->type, MULTIPART_PART_FILE);
                break;
            default:
                FAIL() << "More parts than expected";
                break;
        }
    }

    htp_mpartp_destroy(&mpartp);
}

TEST_F(Multipart, Test2) {
    htp_mpartp_t *mpartp = htp_mpartp_create(cfg);
    htp_mpartp_init_boundary_ex(mpartp, "BBB");

    const char *i1 = "x0000x\n--BBB\n\nx1111x\n--\nx2222x\n--";
    const char *i2 = "BBB\n\nx3333x\n--B";
    const char *i3 = "B\n\nx4444x\n--BB\r";
    const char *i4 = "\n--B";
    const char *i5 = "B";
    const char *i6 = "B\n\nx5555x\r";
    const char *i7 = "\n--x6666x\r";
    const char *i8 = "-";
    const char *i9 = "-";

    htp_mpartp_parse(mpartp, (const unsigned char *) i1, strlen(i1));
    htp_mpartp_parse(mpartp, (const unsigned char *) i2, strlen(i2));
    htp_mpartp_parse(mpartp, (const unsigned char *) i3, strlen(i3));
    htp_mpartp_parse(mpartp, (const unsigned char *) i4, strlen(i4));
    htp_mpartp_parse(mpartp, (const unsigned char *) i5, strlen(i5));
    htp_mpartp_parse(mpartp, (const unsigned char *) i6, strlen(i6));
    htp_mpartp_parse(mpartp, (const unsigned char *) i7, strlen(i7));
    htp_mpartp_parse(mpartp, (const unsigned char *) i8, strlen(i8));
    htp_mpartp_parse(mpartp, (const unsigned char *) i9, strlen(i9));
    htp_mpartp_finalize(mpartp);

    htp_multipart_t *body = htp_mpartp_get_multipart(mpartp);
    ASSERT_TRUE(body != NULL);

    for (size_t i = 0, n = htp_list_size(body->parts); i < n; i++) {
        htp_multipart_part_t *part = (htp_multipart_part_t *) htp_list_get(body->parts, i);

        switch (i) {
            case 0:
                ASSERT_EQ(part->type, 3);
                ASSERT_TRUE(part->value == NULL); // Not keeping prologue data at the moment
                break;            
            case 1:
                ASSERT_EQ(part->type, 1);
                ASSERT_TRUE(part->value != NULL);
                ASSERT_TRUE(bstr_cmp_c(part->value, "x1111x\n--\nx2222x") == 0);
                break;
            case 2:
                ASSERT_EQ(part->type, 1);
                ASSERT_TRUE(part->value != NULL);                
                ASSERT_TRUE(bstr_cmp_c(part->value, "x3333x\n--BB\n\nx4444x\n--BB") == 0);
                break;
            case 3:
                ASSERT_EQ(part->type, 1);
                ASSERT_TRUE(part->value != NULL);                
                ASSERT_TRUE(bstr_cmp_c(part->value, "x5555x\r\n--x6666x\r--") == 0);
                break;
            default:
                FAIL();

        }
    }

    htp_mpartp_destroy(&mpartp);
}

static void Multipart_Helper(htp_cfg_t *cfg, char *parts[]) {
    char boundary[] = "0123456789";

    htp_mpartp_t *mpartp = htp_mpartp_create(cfg);
    htp_mpartp_init_boundary_ex(mpartp, boundary);

    size_t i = 0;
    for (;;) {
        if (parts[i] == NULL) break;
        htp_mpartp_parse(mpartp, (const unsigned char *) parts[i], strlen(parts[i]));
        i++;
    }

    htp_mpartp_finalize(mpartp);

    // Examine the result
    htp_multipart_t *body = htp_mpartp_get_multipart(mpartp);
    ASSERT_TRUE(body != NULL);

    ASSERT_TRUE(htp_list_size(body->parts) == 2);

    for (size_t i = 0, n = htp_list_size(body->parts); i < n; i++) {
        htp_multipart_part_t *part = (htp_multipart_part_t *) htp_list_get(body->parts, i);

        switch (i) {
            case 0:
                ASSERT_EQ(part->type, MULTIPART_PART_TEXT);
                ASSERT_TRUE(part->name != NULL);
                ASSERT_TRUE(bstr_cmp_c(part->name, "field1") == 0);                
                ASSERT_TRUE(part->value != NULL);
                ASSERT_TRUE(bstr_cmp_c(part->value, "ABCDEF") == 0);
                break;
            case 1:
                ASSERT_EQ(part->type, MULTIPART_PART_TEXT);
                ASSERT_TRUE(part->name != NULL);
                ASSERT_TRUE(bstr_cmp_c(part->name, "field2") == 0);
                ASSERT_TRUE(part->value != NULL);
                ASSERT_TRUE(bstr_cmp_c(part->value, "GHIJKL") == 0);
                break;
        }
    }

    htp_mpartp_destroy(&mpartp);
}

TEST_F(Multipart, Test3) {    
    char *parts[] = {
        "--0123456789\r\n"
        "Content-Disposition: form-data;\n name=\"field1\"\r\n"
        "\r\n"
        "ABCDEF"
        "\r\n--0123456789\r\n"
        "Content-Disposition: form-data;\n name=\"field2\"\r\n"
        "\r\n"
        "GHIJKL"
        "\r\n--0123456789--",
        NULL
    };

    Multipart_Helper(cfg, parts);
}

TEST_F(Multipart, Test4) {
    char *parts[] = {
        "\r\n--0123456789\r\n"
        "Content-Disposition: form-data;\n name=\"field1\"\r\n"
        "\r\n"
        "ABCDEF"
        "\r\n--0123456789\r\n"
        "Content-Disposition: form-data;\n name=\"field2\"\r\n"
        "\r\n"
        "GHIJKL"
        "\r\n--0123456789--",
        NULL
    };

    Multipart_Helper(cfg, parts);
}

TEST_F(Multipart, Test5) {    
    char *parts[] = {
        "\n--0123456789\r\n"
        "Content-Disposition: form-data;\n name=\"field1\"\r\n"
        "\r\n"
        "ABCDEF"
        "\r\n--0123456789\r\n"
        "Content-Disposition: form-data;\n name=\"field2\"\r\n"
        "\r\n"
        "GHIJKL"
        "\r\n--0123456789--",
        NULL
    };

    Multipart_Helper(cfg, parts);
}

TEST_F(Multipart, Test6) {
    char *parts[] = {
        "--0123456789\n"
        "Content-Disposition: form-data;\n name=\"field1\"\n"
        "\n"
        "ABCDEF"
        "\n--0123456789\n"
        "Content-Disposition: form-data;\n name=\"field2\"\n"
        "\n"
        "GHIJKL"
        "\n--0123456789--",
        NULL
    };

    Multipart_Helper(cfg, parts);
}
