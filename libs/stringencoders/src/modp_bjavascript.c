/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
/* vi: set expandtab shiftwidth=4 tabstop=4: */

/**
 * \file
 * <pre>
 * modp_bjavascript.c High performance URL encoder/decoder
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
 * </PRE>
 */
#include <stdint.h>

#include "modp_bjavascript.h"
#include "modp_bjavascript_data.h"

int modp_bjavascript_encode(char* dest, const char* src, int len)
{
    const char* deststart = dest;
    const uint8_t* s = (const uint8_t*) src;
    const uint8_t* srcend = s + len;
    uint8_t x;
    uint8_t val;

    // if 0, do nothing
    // if 'A', hex escape
    // else, \\ + value
    while (s < srcend) {
        x = *s++;
        val = gsJavascriptEncodeMap[x];
        if (val == 0) {
            *dest++ = (char) x;
        } else if (val == 'A') {
            *dest++ = '\\';
            *dest++ = 'x';
            *dest++ = (char)(gsHexEncodeMap1[x]);
            *dest++ = (char)(gsHexEncodeMap2[x]);
        } else {
            *dest++ = '\\';
            *dest++ = (char)val;
        }
    }
    *dest = '\0';
    return (int)(dest - deststart);
}

int modp_bjavascript_encode_strlen(const char* src, int len)
{
    const uint8_t* s = (const uint8_t*)src;
    const uint8_t* srcend = s + len;
    int count = 0;
    uint8_t val;

    while (s < srcend) {
        val = gsJavascriptEncodeMap[*s++];
        if (val == 0) {
            count++;
        } else if (val == 'A') {
            count += 4;
        } else {
            count += 2;
        }
    }
    return count;
}
