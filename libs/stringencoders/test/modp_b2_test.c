/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
/* vi: set expandtab shiftwidth=4 tabstop=4: */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "minunit.h"
#include "modp_b2.h"
#include "modp_b16.h"

static char* testEndian()
{
    // this test that "0001" is "0...1"
    char buf[100];
    char result[100];
    char endian[] = {(char)0, (char)0, (char)0, (char)1};
    int d = modp_b2_encode(buf, endian, 4);
    mu_assert_int_equals(32, d);
    mu_assert_str_equals("00000000000000000000000000000001", buf);
    mu_assert_int_equals('0', buf[0]);
    mu_assert_int_equals('1', buf[31]);

    memset(result, 255, sizeof(result));
    d = modp_b2_decode(result, buf, 32);
    mu_assert_int_equals(4, d);
    mu_assert_int_equals(endian[0], result[0]);
    mu_assert_int_equals(endian[1], result[1]);
    mu_assert_int_equals(endian[2], result[2]);
    mu_assert_int_equals(endian[3], result[3]);

    return 0;
}

static char* testEncodeDecode()
{
    // 2 bytes == 4 bytes out
    char ibuf[2];
    char obuf[17];
    char rbuf[17];
    char msg[100]; // for test messages output
    msg[0] = 0; // make msg an empty string
    unsigned int i,j;
    int d;

    for (i = 0; i < 256; ++i) {
        for (j = 0; j < 256; ++j) {
            // comment this out.. it really slows down the test
            sprintf(msg, "(i,j) = (%d,%d):", i,j);
            ibuf[0] = (char)((unsigned char) i);
            ibuf[1] = (char)((unsigned char) j);

            memset(obuf, 0, sizeof(obuf));
            d = modp_b16_encode(obuf, ibuf, 2);
            mu_assert_int_equals_msg(msg, 4, d);
            d = modp_b16_decode(rbuf, obuf, d);
            mu_assert_int_equals_msg(msg, 2, d);
            mu_assert_int_equals_msg(msg, ibuf[0], rbuf[0]);
            mu_assert_int_equals_msg(msg, ibuf[1], rbuf[1]);
        }
    }
    return 0;
}

static char* testOddDecode()
{
    char obuf[100];
    char ibuf[100];

    memset(obuf, 0, sizeof(obuf));
    memset(ibuf, 0, sizeof(ibuf));

    /* Test Odd Number of Input bytes
     * Should be error
     */
    obuf[0] = 1;
    mu_assert_int_equals(-1, modp_b2_decode(obuf, ibuf, 1));
    mu_assert_int_equals(1, obuf[0]);

    obuf[0] = 1;
    mu_assert_int_equals(-1, modp_b2_decode(obuf, ibuf, 3));
    mu_assert_int_equals(1, obuf[0]);

    obuf[0] = 1;
    mu_assert_int_equals(-1, modp_b2_decode(obuf, ibuf, 7));
    mu_assert_int_equals(1, obuf[0]);

    return 0;
}

/** \brief test input that is a multiple of 2 (special case in code)
 */
static char* testDecodeMutlipleOf2()
{
    char obuf[100];
    memset(obuf, 0xff, sizeof(obuf));

    mu_assert_int_equals(1, modp_b16_decode(obuf, "01", 2));
    mu_assert_int_equals(1, obuf[0]);
    return 0;
}

static char* testOddEncode()
{
    char obuf[100];
    char ibuf[100];

    // oddball 1 char.
    ibuf[0] = 1;
    mu_assert_int_equals(2, modp_b16_encode(obuf, ibuf, 1));
    mu_assert_int_equals(obuf[0], '0');
    mu_assert_int_equals(obuf[1], '1');

    // oddball 2 char.
    ibuf[0] = 0;
    ibuf[1] = 1;
    mu_assert_int_equals(4, modp_b16_encode(obuf, ibuf, 2));
    mu_assert_int_equals(obuf[0], '0');
    mu_assert_int_equals(obuf[1], '0');
    mu_assert_int_equals(obuf[2], '0');
    mu_assert_int_equals(obuf[3], '1');

    // oddball 1 char.
    ibuf[0] = 0;
    ibuf[1] = 0;
    ibuf[2] = 1;
    mu_assert_int_equals(6, modp_b16_encode(obuf, ibuf, 3));
    mu_assert_int_equals(obuf[0], '0');
    mu_assert_int_equals(obuf[1], '0');
    mu_assert_int_equals(obuf[2], '0');
    mu_assert_int_equals(obuf[3], '0');
    mu_assert_int_equals(obuf[4], '0');
    mu_assert_int_equals(obuf[5], '1');
    return 0;
}


static char* testBadDecode()
{
    char obuf[100];
    char ibuf[100];

    memset(obuf, 0, sizeof(obuf));
    memset(ibuf, 0, sizeof(ibuf));

    /* we are testing input of 2 bytes, 4 bytes
     * to test all possible screwups
     */
    strcpy(ibuf, "X1");
    mu_assert_int_equals(-1, modp_b16_decode(obuf, ibuf,(int) strlen(ibuf)));
    strcpy(ibuf, "1X");
    mu_assert_int_equals(-1, modp_b16_decode(obuf, ibuf,(int) strlen(ibuf)));
    strcpy(ibuf, "XX");
    mu_assert_int_equals(-1, modp_b16_decode(obuf, ibuf,(int) strlen(ibuf)));

    /* 1 bad char */
    strcpy(ibuf, "X111");
    mu_assert_int_equals(-1, modp_b16_decode(obuf, ibuf,(int) strlen(ibuf)));
    strcpy(ibuf, "1X11");
    mu_assert_int_equals(-1, modp_b16_decode(obuf, ibuf,(int) strlen(ibuf)));
    strcpy(ibuf, "11X1");
    mu_assert_int_equals(-1, modp_b16_decode(obuf, ibuf,(int) strlen(ibuf)));
    strcpy(ibuf, "111X");
    mu_assert_int_equals(-1, modp_b16_decode(obuf, ibuf,(int) strlen(ibuf)));

    /* 2 bad chars */
    strcpy(ibuf, "XX11");
    mu_assert_int_equals(-1, modp_b16_decode(obuf, ibuf,(int) strlen(ibuf)));
    strcpy(ibuf, "1XX1");
    mu_assert_int_equals(-1, modp_b16_decode(obuf, ibuf,(int) strlen(ibuf)));
    strcpy(ibuf, "11XX");
    mu_assert_int_equals(-1, modp_b16_decode(obuf, ibuf,(int) strlen(ibuf)));
    strcpy(ibuf, "X1X1");
    mu_assert_int_equals(-1, modp_b16_decode(obuf, ibuf,(int) strlen(ibuf)));
    strcpy(ibuf, "1X1X");
    mu_assert_int_equals(-1, modp_b16_decode(obuf, ibuf,(int) strlen(ibuf)));
    strcpy(ibuf, "X11X");
    mu_assert_int_equals(-1, modp_b16_decode(obuf, ibuf,(int) strlen(ibuf)));

    /* 3 bad chars */
    strcpy(ibuf, "1XXX");
    mu_assert_int_equals(-1, modp_b16_decode(obuf, ibuf,(int) strlen(ibuf)));
    strcpy(ibuf, "X1XX");
    mu_assert_int_equals(-1, modp_b16_decode(obuf, ibuf,(int) strlen(ibuf)));
    strcpy(ibuf, "XX1X");
    mu_assert_int_equals(-1, modp_b16_decode(obuf, ibuf,(int) strlen(ibuf)));
    strcpy(ibuf, "XXX1");
    mu_assert_int_equals(-1, modp_b16_decode(obuf, ibuf,(int) strlen(ibuf)));

    /* 4 bad chars */
    strcpy(ibuf, "XXXX");
    mu_assert_int_equals(-1, modp_b16_decode(obuf, ibuf,(int) strlen(ibuf)));

    return 0;
}

static char* testEmptyInput()
{
    char obuf[100];
    char ibuf[100];

    memset(obuf, 0, sizeof(obuf));
    memset(ibuf, 0, sizeof(ibuf));

    // encode 0 bytes, get a null byte back
    obuf[0] = 1;
    mu_assert_int_equals(0, modp_b16_encode(obuf, ibuf,0));
    mu_assert_int_equals(0, obuf[0]);

    // decode 0 bytes, buffer is untouched
    obuf[0] = 1;
    mu_assert_int_equals(0, modp_b16_decode(obuf, ibuf, 0));
    mu_assert_int_equals(1, obuf[0]);
    return 0;
}

static char* testLengths()
{
    /* Decode Len
     * 2 input -> 1 output.  No added NULL byte
     */
    mu_assert_int_equals(0, modp_b16_decode_len(0));
    mu_assert_int_equals(1, modp_b16_decode_len(1));
    mu_assert_int_equals(1, modp_b16_decode_len(2));
    mu_assert_int_equals(2, modp_b16_decode_len(3));
    mu_assert_int_equals(2, modp_b16_decode_len(4));

    /* Encode Len
     * 1 byte -> 2 output + 1 null byte
     */
    mu_assert_int_equals(1, modp_b16_encode_len(0));
    mu_assert_int_equals(3, modp_b16_encode_len(1));
    mu_assert_int_equals(5, modp_b16_encode_len(2));
    mu_assert_int_equals(7, modp_b16_encode_len(3));
    mu_assert_int_equals(9, modp_b16_encode_len(4));
    return 0;
}

static char* all_tests() {
    mu_run_test(testEndian);
    mu_run_test(testEncodeDecode);
    mu_run_test(testLengths);
    mu_run_test(testEmptyInput);
    mu_run_test(testBadDecode);
    mu_run_test(testOddDecode);
    mu_run_test(testOddEncode);
    mu_run_test(testDecodeMutlipleOf2);
    return 0;
}

UNITTESTS
