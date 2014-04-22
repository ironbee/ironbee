/*
* ModSecurity for Apache 2.x, http://www.modsecurity.org/
* Copyright (c) 2004-2011 Trustwave Holdings, Inc. (http://www.trustwave.com/)
*
* You may not use this file except in compliance with
* the License. You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* If any of the files related to licensing are missing or if you have any
* other questions related to licensing please contact Trustwave Holdings, Inc.
* directly using the email address security@modsecurity.org.
*/

/**
 * @file
 * @brief IronBee --- Utility Functions from ModSecurity
 *
 * These utility functions and routines are based on ModSecurity code.
 *
 * https://www.modsecurity.org/
 */

#include "ironbee_config_auto.h"

#include <ironbee/decode.h>
#include <ironbee/path.h>
#include <ironbee/string.h>
#include <ironbee/util.h>

#include <assert.h>
#include <ctype.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>

/**
 * Check if a character is a valid hex character
 *
 * NOTE: Be careful as these can ONLY be used on static values for X.
 * (i.e. IS_HEX_CHAR(c++) will NOT work)
 *
 * @param[in] ch The character to check
 *
 * @returns: true if the @a ch is a valid hex character, otherwise false
 */
#define IS_HEX_CHAR(ch) \
    ( (((ch) >= '0') && ((ch) <= '9')) || \
      (((ch) >= 'a') && ((ch) <= 'f')) || \
      (((ch) >= 'A') && ((ch) <= 'F')) )

/** ASCII code for a non-breaking space. */
#define NBSP 160

/**
 * Convert a 2-character string from hex to a digit.
 *
 * Converts the two chars @a ptr given as its hexadecimal representation
 * into a proper byte. Handles uppercase and lowercase letters
 * but does not check for overflows.
 *
 * @param[in] ptr Pointer to input
 *
 * @returns The digit
 */
static uint8_t x2c(const uint8_t *ptr)
{
    uint8_t digit;
    uint8_t c;

    c = *(ptr + 0);
    digit = ( (c >= 'A') ? ((c & 0xdf) - 'A') + 10 : (c - '0') );

    digit *= 16;

    c = *(ptr + 1);
    digit += ( (c >= 'A') ? ((c & 0xdf) - 'A') + 10 : (c - '0') );

    return digit;
}

ib_status_t ib_util_decode_url(
    const uint8_t  *data_in,
    size_t          dlen_in,
    uint8_t        *data_out,
    size_t         *dlen_out
)
{
    assert(data_in != NULL);
    assert(data_out != NULL);
    assert(dlen_out != NULL);

    uint8_t *out = data_out;
    const uint8_t *in  = data_in;
    const uint8_t *end = data_in + dlen_in;

    while (in < end) {
        if (*in == '%') {
            /* Character is a percent sign. */

            /* Are there enough bytes available? */
            if (in + 2 < end) {
                char c1 = *(in + 1);
                char c2 = *(in + 2);

                if (IS_HEX_CHAR(c1) && IS_HEX_CHAR(c2)) {
                    /* Valid encoding - decode it. */
                    *out++ = x2c(in + 1);
                    in += 3;
                }
                else {
                    /* Not a valid encoding, skip this % */
                    if (in == out) {
                        ++out;
                        ++in;
                    }
                    else {
                        *out++ = *in++;
                    }
                }
            }
            else {
                /* Not enough bytes available, copy the raw bytes. */
                if (in == out) {
                    ++out;
                    ++in;
                }
                else {
                    *out++ = *in++;
                }
            }
        }
        else {
            /* Character is not a percent sign. */
            if (*in == '+') {
                *out++ = ' ';
            }
            else if (out != in) {
                *out++ = *in;
            }
            else {
                ++out;
            }
            ++in;
        }
    }
    *dlen_out = (out - data_out);

    return IB_OK;
}

ib_status_t ib_util_decode_html_entity(
    const uint8_t  *data_in,
    size_t          dlen_in,
    uint8_t        *data_out,
    size_t         *dlen_out
)
{
    assert(data_in != NULL);
    assert(data_out != NULL);
    assert(dlen_out != NULL);

    uint8_t *out = data_out;
    const uint8_t *in = data_in;
    const uint8_t *end;

    end = in + dlen_in;
    while (in < end) {
        size_t copy = 1;

        /* Require an ampersand and at least one character to
         * start looking into the entity.
         */
        if ( (*in == '&') && (in + 1 < end) ) {
            const uint8_t *t1 = in + 1;

            if (*t1 == '#') {
                /* Numerical entity. */
                ++copy;

                if (t1 + 1 >= end) {
                    goto HTML_ENT_OUT; /* Not enough bytes. */
                }
                ++t1;

                if ( (*t1 == 'x') || (*t1 == 'X') ) {
                    const uint8_t *t2;

                    /* Hexadecimal entity. */
                    ++copy;

                    if (t1 + 1 >= end) {
                        goto HTML_ENT_OUT; /* Not enough bytes. */
                    }
                    ++t1; /* j is the position of the first digit now. */

                    t2 = t1;
                    while( (t1 < end) && (isxdigit(*t1)) ) {
                        ++t1;
                    }
                    if (t1 > t2) { /* Do we have at least one digit? */
                        /* Decode the entity. */
                        char *tmp = ib_util_memdup_to_string(t2, t1 - t2);
                        if (tmp == NULL) {
                            return IB_EALLOC;
                        }
                        *out++ = (uint8_t)strtol(tmp, NULL, 16);
                        free(tmp);

                        /* Skip over the semicolon if it's there. */
                        if ( (t1 < end) && (*t1 == ';') ) {
                             in = t1 + 1;
                        }
                        else {
                            in = t1;
                        }

                        continue;
                    }
                    else {
                        goto HTML_ENT_OUT;
                    }
                }
                else {
                    /* Decimal entity. */
                    const uint8_t *t2 = t1;

                    while( (t1 < end) && (isdigit(*t1)) ) {
                        ++t1;
                    }
                    if (t1 > t2) { /* Do we have at least one digit? */
                        /* Decode the entity. */
                        char *tmp = ib_util_memdup_to_string(t2, t1 - t2);
                        if (tmp == NULL) {
                            return IB_EALLOC;
                        }
                        *out++ = (uint8_t)strtol(tmp, NULL, 10);
                        free(tmp);

                        /* Skip over the semicolon if it's there. */
                        if ( (t1 < end) && (*t1 == ';') ) {
                             in = t1 + 1;
                        }
                        else {
                            in = t1;
                        }

                        continue;
                    }
                    else {
                        goto HTML_ENT_OUT;
                    }
                }
            }
            else {
                /* Text entity. */
                const uint8_t *t2 = t1;

                while( (t1 < end) && (isalnum(*t1)) ) {
                    ++t1;
                }
                if (t1 > t2) { /* Do we have at least one digit? */
                    size_t tlen = t1 - t2;
                    char *tmp = ib_util_memdup_to_string(t2, tlen);
                    if (tmp == NULL) {
                        return IB_EALLOC;
                    }

                    /* Decode the entity. */
                    /* ENH What about others? */
                    if (strcasecmp(tmp, "quot") == 0) {
                        *out++ = '"';
                    }
                    else if (strcasecmp(tmp, "amp") == 0) {
                        *out++ = '&';
                    }
                    else if (strcasecmp(tmp, "lt") == 0) {
                        *out++ = '<';
                    }
                    else if (strcasecmp(tmp, "gt") == 0) {
                        *out++ = '>';
                    }
                    else if (strcasecmp(tmp, "nbsp") == 0) {
                        *out++ = NBSP;
                    }
                    else {
                        /* We do no want to convert this entity,
                         * copy the raw data over. */
                        copy = t1 - t2 + 1;
                        free(tmp);
                        goto HTML_ENT_OUT;
                    }
                    free(tmp);

                    /* Skip over the semicolon if it's there. */
                    if ( (t1 < end) && (*t1 == ';') ) {
                        in = t1 + 1;
                    }
                    else {
                         in = t1;
                    }
                    continue;
                }
            }
        }

  HTML_ENT_OUT:
        {
            size_t z;
            for(z = 0; ( (z < copy) && (in < end) ); ++z) {
                *out++ = *in++;
            }
        }
    }

    *dlen_out = (out - data_out);

    return IB_OK;
}

ib_status_t ib_util_normalize_path(
    ib_mm_t mm,
    const uint8_t *data_in,
    size_t dlen_in,
    bool win,
    uint8_t **data_out,
    size_t *dlen_out
)
{
    assert(data_in != NULL);
    assert(data_out != NULL);
    assert(dlen_out != NULL);

    uint8_t *buf;
    uint8_t *src;
    uint8_t *dst;
    const uint8_t *end;
    bool hitroot = false;
    bool done = false;
    bool relative;
    bool trailing;
    bool modified = false;

    /* Copy original */
    buf = ib_mm_alloc(mm, dlen_in);
    if (buf == NULL) {
        return IB_EALLOC;
    }
    memcpy(buf, data_in, dlen_in);
    
    src = dst = buf;
    end = src + (dlen_in - 1);
    
    /* Some special cases */
    if (dlen_in == 0) {
        goto finish;
    }
    else if ( (dlen_in == 1) && (*src == '/') ) {
        dst = buf + 1;
        goto finish;
    }
    else if ( (dlen_in == 2) && (*src == '.') && (*(src + 1) == '.') ) {
        dst = buf + 2;
        goto finish;
    }

    /*
     * ENH: Deal with UNC and drive letters?
     */

    relative = ! ((!win && *buf == '/') || (win && (*buf == '\\')));
    trailing = ((!win && *end == '/') || (win && (*end == '\\')));

    while ( (! done) && (src <= end) && (dst <= end) ) {

        /* Convert backslash to forward slash on Windows only. */
        if (win) {
            if (*src == '\\') {
                *src = '/';
            }
            if ((src < end) && (*(src + 1) == '\\')) {
                *(src + 1) = '/';
            }
        }

        /* Always normalize at the end of the input. */
        if (src == end) {
            done = true;
        }

        /* Skip normalization if this is NOT the end of the path segment. */
        /* else if ( (src + 1 != end) && (*(src + 1) != '/') ) { */
        else if (*(src + 1) != '/') {
            goto copy; /* Skip normalization. */
        }

        /*** Normalize the path segment. ***/

        /* Could it be an empty path segment? */
        if ( (src != end) && (*src == '/') ) {
            /* Ignore */
            goto copy; /* Copy will take care of this. */
        }

        /* Could it be a back or self reference? */
        else if (*src == '.') {

            /* Back-reference? */
            if ( (dst > buf) && (*(dst - 1) == '.') ) {
                /* If a relative path and either our normalization has
                 * already hit the rootdir, or this is a backref with no
                 * previous path segment, then mark that the rootdir was hit
                 * and just copy the backref as no normalization is possible.
                 */
                if (relative && (hitroot || ((dst - 2) <= buf))) {
                    hitroot = true;

                    goto copy; /* Skip normalization. */
                }

                /* Remove backreference and the previous path segment. */
                modified = true; /* ? */
                dst -= 3;
                while ( (dst > buf) && (*dst != '/') ) {
                    --dst;
                }

                /* But do not allow going above rootdir. */
                if (dst <= buf) {
                    hitroot = true;
                    dst = buf;

                    /* Need to leave the root slash if this
                     * is not a relative path and the end was reached
                     * on a backreference.
                     */
                    if (!relative && (src == end)) {
                        ++dst;
                    }
                }

                if (done) {
                    continue; /* Skip the copy. */
                }
                ++src;
                modified = true;
            }

            /* Relative Self-reference? */
            else if (dst == buf) {
                modified = true;

                /* Ignore. */
                if (done) {
                    continue; /* Skip the copy. */
                }
                ++src;
            }

            /* Self-reference? */
            else if (*(dst - 1) == '/') {
                modified = true;

                /* Ignore. */
                if (done) {
                    continue; /* Skip the copy. */
                }
                --dst;
                ++src;
            }
        }

        /* Found a regular path segment. */
        else if (dst > buf) {
            hitroot = false;
        }

  copy:
        /*** Copy the byte if required. ***/

        /* Skip to the last forward slash when multiple are used. */
        if (*src == '/') {
            uint8_t *tmp = src;

            while ( (src < end) &&
                    ( (*(src + 1) == '/') || (win && (*(src + 1) == '\\')) ) )
            {
                ++src;
            }
            if (tmp != src) {
                modified = true;
            }

            /* Do not copy the forward slash to the root
             * if it is not a relative path.  Instead
             * move over the slash to the next segment.
             */
            if (relative && (dst == buf)) {
                ++src;
                continue;
            }
        }

        *(dst++) = *(src++);
    }

    /* Make sure that there is not a trailing slash in the
     * normalized form if there was not one in the original form.
     */
    if (!trailing && (dst > buf) && *(dst - 1) == '/') {
        --dst;
    }
    if (!relative && (dst == buf) ) {
        ++dst;
    }

finish:
    *data_out = buf;
    *dlen_out = (dst - buf);

    return IB_OK;
}
