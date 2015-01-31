/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
/* vi: set expandtab shiftwidth=4 tabstop=4: */

/**
 * \file
 * <PRE>
 * MODP_B85 - High performance base85 encoder/decoder
 * http://code.google.com/p/stringencoders/
 *
 * Copyright &copy; 2006, 2007  Nick Galbreath -- nickg [at] modp [dot] com
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 *   Neither the name of the modp.com nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This is the standard "new" BSD license:
 * http://www.opensource.org/licenses/bsd-license.php
 * </pre>
 */

/* exported public header */
#include "modp_b85.h"

#include <stdint.h>

/* private header */
#include "modp_b85_data.h"

/*
 * Might need changing depending on platform
 * we need htonl, and ntohl
 */
#include <arpa/inet.h>

/**
 * you can decode IN PLACE!
 * no memory allocated
 */
int modp_b85_decode(char* out, const char* data, int len)
{
    int i,j;
    const int buckets = len / 5;
    const uint8_t* d2 = (const uint8_t*) data;
    if (len % 5 != 0) {
        return -1;
    }

    uint32_t* o2  = (uint32_t*)out;
    for (i = 0; i < buckets; ++i) {
        uint32_t tmp = 0;
        for (j = 0; j < 5; ++j) {
            uint32_t digit =  gsCharToInt[(uint32_t) *d2++];
            if (digit >= 85) {
                return -1;
            }
            tmp = tmp * 85 + digit;
        }
        *o2++ = htonl(tmp);
    }
    return 4 * buckets;
}

/**
 * src != out
 */
int modp_b85_encode(char* out, const char* src, int len)
{
    const uint32_t* sary = (const uint32_t*) src;
    const int buckets = len / 4;
    if (len % 4 != 0) {
        return -1;
    }

    int i;
    for (i = 0; i < buckets; ++i) {
        uint32_t tmp = *sary++;
        tmp = htonl(tmp);

        /* this crazy function */
#if 1
        *out++ =  (char)gsIntToChar[(tmp / 52200625)]; // don't need % 85 here, always < 85
        *out++ =  (char)gsIntToChar[(tmp / 614125) % 85];
        *out++ =  (char)gsIntToChar[(tmp / 7225) % 85];
        *out++ =  (char)gsIntToChar[(tmp / 85) % 85];
        *out++ =  (char)gsIntToChar[tmp % 85];
#else
        /* is really this */
        *(out+4) =  gsIntToChar[tmp % 85]; tmp /= 85;
        *(out+3) =  gsIntToChar[tmp % 85]; tmp /= 85;
        *(out+2) =  gsIntToChar[tmp % 85]; tmp /= 85;
        *(out+1) =  gsIntToChar[tmp % 85]; tmp /= 85;
        *out =  gsIntToChar[tmp];
        out += 5;
#endif
        // NOTES
        // Version 1 under -O3 is about 10-20 PERCENT faster than version 2
        // BUT Version 1 is 10 TIMES SLOWER when used with -Os !!!
        // Reason: gcc does a lot of tricks to remove the divisions
        //  op with multiplies and shift.
        // In V1 with -O3 this works.  Under -Os it reverts to very
        //   slow division.
        // In V2 -O3 it does the same thing, but under Os, it's smart
        // enough to know we want the quotient and remainder and only
        // one div call per line.

    }
    *out = 0; // final null
    return buckets * 5;
}
