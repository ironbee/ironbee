/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
/* vi: set expandtab shiftwidth=4 tabstop=4: */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "modp_burl.h"
#include "minunit.h"

/**
 * Test empty input to encode and decode
 */
static char* testUrlEmpty()
{
    int d;
    char buf[1000];
    buf[0] = 1;
    d = modp_burl_encode(buf, "", 0);
    mu_assert_int_equals(d, 0);
    mu_assert(buf[0] == 0);

    buf[0] = 1;
    d = modp_burl_decode(buf, "", 0);
    mu_assert_int_equals(d, 0);
    mu_assert(buf[0] == 0);

    return 0;
}

/**
 * test space <--> plus conversion
 */
static char* testUrlSpaces()
{
    size_t d = 0;
    char buf[1000];
    const char* input = "   ";
    const char* output = "+++";

    d = modp_burl_encode(buf, input, strlen(input));
    mu_assert_int_equals(d, strlen(output));
    mu_assert_str_equals(buf, output);

    d = modp_burl_decode(buf, output, strlen(output));
    mu_assert_int_equals(d, strlen(input));
    mu_assert_str_equals(buf, input);

    return 0;
}

/**
 * Test charactes that should be unchanged
 */
static char* testUrlUntouched()
{
    const char* lower = "abcdefghijklmnopqrstuvwxyz";
    const char* upper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const char* digits = "0123456789";
    const char* special = ".-_";
    char buf[1000];
    size_t d = 0;

    memset(buf, 0, sizeof(buf));
    d = modp_burl_encode(buf, lower, strlen(lower));
    mu_assert_int_equals(d, strlen(lower));
    mu_assert_str_equals(buf, lower);
    memset(buf, 0, sizeof(buf));
    d = modp_burl_decode(buf, lower, strlen(lower));
    mu_assert_int_equals(d, strlen(lower));
    mu_assert_str_equals(buf, lower);

    memset(buf, 0, sizeof(buf));
    d = modp_burl_encode(buf, upper, strlen(upper));
    mu_assert_int_equals(d, strlen(upper));
    mu_assert_str_equals(buf, upper);
    memset(buf, 0, sizeof(buf));
    d = modp_burl_decode(buf, upper, strlen(upper));
    mu_assert_int_equals(d, strlen(upper));
    mu_assert_str_equals(buf, upper);

    memset(buf, 0, sizeof(buf));
    d = modp_burl_encode(buf, digits, strlen(digits));
    mu_assert_int_equals(d, strlen(digits));
    mu_assert_str_equals(buf, digits);
    memset(buf, 0, sizeof(buf));
    d = modp_burl_decode(buf, digits, strlen(digits));
    mu_assert_int_equals(d, strlen(digits));
    mu_assert_str_equals(buf, digits);

    memset(buf, 0, sizeof(buf));
    d = modp_burl_encode(buf, special, strlen(special));
    mu_assert_int_equals(d, strlen(special));
    mu_assert_str_equals(buf, special);
    memset(buf, 0, sizeof(buf));
    d = modp_burl_decode(buf, special, strlen(special));
    mu_assert_int_equals(d, strlen(special));
    mu_assert_str_equals(buf, special);

    return 0;
}


/**
 * Test charactes that should be unchanged
 */
static char* testUrlMinUntouched()
{
    const char* lower   = "abcdefghijklmnopqrstuvwxyz";
    const char* upper   = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const char* digits  = "0123456789";
    const char* special = ".-_";
    const char* extra = "~!$()*,;:@/?";
    char buf[1000];
    size_t d = 0;

    memset(buf, 0, sizeof(buf));
    d = modp_burl_min_encode(buf, lower, strlen(lower));
    mu_assert_int_equals(d, strlen(lower));
    mu_assert_str_equals(buf, lower);
    memset(buf, 0, sizeof(buf));
    d = modp_burl_decode(buf, lower, strlen(lower));
    mu_assert_int_equals(d, strlen(lower));
    mu_assert_str_equals(buf, lower);

    memset(buf, 0, sizeof(buf));
    d = modp_burl_min_encode(buf, upper, strlen(upper));
    mu_assert_int_equals(d, strlen(upper));
    mu_assert_str_equals(buf, upper);
    memset(buf, 0, sizeof(buf));
    d = modp_burl_decode(buf, upper, strlen(upper));
    mu_assert_int_equals(d, strlen(upper));
    mu_assert_str_equals(buf, upper);

    memset(buf, 0, sizeof(buf));
    d = modp_burl_min_encode(buf, digits, strlen(digits));
    mu_assert_int_equals(d, strlen(digits));
    mu_assert_str_equals(buf, digits);
    memset(buf, 0, sizeof(buf));
    d = modp_burl_decode(buf, digits, strlen(digits));
    mu_assert_int_equals(d, strlen(digits));
    mu_assert_str_equals(buf, digits);

    memset(buf, 0, sizeof(buf));
    d = modp_burl_min_encode(buf, special, strlen(special));
    mu_assert_int_equals(d, strlen(special));
    mu_assert_str_equals(buf, special);
    memset(buf, 0, sizeof(buf));
    d = modp_burl_decode(buf, special, strlen(special));
    mu_assert_int_equals(d, strlen(special));
    mu_assert_str_equals(buf, special);

    memset(buf, 0, sizeof(buf));
    d = modp_burl_min_encode(buf, extra, strlen(extra));
    mu_assert_int_equals(d, strlen(extra));
    mu_assert_str_equals(buf, extra);
    memset(buf, 0, sizeof(buf));
    d = modp_burl_decode(buf, extra, strlen(extra));
    mu_assert_int_equals(d, strlen(extra));
    mu_assert_str_equals(buf, extra);

    return 0;
}

/** \brief make sure min encoding actually does hex encoding
 *
 */
static char* testUrlMinEncodeHex()
{
    char buf[1000];
    size_t d = 0;

    memset(buf, 0, sizeof(buf));
    const char* str1 = "a b";
    d = modp_burl_min_encode(buf, str1, strlen(str1));
    mu_assert_int_equals(3, d);
    mu_assert_str_equals("a+b", buf);

    memset(buf, 0, sizeof(buf));
    const char* str2 = "ab\n";
    d = modp_burl_min_encode(buf, str2, strlen(str2));
    mu_assert_int_equals(5, d);
    mu_assert_str_equals("ab%0A", buf);

    return 0;
}

static char* testUrlDecodeHexBad()
{

    const char* bad1 = "%0X"; // bad trailing char
    const char* bad2 = "%X0"; // bad leading char
    const char* bad3 = "%XX"; // bad chars
    const char* bad4 = "%2"; // not enough room, good char
    const char* bad5 = "%X"; // not enought room, bad char
    const char* bad6 = "%";  // test oddball
    const char* bad7 = "AA%"; // test end of line
    char bad8[4]; // %XX where X is high bit (test sign char vs. uint8_t*)
    bad8[0] = '%';
    bad8[1] = 0x81;
    bad8[2] = 0x82;
    bad8[3] = 0;

    size_t d = 0;
    char buf[1000];

    memset(buf, 0, sizeof(buf));
    d = modp_burl_decode(buf, bad1, strlen(bad1));
    mu_assert_int_equals(d, strlen(bad1));
    mu_assert_str_equals(buf, bad1);

    memset(buf, 0, sizeof(buf));
    d = modp_burl_decode(buf, bad2, strlen(bad2));
    mu_assert_int_equals(d, strlen(bad2));
    mu_assert_str_equals(bad2, buf);

    memset(buf, 0, sizeof(buf));
    d = modp_burl_decode(buf, bad3, strlen(bad3));
    mu_assert_int_equals(d, strlen(bad3));
    mu_assert_str_equals(buf, bad3);

    memset(buf, 0, sizeof(buf));
    d = modp_burl_decode(buf, bad4, strlen(bad4));
    mu_assert_int_equals(d, strlen(bad4));
    mu_assert_str_equals(buf, bad4);

    memset(buf, 0, sizeof(buf));
    d = modp_burl_decode(buf, bad5, strlen(bad5));
    mu_assert_int_equals(d, strlen(bad5));
    mu_assert_str_equals(buf, bad5);

    memset(buf, 0, sizeof(buf));
    d= modp_burl_decode(buf, bad6, strlen(bad6));
    mu_assert_int_equals(d, strlen(bad6));
    mu_assert_str_equals(buf, bad6);

    memset(buf, 0, sizeof(buf));
    d= modp_burl_decode(buf, bad7, strlen(bad7));
    mu_assert_int_equals(d, strlen(bad7));
    mu_assert_str_equals(buf, bad7);

    memset(buf, 0, sizeof(buf));
    d = modp_burl_decode(buf, bad8, strlen(bad8));
    mu_assert_int_equals(d, strlen(bad8));
    mu_assert_str_equals(buf, bad8);
    return 0;

    return 0;
}

static char* testUrlDecodeHex()
{
    int d; // size of output
    int i, j; // loops
    int k = 0; // position in inputbuf;
    char inputbuf[3*256+1];
    char output[257];
    char msg[1000];

    // make input string contain every possible "%XX"
    static const char* hexdigits1 = "0123456789ABCDEF";
    memset(inputbuf, 0, sizeof(inputbuf));
    memset(output, 1, sizeof(output));
    k = 0;
    for (i = 0; i < 16; ++i) {
        for (j = 0; j < 16; ++j) {
            inputbuf[k++] = '%';
            inputbuf[k++] = hexdigits1[i];
            inputbuf[k++] = hexdigits1[j];
        }
    }

    d = modp_burl_decode(output, inputbuf, sizeof(inputbuf)-1);
    mu_assert_int_equals(d, 256);
    for (i = 0; i < 256; ++i) {
        sprintf(msg, "Loop at %d", i);
        mu_assert_int_equals_msg(msg, i, (unsigned char) output[i]);
    }

    // make input string contain every possible "%XX"
    static const char* hexdigits2 = "0123456789abcdef";
    memset(inputbuf, 0, sizeof(inputbuf));
    memset(output, 1, sizeof(output));

    k = 0;
    for (i = 0; i < 16; ++i) {
        for (j = 0; j < 16; ++j) {
            inputbuf[k++] = '%';
            inputbuf[k++] = hexdigits2[i];
            inputbuf[k++] = hexdigits2[j];
        }
    }

    d = modp_burl_decode(output, inputbuf, sizeof(inputbuf)-1);
    mu_assert_int_equals(256, d);
    for (i = 0; i < 256; ++i) {
        sprintf(msg, "Loop at %d", i);
        mu_assert_int_equals_msg(msg, i, (unsigned char)output[i]);
    }
    return 0;
}

/**
 * test hex encoding.. to be done after hex decoding
 * is tested.
 */
static char* testHexEncoding()
{
    int i = 0;
    int d = 0;
    char msg[1000];
    char input[257];
    memset(input, 0, sizeof(input));
    char output[257*3];
    memset(output, 0, sizeof(output));
    char buf[1000];
    memset(buf, 0, sizeof(buf));
    d = modp_burl_encode(output, input, 256);
    d = modp_burl_decode(buf, output, d);
    mu_assert_int_equals(256, d);
    for (i= 0; i < 256; ++i) {
        sprintf(msg, "Loop at %d failed", i);
        mu_assert_int_equals_msg(msg, input[i], buf[i]);
    }
    return 0;
}

static char* testEncodeStrlen()
{
    char ibuf[100];
    char obuf[100];
    memset(ibuf, 0, sizeof(ibuf));
    memset(obuf, 0, sizeof(obuf));

    // Empty.  should be 0
    ibuf[0] = 0;
    mu_assert_int_equals(strlen(ibuf), (size_t) modp_burl_encode_strlen(ibuf, strlen(ibuf)));

    // Plain, should be same size
    strcpy(ibuf, "abcdefg");
    mu_assert_int_equals(strlen(ibuf), (size_t) modp_burl_encode_strlen(ibuf, strlen(ibuf)));

    // Plain and spaces, should be same size
    strcpy(ibuf, "a b c d e f g");
    mu_assert_int_equals(strlen(ibuf), (size_t) modp_burl_encode_strlen(ibuf, strlen(ibuf)));

    // one bad char, adds two bytes
    strcpy(ibuf, "abcdefg\n");
    mu_assert_int_equals(strlen(ibuf)+2, (size_t) modp_burl_encode_strlen(ibuf, strlen(ibuf)));

    // 2 bad chars, adds 4 bytes
    strcpy(ibuf, "\nabcdefg\n");
    mu_assert_int_equals(strlen(ibuf)+4, (size_t) modp_burl_encode_strlen(ibuf, strlen(ibuf)));
    return 0;
}


/** \brief test "modp_burl_min_encode_strlen"
 *
 */
static char* testEncodeMinStrlen()
{
    char ibuf[100];
    char obuf[100];
    memset(ibuf, 0, sizeof(ibuf));
    memset(obuf, 0, sizeof(obuf));

    // Empty.  should be 0
    ibuf[0] = 0;
    mu_assert_int_equals(strlen(ibuf), (size_t) modp_burl_min_encode_strlen(ibuf, strlen(ibuf)));

    // Plain, should be same size
    strcpy(ibuf, "abcdefg");
    mu_assert_int_equals(strlen(ibuf), (size_t) modp_burl_min_encode_strlen(ibuf, strlen(ibuf)));

    // Plain and spaces, should be same size
    strcpy(ibuf, "a b c d e f g");
    mu_assert_int_equals(strlen(ibuf), (size_t) modp_burl_min_encode_strlen(ibuf, strlen(ibuf)));

    // one bad char, adds two bytes
    strcpy(ibuf, "abcdefg\n");
    mu_assert_int_equals(strlen(ibuf)+2, (size_t) modp_burl_min_encode_strlen(ibuf, strlen(ibuf)));

    // 2 bad chars, adds 4 bytes
    strcpy(ibuf, "\nabcdefg\n");
    mu_assert_int_equals(strlen(ibuf)+4, (size_t) modp_burl_min_encode_strlen(ibuf, strlen(ibuf)));
    return 0;
}

static char* all_tests()
{
    mu_run_test(testUrlUntouched);
    mu_run_test(testUrlEmpty);
    mu_run_test(testUrlSpaces);
    mu_run_test(testUrlDecodeHex);
    mu_run_test(testUrlDecodeHexBad);
    mu_run_test(testHexEncoding);
    mu_run_test(testEncodeStrlen);
    mu_run_test(testUrlMinUntouched);
    mu_run_test(testUrlMinEncodeHex);
    mu_run_test(testEncodeMinStrlen);
    return 0;
}

UNITTESTS

