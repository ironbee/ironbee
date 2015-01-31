/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
/* vi: set expandtab shiftwidth=4 tabstop=4: */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "modp_bjavascript.h"
#include "minunit.h"

/**
 * Tests input where no escaping happens
 */
static char* testNoEscape()
{
    char buf[100];
    const char* s1 = "this is a string";
    const int len1 = strlen(s1);
    int d = modp_bjavascript_encode(buf, s1, len1);

    mu_assert_int_equals(len1, d);
    mu_assert_str_equals(buf, s1);

    int sz = modp_bjavascript_encode_strlen(s1, len1);
    mu_assert_int_equals(sz, len1);

    return 0;
}

static char* testSimpleEscape()
{
    char buf[100];
    const char* s1 = "\\this\nis a string\n";
    const char* s2 = "\\\\this\\nis a string\\n";
    const int len1 = strlen(s1);
    const int len2 = strlen(s2);
    int d = modp_bjavascript_encode(buf, s1, len1);

    mu_assert_int_equals(len2, d);
    mu_assert_str_equals(buf, s2);

    int sz = modp_bjavascript_encode_strlen(s1, len1);
    mu_assert_int_equals(sz, len2);

    /*
     * Test the Raw escape '\' --> '\\'
     */
    char ibuf[] = {'\\', '\0'};
    memset(buf, 0, sizeof(buf));   
    d = modp_bjavascript_encode(buf,ibuf, 1);
    mu_assert_int_equals(buf[0], '\\');
    mu_assert_int_equals(buf[1], '\\');
    mu_assert_int_equals(buf[2], 0);

    return 0;
}

static char* testSQuoteEscape()
{
    char buf[100];
    const char* s1 = "this is a 'string'\n";
    const char* s2 = "this is a \\'string\\'\\n";
    const int len1 = strlen(s1);
    const int len2 = strlen(s2);
    int d = modp_bjavascript_encode(buf, s1, len1);

    mu_assert_int_equals(len2, d);
    mu_assert_str_equals(buf, s2);

    int sz = modp_bjavascript_encode_strlen(s1, len1);
    mu_assert_int_equals(sz, len2);

    char ibuf[] = {'\'', '\0'};
    memset(buf, 0, sizeof(buf));
    d = modp_bjavascript_encode(buf, ibuf, 1);
    mu_assert_int_equals(buf[0], '\\');
    mu_assert_int_equals(buf[1], '\'');
    mu_assert_int_equals(buf[2], '\0');

    return 0;
}

static char* testDQuoteEscape()
{
    char buf[100];
    const char* s1 = "this is a \"string\"\n";
    const char* s2 = "this is a \\\"string\\\"\\n";
    const int len1 = strlen(s1);
    const int len2 = strlen(s2);
    int d = modp_bjavascript_encode(buf, s1, len1);

    mu_assert_int_equals(len2, d);
    mu_assert_str_equals(buf, s2);

    int sz = modp_bjavascript_encode_strlen(s1, len1);
    mu_assert_int_equals(sz, len2);
    char ibuf[] = {'\"', '\0'};
    memset(buf, 0, sizeof(buf));
    d = modp_bjavascript_encode(buf, ibuf, 1);
    mu_assert_int_equals(buf[0], '\\');
    mu_assert_int_equals(buf[1], '\"');
    mu_assert_int_equals(buf[2], '\0');
    return 0;
}

static char* testBinaryEscape()
{
    char buf[100];
    const char s1[] = {1,2,3,4,0};
    const char* s2 = "\\x01\\x02\\x03\\x04";
    const int len1 = strlen(s1);
    const int len2 = strlen(s2);
    int d = modp_bjavascript_encode(buf, s1, len1);

    mu_assert_int_equals(len2, d);
    mu_assert_str_equals(buf, s2);

    int sz = modp_bjavascript_encode_strlen(s1, len1);
    mu_assert_int_equals(sz, len2);
    return 0;
}

static char* all_tests()
{
    mu_run_test(testNoEscape);
    mu_run_test(testSimpleEscape);
    mu_run_test(testBinaryEscape);
    mu_run_test(testSQuoteEscape);
    mu_run_test(testDQuoteEscape);
    return 0;
}

UNITTESTS


