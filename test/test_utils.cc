/***************************************************************************
 * Copyright 2011 Qualys, Inc.
 *
 * Licensed to You under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ***************************************************************************/

/**
 * @file
 * @brief Tests for various utility functions.
 *
 * @author Craig Forbes <cforbes@qualys.com>
 */

#include<iostream>

#include<gtest/gtest.h>

#include<htp/htp.h>
#include<htp/dslib.h>
#include<htp/utf8_decoder.h>
#include<htp/htp_base64.h>

TEST(Utf8, SingleByte) {
    uint32_t state = HTP_UTF8_ACCEPT;
    uint32_t codep;
    uint32_t result;

    result = htp_utf8_decode(&state, &codep, 0x00);
    EXPECT_EQ(0, result);
    EXPECT_EQ(HTP_UTF8_ACCEPT, state);
    EXPECT_EQ(0, codep);
}

TEST(Base64, Single) {
    EXPECT_EQ(62, htp_base64_decode_single('+'));
    EXPECT_EQ(63, htp_base64_decode_single('/'));
    EXPECT_EQ(-1, htp_base64_decode_single(','));
    EXPECT_EQ(-1, htp_base64_decode_single(0));
    EXPECT_EQ(-1, htp_base64_decode_single('~'));
    EXPECT_EQ(26, htp_base64_decode_single('a'));
    EXPECT_EQ(0, htp_base64_decode_single('A'));
}

TEST(Base64, Decode) {
    char *input ="dGhpcyBpcyBhIHRlc3QuLg==";
    bstr *out = htp_base64_decode_mem(input, strlen(input));
    EXPECT_EQ(0, bstr_cmp_c(out, "this is a test.."));
    bstr_free(&out);
}

TEST(UtilTest, Separator) {
    EXPECT_EQ(0, htp_is_separator('a'));
    EXPECT_EQ(0, htp_is_separator('^'));
    EXPECT_EQ(0, htp_is_separator('-'));
    EXPECT_EQ(0, htp_is_separator('_'));
    EXPECT_EQ(0, htp_is_separator('&'));
    EXPECT_EQ(1, htp_is_separator('('));
    EXPECT_EQ(1, htp_is_separator('\\'));
    EXPECT_EQ(1, htp_is_separator('/'));
    EXPECT_EQ(1, htp_is_separator('='));
    EXPECT_EQ(1, htp_is_separator('\t'));
}

TEST(UtilTest, Text) {
    EXPECT_EQ(1, htp_is_text('\t'));
    EXPECT_EQ(1, htp_is_text('a'));
    EXPECT_EQ(1, htp_is_text('~'));
    EXPECT_EQ(1, htp_is_text(' '));
    EXPECT_EQ(0, htp_is_text('\n'));
    EXPECT_EQ(0, htp_is_text('\r'));
    EXPECT_EQ(0, htp_is_text('\r'));
    EXPECT_EQ(0, htp_is_text(31));
}

TEST(UtilTest, Token) {
    EXPECT_EQ(1, htp_is_token('a'));
    EXPECT_EQ(1, htp_is_token('&'));
    EXPECT_EQ(1, htp_is_token('+'));
    EXPECT_EQ(0, htp_is_token('\t'));
    EXPECT_EQ(0, htp_is_token('\n'));
}

TEST(UtilTest, Chomp) {
    char data[100];
    size_t len;
    int result;

    strcpy(data, "test\r\n");
    len = strlen(data);
    result = htp_chomp((unsigned char*)data, &len);
    EXPECT_EQ(2, result);
    EXPECT_EQ(4, len);

    strcpy(data, "foo\n");
    len = strlen(data);
    result = htp_chomp((unsigned char*)data, &len);
    EXPECT_EQ(1, result);
    EXPECT_EQ(3, len);

    strcpy(data, "arfarf");
    len = strlen(data);
    result = htp_chomp((unsigned char*)data, &len);
    EXPECT_EQ(0, result);
    EXPECT_EQ(6, len);
}

TEST(UtilTest, Space) {
    EXPECT_EQ(0, htp_is_space('a'));
    EXPECT_EQ(1, htp_is_space(' '));
    EXPECT_EQ(1, htp_is_space('\f'));
    EXPECT_EQ(1, htp_is_space('\n'));
    EXPECT_EQ(1, htp_is_space('\r'));
    EXPECT_EQ(1, htp_is_space('\t'));
    EXPECT_EQ(1, htp_is_space('\v'));
}

TEST(UtilTest, Method) {
    bstr *method = bstr_dup_c("GET");

    EXPECT_EQ(M_GET, htp_convert_method_to_number(method));

    bstr_free(&method);
}

TEST(UtilTest, IsLineEmpty) {
    char data[100];
    strcpy(data, "arfarf");
    EXPECT_EQ(0, htp_is_line_empty((unsigned char*) data, 6));

    strcpy(data, "\r\n");
    EXPECT_EQ(1, htp_is_line_empty((unsigned char*) data, 2));
    strcpy(data, "\r");
    EXPECT_EQ(1, htp_is_line_empty((unsigned char*) data, 1));
    EXPECT_EQ(0, htp_is_line_empty((unsigned char*) data, 0));

}

TEST(UtilTest, IsLineWhitespace) {
    char data[100];
    strcpy(data, "arfarf");
    EXPECT_EQ(0, htp_is_line_whitespace((unsigned char*) data, 6));

    strcpy(data, "\r\n");
    EXPECT_EQ(1, htp_is_line_whitespace((unsigned char*) data, 2));
    strcpy(data, "\r");
    EXPECT_EQ(1, htp_is_line_whitespace((unsigned char*) data, 1));
    EXPECT_EQ(1, htp_is_line_whitespace((unsigned char*) data, 0));
}

TEST(UtilTest, ParsePositiveIntegerWhitespace) {
    EXPECT_EQ(123, htp_parse_positive_integer_whitespace(
        (unsigned char*)"123   ", 6, 10));
    EXPECT_EQ(123, htp_parse_positive_integer_whitespace(
        (unsigned char*)"   123", 6, 10));
    EXPECT_EQ(123, htp_parse_positive_integer_whitespace(
        (unsigned char*)"   123   ", 9, 10));
    EXPECT_EQ(-1, htp_parse_positive_integer_whitespace(
        (unsigned char*)"a123", 4, 10));
    EXPECT_EQ(-1001, htp_parse_positive_integer_whitespace(
        (unsigned char*)"   \t", 4, 10));
    EXPECT_EQ(-1002, htp_parse_positive_integer_whitespace(
        (unsigned char*)"123b ", 5, 10));

    EXPECT_EQ(-1, htp_parse_positive_integer_whitespace(
        (unsigned char*)"   a123   ", 9, 10));
    EXPECT_EQ(-1002, htp_parse_positive_integer_whitespace(
        (unsigned char*)"   123b   ", 9, 10));

    EXPECT_EQ(0x123, htp_parse_positive_integer_whitespace(
        (unsigned char*)"   123   ", 9, 16));
}

TEST(UtilTest, ParseContentLength) {
    bstr *str = bstr_dup_c("134");

    EXPECT_EQ(134, htp_parse_content_length(str));

    bstr_free(&str);
}

TEST(UtilTest, ParseChunkedLength) {
    EXPECT_EQ(0x12a5, htp_parse_chunked_length((unsigned char*)"12a5",4));
}

TEST(UtilTest, IsLineFolded) {
    EXPECT_EQ(-1, htp_connp_is_line_folded((unsigned char*)"", 0));
    EXPECT_EQ(1, htp_connp_is_line_folded((unsigned char*)"\tline", 5));
    EXPECT_EQ(1, htp_connp_is_line_folded((unsigned char*)" line", 5));
    EXPECT_EQ(0, htp_connp_is_line_folded((unsigned char*)"line ", 5));
}

static void free_htp_uri_t(htp_uri_t **urip) {
    htp_uri_t *uri = *urip;

    if (uri == NULL) {
        return;
    }
    bstr_free(&(uri->scheme));
    bstr_free(&(uri->username));
    bstr_free(&(uri->password));
    bstr_free(&(uri->hostname));
    bstr_free(&(uri->port));
    bstr_free(&(uri->path));
    bstr_free(&(uri->query));
    bstr_free(&(uri->fragment));

    free(uri);
    *urip = NULL;
}
