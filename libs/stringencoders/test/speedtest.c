/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
/* vi: set expandtab shiftwidth=4 tabstop=4: */

/** \file speedtest.c
 *
 * Copyright 2005,2006,2007, Nick Galbreath -- nickg [at] modp [dot] com
 * All rights reserved.
 *
 * http://code.google.com/p/stringencoders/
 *
 * Released under bsd license.  See modp_b64.c for details.
 *
 * Quickie performance tester.  This does NOT test correctness.
 *
 */

#include "modp_b2.h"
#include "modp_b16.h"
#include "modp_b64.h"
#include "apr_base64.h"
#include "modp_b85.h"
#include "modp_burl.h"
#include "modp_bjavascript.h"

#include <time.h>
#ifndef CLOCKS_PER_SEC
# ifdef CLK_TCK
#  define CLOCKS_PER_SEC (CLK_TCK)
# endif
#endif
#include <stdio.h>
#include <string.h>

#define SZ 4096
int main() {
    double s1, s2;
    int i, j;
    clock_t c0, c1;
    char teststr1[SZ];
    char teststr2[SZ];

    /*
      this contains the message sizes we'll test on
      add, subtract, change as desired.
    */
    int sizes[] = {20, 200, 2000};

    for (i = 0; i < (int)sizeof(teststr1); ++i) {
        teststr1[i] = 'A' + i % 26;
        teststr2[i] = 'A' + i % 26;
    }

    // over allocate result buffers
    char result[SZ*8];
    char result2[SZ*8];

    const int MAX = 1000000;

    for (j = 0; j < (int)(sizeof(sizes)/sizeof(int)); ++j) {
        printf("\nMessage size = %d\n", sizes[j]);

        printf("\tmodpb64\tapache\timprovement\tmodpb85\tmodpurl\tmodpb16\tmodpb2\tmodpjs\n");
        printf("Encode\t");
        fflush(stdout);

        /* MODP_B64 ENCODE */
        c0 = clock();
        for (i = 0; i < MAX; ++i) {
            modp_b64_encode(result, teststr1, sizes[j]);
        }
        c1 = clock();
        s1 = (c1 - c0)*(1.0 / (double)CLOCKS_PER_SEC);
        printf("%6.2f\t", s1);
        fflush(stdout);

        /* APACHE ENCODE */
        c0 = clock();
        for (i = 0; i < MAX; ++i) {
            apr_base64_encode_binary(result,
                                     (const unsigned char*) teststr1,
                                     sizes[j]);
        }
        c1 = clock();
        s2 = (c1 - c0)*(1.0 / (double)CLOCKS_PER_SEC);
        printf("%6.2f\t",  s2);
        printf("%6.2fx\t\t", s2/s1);
        fflush(stdout);


        /*
         * base85 encode, let's see what is faster
         */
        c0 = clock();
        for (i = 0; i < MAX; ++i) {
            modp_b85_encode(result, teststr1, sizes[j]);
        }
        c1 = clock();
        s1 = (c1 - c0)*(1.0 / (double)CLOCKS_PER_SEC);
        printf("%6.2f\t", s1);
        fflush(stdout);

        /*
         * url encode
         */
        c0 = clock();
        for (i = 0; i < MAX; ++i) {
            modp_burl_encode(result, teststr1, sizes[j]);
        }
        c1 = clock();
        s1 = (c1 - c0)*(1.0 / (double)CLOCKS_PER_SEC);
        printf("%6.2f\t", s1);
        fflush(stdout);

        /**
         * B16
         */
        c0 = clock();
        for (i = 0; i < MAX; ++i) {
            modp_b16_encode(result, teststr1, sizes[j]);
        }
        c1 = clock();
        s1 = (c1 - c0)*(1.0 / (double)CLOCKS_PER_SEC);
        printf("%6.2f\t", s1);
        fflush(stdout);

        /**
         * B2 BINARY
         */
        c0 = clock();
        for (i = 0; i < MAX; ++i) {
            modp_b2_encode(result, teststr1, sizes[j]);
        }
        c1 = clock();
        s1 = (c1 - c0)*(1.0 / (double)CLOCKS_PER_SEC);
        printf("%6.2f\t", s1);
        fflush(stdout);

        /**
         * javascript
         */
        c0 = clock();
        for (i = 0; i < MAX; ++i) {
            modp_bjavascript_encode(result, teststr1, sizes[j]);
        }
        c1 = clock();
        s1 = (c1 - c0)*(1.0 / (double)CLOCKS_PER_SEC);
        printf("%6.2f\t", s1);
        fflush(stdout);

        printf("\n");
        fflush(stdout);

        /***
         * DECODE
         */
        /* reset result to have b64 chars */
        modp_b64_encode(result, teststr1, sizes[j]);
        int len = strlen(result);

        printf("Decode\t");
        fflush(stdout);

        /* MODP_B64 */
        c0 = clock();
        for (i = 0; i < MAX; ++i) {
            modp_b64_decode(result2, result, len);
        }
        c1 = clock();
        s1 =  (c1 - c0)*(1.0 / (double)CLOCKS_PER_SEC);
        printf("%6.2f\t", s1);
        fflush(stdout);

        /* APACHE */
        c0 = clock();
        for (i = 0; i < MAX; ++i) {
            apr_base64_decode_binary((unsigned char*)result2, result);
        }
        c1 = clock();
        s2 =  (c1 - c0)*(1.0 / (double)CLOCKS_PER_SEC);
        printf("%6.2f\t",  s2);
        printf("%6.2fx\t\t", s2/s1);
        fflush(stdout);

        /*
         * modp_b85 decode
         */
        /* re-encode to get b85 chars, not b64 */
        len = modp_b85_encode(result, teststr1, sizes[j]);

        c0 = clock();
        for (i = 0; i < MAX; ++i) {
            modp_b85_decode(result2, result, len);
        }
        c1 = clock();
        s1 =  (c1 - c0)*(1.0 / (double)CLOCKS_PER_SEC);
        printf("%6.2f\t", s1);
        fflush(stdout);

        /* re-encode to get urlencoded chars, not b64 */
        len = modp_burl_encode(result, teststr1, sizes[j]);

        c0 = clock();
        for (i = 0; i < MAX; ++i) {
            modp_burl_decode(result2, result, len);
        }
        c1 = clock();
        s1 =  (c1 - c0)*(1.0 / (double)CLOCKS_PER_SEC);
        printf("%6.2f\t", s1);
        fflush(stdout);

        /**
         ** B16 DECODE
         **/
        /* re-encode to get urlencoded chars, not b64 */
        len = modp_b16_encode(result, teststr1, sizes[j]);

        c0 = clock();
        for (i = 0; i < MAX; ++i) {
            modp_b16_decode(result2, result, len);
        }
        c1 = clock();
        s1 =  (c1 - c0)*(1.0 / (double)CLOCKS_PER_SEC);
        printf("%6.2f\t", s1);
        fflush(stdout);

        /**
         ** B16 DECODE
         **/
        /* re-encode to get urlencoded chars, not b64 */
        len = modp_b2_encode(result, teststr1, sizes[j]);

        c0 = clock();
        for (i = 0; i < MAX; ++i) {
            modp_b2_decode(result2, result, len);
        }
        c1 = clock();
        s1 =  (c1 - c0)*(1.0 / (double)CLOCKS_PER_SEC);
        printf("%6.2f\t", s1);
        fflush(stdout);

        printf("\n");
        fflush(stdout);
    }

    return 0;
}
