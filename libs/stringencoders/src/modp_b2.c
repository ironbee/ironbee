/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
/* vi: set expandtab shiftwidth=4 tabstop=4: */

/**
 * \file modp_b2.c
 * <PRE>
 * MODP_B2 - Ascii Binary string encode/decode
 * http://code.google.com/p/stringencoders/
 *
 * Copyright &copy; 2005, 2006,2007  Nick Galbreath -- nickg [at] modp [dot] com
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
 * </PRE>
 */
#include <stdint.h>
#include <string.h>
#include "config.h"
#include "modp_b2.h"
#include "modp_b2_data.h"


int modp_b2_encode(char* dest, const char* str, int len)
{
    const uint8_t* orig = (const uint8_t*) str;
#if 0
    /* THIS IS A STANDARD VERSION */
    static const unsigned char gsBinaryChars[] = "01";
    int i,j;
    for (i = 0; i < len; ++i) {
        for (j = 0; j <= 7; ++j) {
            *dest++ = gsBinaryChars[(orig[i] >> (7-j)) & 1];
        }
    }
#else
    /* THIS IS 10X FASTER */
    int i;
    for (i = 0; i < len; ++i) {
        memcpy(dest, modp_b2_encodemap[orig[i]], 8);
        dest += 8;
    }
#endif
    *dest = '\0';
    return  len*8;
}

int modp_b2_decode(char* dest, const char* str, int len)
{
    char d;
    int i,j;
    const int buckets = len / 8;
    const int leftover = len % 8;
    if (leftover != 0) {
        return -1;
    }

    for (i = 0; i < buckets; ++i) {
        d = 0;
        for (j = 0; j <= 7; ++j) {
            char c = *str++;
            if (c == '1') {
                d ^= (char)(1 << (7-j));
            }
        }
        *dest++ = d;
    }

    return buckets;
}
