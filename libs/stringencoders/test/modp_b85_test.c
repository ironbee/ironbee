/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
/* vi: set expandtab shiftwidth=4 tabstop=4: */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "modp_b85.h"
#include "minunit.h"

/**
 * checks to make sure results are the same reguardless of
 * CPU endian type (i.e. Intel vs. Sparc, etc)
 *
 */
static char* testEndian()
{
    // this test that "1" is "!!!!#"
    char buf[100];
    char result[10];
    char endian[] = {(char)0, (char)0, (char)0, (char)1};
    int d = modp_b85_encode(buf, endian, 4);
    mu_assert_int_equals(5, d);
    mu_assert_int_equals('!', buf[0]);
    mu_assert_int_equals('!', buf[1]);
    mu_assert_int_equals('!', buf[2]);
    mu_assert_int_equals('!', buf[3]);
    mu_assert_int_equals('#', buf[4]);

    memset(result, 255, sizeof(result));
    d = modp_b85_decode(result, "!!!!#", 5);
    mu_assert_int_equals(4, d);
    mu_assert_int_equals(0, result[0]);
    mu_assert_int_equals(0, result[1]);
    mu_assert_int_equals(0, result[2]);
    mu_assert_int_equals(1, result[3]);
    mu_assert_int_equals(-1, result[4]);

    return 0;
}

static char* testLength()
{
    /**
     * Decode Len
     * 5 bytes -> 4 in, no null
     */
    mu_assert_int_equals(0, modp_b85_decode_len(0));
    mu_assert_int_equals(4, modp_b85_decode_len(5));
    mu_assert_int_equals(8, modp_b85_decode_len(10));

    /* Encode Len
     * 4 byte -> 5 output + 1 null byte
     */
    mu_assert_int_equals( 1, modp_b85_encode_len(0));
    mu_assert_int_equals( 6, modp_b85_encode_len(4));
    mu_assert_int_equals(11, modp_b85_encode_len(8));
    return 0;
}

/** \brief test bad input lengths
 * The b85 encode only accepts multiples of 4.
 *
 * The b85 decoder only accepts a mutliple of 5.
 *
 */
static char* testBadInputLength()
{
    char buf[100];
    mu_assert_int_equals(-1, modp_b85_encode(buf, buf, 5));
    mu_assert_int_equals(-1, modp_b85_decode(buf, buf, 11));
    return 0;
}

/** \brief test decoding invalid b85 chars
 *
 */
static char* testBadCharDecode()
{
    char buf[] = {'A', 'B', 'C', 'D', '\n', '\0'};
    mu_assert_int_equals(-1, modp_b85_decode(buf, buf, 5));
    return 0;
}

static char* testEncodeDecode()
{
    char ibuf[10]; /* input */
    char obuf[10]; /* output */
    char rbuf[10]; /* final result */
    int d;
    int i,j,k,l;
    for (i = 0; i < 256; ++i) {
        for (j = 0; j < 256; j+=16) { // save some time +=16
            for (k = 0; k < 256; k += 8) { // save some time += 8
                for (l = 0; l < 256; ++l) {
                    ibuf[0] = i;
                    ibuf[1] = j;
                    ibuf[2] = k;
                    ibuf[3] = l;
                    memset(obuf, 255, sizeof(obuf));
                    d = modp_b85_encode(obuf, ibuf, 4);
                    mu_assert_int_equals(5, d);
                    mu_assert_int_equals(0, obuf[5]);
                    memset(rbuf, 255, sizeof(rbuf));
                    d = modp_b85_decode(rbuf, obuf, 5);
                    mu_assert_int_equals(4, d);
                    mu_assert_int_equals(ibuf[0], rbuf[0]);
                    mu_assert_int_equals(ibuf[1], rbuf[1]);
                    mu_assert_int_equals(ibuf[2], rbuf[2]);
                    mu_assert_int_equals(ibuf[3], rbuf[3]);
                    mu_assert_int_equals(-1, rbuf[4]);
                }
            }
        }
    }
    return 0;
}

static char* all_tests()
{
    mu_run_test(testEndian);
    mu_run_test(testLength);
    mu_run_test(testBadInputLength);
    mu_run_test(testBadCharDecode);
    mu_run_test(testEncodeDecode);
    return 0;
}

UNITTESTS

