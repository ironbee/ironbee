/***************************************************************************
 * Copyright (c) 2012 Qualys, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * * Neither the name of the Qualys, Inc. nor the names of its
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

#include <stdlib.h>
#include <string.h>

#include <gtest/gtest.h>
#include <sqltfn.h>

class SqlNormalizePg : public testing::Test {

protected:

    virtual void SetUp() {
        output = NULL;
    }

    virtual void TearDown() {
        if (output != NULL) {
            free(output);
            output = NULL;
        }
    }
    
    char *output;
    
    size_t output_len;
};

TEST_F(SqlNormalizePg, NoTransformation) {
    const char *input = "SELECT 1";
    const char *expected = "SELECT 1";
    
    sqltfn_normalize_pg(input, strlen(input) + 1, &output, &output_len);
    
    ASSERT_TRUE(output != NULL);
    
    ASSERT_EQ(strlen(expected), output_len - 1);
    
    ASSERT_EQ(memcmp(expected, output, output_len), 0);
}

TEST_F(SqlNormalizePg, WhitespaceSpaces) {
    const char *input = "SELECT  1";
    const char *expected = "SELECT 1";
    
    sqltfn_normalize_pg(input, strlen(input) + 1, &output, &output_len);
    
    ASSERT_TRUE(output != NULL);
    
    ASSERT_EQ(strlen(expected), output_len - 1);
    
    ASSERT_EQ(memcmp(expected, output, output_len), 0);
}

TEST_F(SqlNormalizePg, WhitespaceTab) {
    const char *input = "SELECT\x09 1";
    const char *expected = "SELECT 1";
    
    sqltfn_normalize_pg(input, strlen(input) + 1, &output, &output_len);
    
    ASSERT_TRUE(output != NULL);
    
    ASSERT_EQ(strlen(expected), output_len - 1);
    
    ASSERT_EQ(memcmp(expected, output, output_len), 0);
}

TEST_F(SqlNormalizePg, WhitespaceNewline) {
    const char *input = "SELECT\x0a 1";
    const char *expected = "SELECT 1";
    
    sqltfn_normalize_pg(input, strlen(input) + 1, &output, &output_len);
    
    ASSERT_TRUE(output != NULL);
    
    ASSERT_EQ(strlen(expected), output_len - 1);
    
    ASSERT_EQ(memcmp(expected, output, output_len), 0);
}

TEST_F(SqlNormalizePg, WhitespaceWhatIsC) {
    const char *input = "SELECT\x0c 1";
    const char *expected = "SELECT 1";
    
    sqltfn_normalize_pg(input, strlen(input) + 1, &output, &output_len);
    
    ASSERT_TRUE(output != NULL);
    
    ASSERT_EQ(strlen(expected), output_len - 1);
    
    ASSERT_EQ(memcmp(expected, output, output_len), 0);
}

TEST_F(SqlNormalizePg, WhitespaceCarriageReturn) {
    const char *input = "SELECT\x0d 1";
    const char *expected = "SELECT 1";
    
    sqltfn_normalize_pg(input, strlen(input) + 1, &output, &output_len);
    
    ASSERT_TRUE(output != NULL);
    
    ASSERT_EQ(strlen(expected), output_len - 1);
    
    ASSERT_EQ(memcmp(expected, output, output_len), 0);
}

TEST_F(SqlNormalizePg, WhitespaceMix) {
    const char *input = "SELECT \x0c\x09\x09 \x0d\x0a 1";
    const char *expected = "SELECT 1";
    
    sqltfn_normalize_pg(input, strlen(input) + 1, &output, &output_len);
    
    ASSERT_TRUE(output != NULL);
    
    ASSERT_EQ(strlen(expected), output_len - 1);
    
    ASSERT_EQ(memcmp(expected, output, output_len), 0);
}

TEST_F(SqlNormalizePg, DashComment) {
    const char *input = "SELECT --\x0a 1";
    const char *expected = "SELECT 1";
    
    sqltfn_normalize_pg(input, strlen(input) + 1, &output, &output_len);
    
    ASSERT_TRUE(output != NULL);
    
    ASSERT_EQ(strlen(expected), output_len - 1);
    
    ASSERT_EQ(memcmp(expected, output, output_len), 0);
}

TEST_F(SqlNormalizePg, DashCommentNoSpaces) {
    const char *input = "SELECT--\x0a\x31";
    const char *expected = "SELECT 1";
    
    sqltfn_normalize_pg(input, strlen(input) + 1, &output, &output_len);
    
    ASSERT_TRUE(output != NULL);
    
    ASSERT_EQ(strlen(expected), output_len - 1);
    
    ASSERT_EQ(memcmp(expected, output, output_len), 0);
}

TEST_F(SqlNormalizePg, MultilineComment) {
    const char *input = "SELECT /* */ 1";
    const char *expected = "SELECT 1";
    
    sqltfn_normalize_pg(input, strlen(input) + 1, &output, &output_len);
    
    ASSERT_TRUE(output != NULL);
    
    ASSERT_EQ(strlen(expected), output_len - 1);
    
    ASSERT_EQ(memcmp(expected, output, output_len), 0);
}

TEST_F(SqlNormalizePg, MultilineRecursiveComment) {
    const char *input = "SELECT /* /* */ */ 1";
    const char *expected = "SELECT 1";
    
    sqltfn_normalize_pg(input, strlen(input) + 1, &output, &output_len);
    
    ASSERT_TRUE(output != NULL);
    
    ASSERT_EQ(strlen(expected), output_len - 1);
    
    ASSERT_EQ(memcmp(expected, output, output_len), 0);
}

TEST_F(SqlNormalizePg, MultilineRecursiveCommentNoSpaces) {
    const char *input = "SELECT/* /* */ */1";
    const char *expected = "SELECT 1";
    
    sqltfn_normalize_pg(input, strlen(input) + 1, &output, &output_len);
    
    ASSERT_TRUE(output != NULL);
    
    ASSERT_EQ(strlen(expected), output_len - 1);
    
    ASSERT_EQ(memcmp(expected, output, output_len), 0);
}

TEST_F(SqlNormalizePg, StringWithDoubledSingleQuote) {
    const char *input = "SELECT '--''--', 2";
    const char *expected = "SELECT '--''--', 2";
    
    sqltfn_normalize_pg(input, strlen(input) + 1, &output, &output_len);
    
    ASSERT_TRUE(output != NULL);
       
    ASSERT_EQ(strlen(expected), output_len - 1);
    
    ASSERT_EQ(memcmp(expected, output, output_len), 0);
}

TEST_F(SqlNormalizePg, StringWithEscapedSingleQuote) {
    const char *input = "SELECT '--\\'--', 2";
    const char *expected = "SELECT '--\\'--', 2";
    
    sqltfn_normalize_pg(input, strlen(input) + 1, &output, &output_len);
    
    ASSERT_TRUE(output != NULL);
       
    ASSERT_EQ(strlen(expected), output_len - 1);
    
    ASSERT_EQ(memcmp(expected, output, output_len), 0);
}

TEST_F(SqlNormalizePg, StringWithComment) {
    const char *input = "SELECT '--', 2";
    const char *expected = "SELECT '--', 2";
    
    sqltfn_normalize_pg(input, strlen(input) + 1, &output, &output_len);
    
    ASSERT_TRUE(output != NULL);
       
    ASSERT_EQ(strlen(expected), output_len - 1);
    
    ASSERT_EQ(memcmp(expected, output, output_len), 0);
}

TEST_F(SqlNormalizePg, EStringLowercaseWithComment) {
    const char *input = "SELECT e'--', 2";
    const char *expected = "SELECT e'--', 2";
    
    sqltfn_normalize_pg(input, strlen(input) + 1, &output, &output_len);
    
    ASSERT_TRUE(output != NULL);
       
    ASSERT_EQ(strlen(expected), output_len - 1);
    
    ASSERT_EQ(memcmp(expected, output, output_len), 0);
}

TEST_F(SqlNormalizePg, EStringUppercaseWithComment) {
    const char *input = "SELECT E'--', 2";
    const char *expected = "SELECT E'--', 2";
    
    sqltfn_normalize_pg(input, strlen(input) + 1, &output, &output_len);
    
    ASSERT_TRUE(output != NULL);
       
    ASSERT_EQ(strlen(expected), output_len - 1);
    
    ASSERT_EQ(memcmp(expected, output, output_len), 0);
}

TEST_F(SqlNormalizePg, UStringLowercaseWithComment) {
    const char *input = "SELECT u'--', 2";
    const char *expected = "SELECT u'--', 2";
    
    sqltfn_normalize_pg(input, strlen(input) + 1, &output, &output_len);
    
    ASSERT_TRUE(output != NULL);
       
    ASSERT_EQ(strlen(expected), output_len - 1);
    
    ASSERT_EQ(memcmp(expected, output, output_len), 0);
}


TEST_F(SqlNormalizePg, UStringUppercaseWithComment) {
    const char *input = "SELECT U'--', 2";
    const char *expected = "SELECT U'--', 2";
    
    sqltfn_normalize_pg(input, strlen(input) + 1, &output, &output_len);
    
    ASSERT_TRUE(output != NULL);
       
    ASSERT_EQ(strlen(expected), output_len - 1);
    
    ASSERT_EQ(memcmp(expected, output, output_len), 0);
}

TEST_F(SqlNormalizePg, DollarStringNoTagContainsComment) {
    const char *input = "SELECT $$--$$, 2";
    const char *expected = "SELECT $$--$$, 2";
    
    sqltfn_normalize_pg(input, strlen(input) + 1, &output, &output_len);
    
    ASSERT_TRUE(output != NULL);
       
    ASSERT_EQ(strlen(expected), output_len - 1);
    
    ASSERT_EQ(memcmp(expected, output, output_len), 0);
}

TEST_F(SqlNormalizePg, DollarStringTagContainsComment) {
    const char *input = "SELECT $_tag2$--$_tag2$, 2";
    const char *expected = "SELECT $_tag2$--$_tag2$, 2";
    
    sqltfn_normalize_pg(input, strlen(input) + 1, &output, &output_len);
    
    ASSERT_TRUE(output != NULL);
       
    ASSERT_EQ(strlen(expected), output_len - 1);
    
    ASSERT_EQ(memcmp(expected, output, output_len), 0);
}

TEST_F(SqlNormalizePg, Crash_2012_11_01) {
    const char *input = "$";
    const char *expected = "$";

    sqltfn_normalize_pg(input, strlen(input) + 1, &output, &output_len);

    ASSERT_TRUE(output != NULL);

    ASSERT_EQ(strlen(expected), output_len - 1);

    ASSERT_EQ(memcmp(expected, output, output_len), 0);
}

TEST_F(SqlNormalizePg, Bug_2012_11_01) {
    const char *input = "cast(cast((SELECT $$1001$$)as text)as int)/*union select */";
    const char *expected = "cast(cast((SELECT $$1001$$)as text)as int) ";

    sqltfn_normalize_pg(input, strlen(input) + 1, &output, &output_len);

    ASSERT_TRUE(output != NULL);   

    ASSERT_EQ(strlen(expected), output_len - 1);

    ASSERT_EQ(memcmp(expected, output, output_len), 0);
}

TEST_F(SqlNormalizePg, Bypass_2012_11_01) {
    const char *input = "strpos(cast((SELECT $$1$$ a$a$)as text),'$a$/*') union select null,ccnumber,null,null from credit_cards";
    const char *expected = "strpos(cast((SELECT $$1$$ a$a$)as text),'$a$/*') union select null,ccnumber,null,null from credit_cards";

    sqltfn_normalize_pg(input, strlen(input) + 1, &output, &output_len);

    ASSERT_TRUE(output != NULL);   

    ASSERT_EQ(strlen(expected), output_len - 1);

    ASSERT_EQ(memcmp(expected, output, output_len), 0);
}

TEST_F(SqlNormalizePg, DoNotTreatParamsAsDollarStrings) {
    const char *input = "SELECT $1 /*$*/";
    const char *expected = "SELECT $1 ";

    sqltfn_normalize_pg(input, strlen(input) + 1, &output, &output_len);

    ASSERT_TRUE(output != NULL);

    ASSERT_EQ(strlen(expected), output_len - 1);

    ASSERT_EQ(memcmp(expected, output, output_len), 0);
}

TEST_F(SqlNormalizePg, Bypass_2012_11_01_2) {
    const char *input = "(select a from \"/*\") union select null,ccnumber,null,null from credit_cards";
    const char *expected = "(select a from \"/*\") union select null,ccnumber,null,null from credit_cards";

    sqltfn_normalize_pg(input, strlen(input) + 1, &output, &output_len);

    ASSERT_TRUE(output != NULL);   

    ASSERT_EQ(strlen(expected), output_len - 1);

    ASSERT_EQ(memcmp(expected, output, output_len), 0);
}

TEST_F(SqlNormalizePg, Bypass_2012_11_02) {
    const char *input = "1001 union--\x0dselect 1,ccnumber,null,null FROM credit_cards";
    const char *expected = "1001 union select 1,ccnumber,null,null FROM credit_cards";

    sqltfn_normalize_pg(input, strlen(input) + 1, &output, &output_len);

    ASSERT_TRUE(output != NULL);

    ASSERT_EQ(strlen(expected), output_len - 1);

    ASSERT_EQ(memcmp(expected, output, output_len), 0);
}

TEST_F(SqlNormalizePg, Bypass_2012_11_02_2) {
    const char *input = "strpos('1001\\\\',$$/*$$)union/**/select null,ccnumber,null,null from credit_cards";
    const char *expected = "strpos('1001\\\\',$$/*$$)union select null,ccnumber,null,null from credit_cards";

    sqltfn_normalize_pg(input, strlen(input) + 1, &output, &output_len);

    ASSERT_TRUE(output != NULL);

    ASSERT_EQ(strlen(expected), output_len - 1);

    ASSERT_EQ(memcmp(expected, output, output_len), 0);
}

