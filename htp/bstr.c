/***************************************************************************
 * Copyright (c) 2009-2010, Open Information Security Foundation
 * Copyright (c) 2009-2012, Qualys, Inc.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * * Neither the name of the Qualys, Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

/**
 * @file
 * @author Ivan Ristic <ivanr@webkreator.com>
 */

#include "bstr.h"
#include <ctype.h>

bstr *bstr_alloc(size_t len) {
    unsigned char *s = malloc(sizeof (bstr_t) + len);
    if (s == NULL) return NULL;

    bstr_t *b = (bstr_t *) s;
    b->len = 0;
    b->size = len;
    b->ptr = NULL;

    return (bstr *) s;
}

void bstr_free(bstr **b) {
    if ((b == NULL) || (*b == NULL)) return;
    free(*b);
    *b = NULL;
}

bstr *bstr_add(bstr *destination, const bstr *source) {
    return bstr_add_mem(destination, bstr_ptr(source), bstr_len(source));
}

bstr *bstr_add_c(bstr *destination, const char *source) {
    return bstr_add_mem(destination, source, strlen(source));
}

bstr *bstr_add_mem(bstr *destination, const char *data, size_t len) {
    // Expand the destination if necessary
    if (bstr_size(destination) < bstr_len(destination) + len) {
        destination = bstr_expand(destination, bstr_len(destination) + len);
        if (destination == NULL) return NULL;
    }

    // Add source to destination
    bstr_t *b = (bstr_t *) destination;
    memcpy(bstr_ptr(destination) + b->len, data, len);
    b->len = b->len + len;

    return destination;
}

bstr *bstr_add_noex(bstr *destination, const bstr *source) {
    return bstr_add_mem_noex(destination, bstr_ptr(source), bstr_len(source));
}

bstr *bstr_add_c_noex(bstr *destination, const char *source) {
    return bstr_add_mem_noex(destination, source, strlen(source));
}

bstr *bstr_add_mem_noex(bstr *destination, const char *data, size_t len) {
    size_t copylen = len;

    // Is there enough room in the destination?
    if (bstr_size(destination) < bstr_len(destination) + copylen) {
        copylen = bstr_size(destination) - bstr_len(destination);
        if (copylen <= 0) return destination;
    }

    // Copy over the bytes
    bstr_t *b = (bstr_t *) destination;
    memcpy(bstr_ptr(destination) + b->len, data, copylen);
    b->len = b->len + copylen;

    return destination;
}

bstr *bstr_expand(bstr *s, size_t newsize) {
    if (((bstr_t *) s)->ptr != NULL) {
        void * newblock = realloc(((bstr_t *) s)->ptr, newsize);
        if (newblock == NULL) {
            return NULL;
        } else {
            ((bstr_t *) s)->ptr = newblock;
        }
    } else {
        void *newblock = realloc(s, sizeof (bstr_t) + newsize);
        if (newblock == NULL) {
            return NULL;
        } else {
            s = newblock;
        }
    }

    ((bstr_t *) s)->size = newsize;

    return s;
}

bstr *bstr_dup_c(const char *data) {
    return bstr_dup_mem(data, strlen(data));
}

bstr *bstr_dup_mem(const char *data, size_t len) {
    bstr *b = bstr_alloc(len);
    if (b == NULL) return NULL;
    memcpy(bstr_ptr(b), data, len);
    ((bstr_t *) b)->len = len;
    return b;
}

bstr *bstr_dup(const bstr *b) {
    return bstr_dup_ex(b, 0, bstr_len(b));
}

bstr *bstr_dup_ex(const bstr *b, size_t offset, size_t len) {
    bstr *bnew = bstr_alloc(len);
    if (bnew == NULL) return NULL;
    memcpy(bstr_ptr(bnew), bstr_ptr(b) + offset, len);
    ((bstr_t *) bnew)->len = len;
    return bnew;
}

char *bstr_util_memdup_to_c(const char *data, size_t len) {
    // Count how many NUL bytes we have in the string.
    size_t i, nulls = 0;
    for (i = 0; i < len; i++) {
        if (data[i] == '\0') {
            nulls++;
        }
    }

    // Now copy the string into a NUL-terminated buffer.
    char *r, *t;
    r = t = malloc(len + nulls + 1);
    if (t == NULL) return NULL;

    while (len--) {
        // TODO We have to at least escape the \ characters too
        // Escape NUL bytes, but just copy everything else.
        if (*data == '\0') {
            data++;
            *t++ = '\\';
            *t++ = '0';
        } else {
            *t++ = *data++;
        }
    }

    // Terminate string.
    *t = '\0';

    return r;
}

char *bstr_util_strdup_to_c(const bstr *b) {
    if (b == NULL) return NULL;
    return bstr_util_memdup_to_c(bstr_ptr(b), bstr_len(b));
}

int bstr_chr(const bstr *b, int c) {
    char *data = bstr_ptr(b);
    size_t len = bstr_len(b);

    size_t i = 0;
    while (i < len) {
        if (data[i] == c) {
            return i;
        }

        i++;
    }

    return -1;
}

int bstr_rchr(const bstr *b, int c) {
    char *data = bstr_ptr(b);
    size_t len = bstr_len(b);

    int i = len;
    while (i >= 0) {
        if (data[i] == c) {
            return i;
        }

        i--;
    }

    return -1;
}

int bstr_cmp_ex(const char *s1, size_t l1, const char *s2, size_t l2) {
    size_t p1 = 0, p2 = 0;

    while ((p1 < l1) && (p2 < l2)) {
        if (s1[p1] != s2[p2]) {
            // Difference
            return (s1[p1] < s2[p2]) ? -1 : 1;
        }

        p1++;
        p2++;
    }

    if ((p1 == l2) && (p2 == l1)) {
        // They're identical
        return 0;
    } else {
        // One string is shorter
        if (p1 == l1) return -1;
        else return 1;
    }
}

int bstr_cmp_nocase_ex(const char *s1, size_t l1, const char *s2, size_t l2) {
    size_t p1 = 0, p2 = 0;

    while ((p1 < l1) && (p2 < l2)) {
        if (tolower((int) s1[p1]) != tolower((int) s2[p2])) {
            // Difference
            return (tolower((int) s1[p1]) < tolower((int) s2[p2])) ? -1 : 1;
        }

        p1++;
        p2++;
    }

    if ((p1 == l2) && (p2 == l1)) {
        // They're identical
        return 0;
    } else {
        // One string is shorter
        if (p1 == l1) return -1;
        else return 1;
    }
}

int bstr_cmp_c(const bstr *b, const char *c) {
    return bstr_cmp_ex(bstr_ptr(b), bstr_len(b), c, strlen(c));
}

int bstr_cmp_c_nocase(const bstr *b, const char *c) {
    return bstr_cmp_nocase_ex(bstr_ptr(b), bstr_len(b), c, strlen(c));
}

int bstr_cmp(const bstr *b1, const bstr *b2) {
    return bstr_cmp_ex(bstr_ptr(b1), bstr_len(b1), bstr_ptr(b2), bstr_len(b2));
}

int bstr_cmp_nocase(const bstr *b1, const bstr *b2) {
    return bstr_cmp_nocase_ex(bstr_ptr(b1), bstr_len(b1), bstr_ptr(b2), bstr_len(b2));
}

bstr *bstr_to_lowercase(bstr *b) {
    if (b == NULL) return NULL;

    unsigned char *data = (unsigned char *) bstr_ptr(b);
    size_t len = bstr_len(b);

    size_t i = 0;
    while (i < len) {
        data[i] = tolower(data[i]);
        i++;
    }

    return b;
}

bstr *bstr_dup_lower(const bstr *b) {
    return bstr_to_lowercase(bstr_dup(b));
}

int64_t bstr_util_mem_to_pint(const char *data, size_t len, int base, size_t *lastlen) {
    int64_t rval = 0, tval = 0, tflag = 0;
    size_t i = 0;

    *lastlen = i;

    for (i = 0; i < len; i++) {
        int d = data[i];

        *lastlen = i;

        // Convert character to digit.
        if ((d >= '0') && (d <= '9')) {
            d -= '0';
        } else if ((d >= 'a') && (d <= 'z')) {
            d -= 'a' - 10;
        } else if ((d >= 'A') && (d <= 'Z')) {
            d -= 'A' - 10;
        } else {
            d = -1;
        }

        // Check that the digit makes sense with the base
        // we are using.
        if ((d == -1) || (d >= base)) {
            if (tflag) {
                // Return what we have so far; lastlen points
                // to the first non-digit position.
                return rval;
            } else {
                // We didn't see a single digit.
                return -1;
            }
        }

        if (tflag) {
            rval *= base;

            if (tval > rval) {
                // Overflow
                return -2;
            }

            rval += d;

            if (tval > rval) {
                // Overflow
                return -2;
            }

            tval = rval;
        } else {
            tval = rval = d;
            tflag = 1;
        }
    }

    *lastlen = i + 1;

    return rval;
}

int bstr_index_of(const bstr *haystack, const bstr *needle) {
    return bstr_index_of_mem(haystack, bstr_ptr(needle), bstr_len(needle));
}

int bstr_index_of_c(const bstr *haystack, const char *needle) {
    return bstr_index_of_mem(haystack, needle, strlen(needle));
}

int bstr_index_of_nocase(const bstr *haystack, const bstr *needle) {
    return bstr_index_of_mem_nocase(haystack, bstr_ptr(needle), bstr_len(needle));
}

int bstr_index_of_c_nocase(const bstr *haystack, const char *needle) {
    return bstr_index_of_mem_nocase(haystack, needle, strlen(needle));
}

int bstr_index_of_mem(const bstr *haystack, const char *data2, size_t len2) {
    unsigned char *data = (unsigned char *) bstr_ptr(haystack);
    size_t len = bstr_len(haystack);
    size_t i, j;

    // TODO Is an optimisation here justified?
    //      http://en.wikipedia.org/wiki/Knuth-Morris-Pratt_algorithm

    for (i = 0; i < len; i++) {
        size_t k = i;

        for (j = 0; ((j < len2) && (k < len)); j++) {
            if (data[k++] != data2[j]) break;
        }

        if (j == len2) {
            return i;
        }
    }

    return -1;
}

int bstr_index_of_mem_nocase(const bstr *haystack, const char *data2, size_t len2) {
    unsigned char *data = (unsigned char *) bstr_ptr(haystack);
    size_t len = bstr_len(haystack);
    size_t i, j;

    // TODO No need to inspect the last len2 - 1 bytes
    for (i = 0; i < len; i++) {
        size_t k = i;

        for (j = 0; ((j < len2) && (k < len)); j++) {
            if (toupper(data[k++]) != toupper((unsigned char) data2[j])) break;
        }

        if ((k - i) == len2) {
            return i;
        }
    }

    return -1;
}

void bstr_chop(bstr *s) {
    bstr_t *b = (bstr_t *) s;
    if (b->len > 0) {
        b->len--;
    }
}

void bstr_util_adjust_len(bstr *s, size_t newlen) {
    bstr_t *b = (bstr_t *) s;
    b->len = newlen;
}

unsigned char bstr_char_at(const bstr *s, size_t pos) {
    unsigned char *data = (unsigned char *) bstr_ptr(s);
    size_t len = bstr_len(s);

    if (pos > len) return -1;
    return data[pos];
}

int bstr_begins_with_mem(const bstr *haystack, const char *data, size_t len) {
    char *hdata = bstr_ptr(haystack);
    size_t hlen = bstr_len(haystack);
    size_t pos = 0;

    while ((pos < len) && (pos < hlen)) {
        if (hdata[pos] != data[pos]) {
            return 0;
        }

        pos++;
    }

    if (pos == len) {
        return 1;
    } else {
        return 0;
    }
}

int bstr_begins_with_mem_nocase(const bstr *haystack, const char *data, size_t len) {
    char *hdata = bstr_ptr(haystack);
    size_t hlen = bstr_len(haystack);
    size_t pos = 0;

    while ((pos < len) && (pos < hlen)) {
        if (tolower((int) hdata[pos]) != tolower((int) data[pos])) {
            return 0;
        }

        pos++;
    }

    if (pos == len) {
        return 1;
    } else {
        return 0;
    }
}

int bstr_begins_with(const bstr *haystack, const bstr *needle) {
    return bstr_begins_with_mem(haystack, bstr_ptr(needle), bstr_len(needle));
}

int bstr_begins_with_c(const bstr *haystack, const char *needle) {
    return bstr_begins_with_mem(haystack, needle, strlen(needle));
}

int bstr_begins_with_nocase(const bstr *haystack, const bstr *needle) {
    return bstr_begins_with_mem_nocase(haystack, bstr_ptr(needle), bstr_len(needle));
}

int bstr_begins_withc_nocase(const bstr *haystack, const char *needle) {
    return bstr_begins_with_mem_nocase(haystack, needle, strlen(needle));
}

bstr *bstr_wrap_c(const char *input) {
    unsigned char *s = malloc(sizeof (bstr_t));
    if (s == NULL) return NULL;

    bstr_t *b = (bstr_t *) s;
    b->len = strlen(input);
    b->size = 0;
    b->ptr = (char *) input;

    return (bstr *) s;
}