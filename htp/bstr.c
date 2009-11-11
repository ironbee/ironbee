
#include "bstr.h"
#include <ctype.h>

/**
 *
 */
bstr *bstr_alloc(size_t len) {
    unsigned char *s = malloc(sizeof (bstr_t) + len);
    if (s == NULL) return NULL;

    bstr_t *b = (bstr_t *) s;
    b->len = 0;
    b->size = len;
    b->ptr = NULL;

    return (bstr *) s;
}

/**
 *
 */
bstr *bstr_add_str(bstr *s, bstr *s2) {
    return bstr_add_mem(s, bstr_ptr(s2), bstr_len(s2));
}

/**
 *
 */
bstr *bstr_add_cstr(bstr *s, char *str) {
    return bstr_add_mem(s, str, strlen(str));
}

/**
 *
 */
bstr *bstr_add_mem(bstr *s, unsigned char *data, size_t len) {
    if (bstr_size(s) < bstr_len(s) + len) {
        s = bstr_expand(s, bstr_len(s) + len);
        if (s == NULL) return NULL;
    }

    bstr_t *b = (bstr_t *) s;
    memcpy(bstr_ptr(s) + b->len, data, len);
    b->len = b->len + len;

    return s;
}

/**
 *
 */
bstr *bstr_expand(bstr *s, size_t newsize) {
    if (((bstr_t *) s)->ptr != NULL) {
        ((bstr_t *) s)->ptr = realloc(((bstr_t *) s)->ptr, newsize);
    } else {
        s = realloc(s, sizeof (bstr_t) + newsize);
    }

    ((bstr_t *) s)->size = newsize;

    return s;
}

/**
 *
 */
bstr *bstr_cstrdup(unsigned char *data) {
    return bstr_memdup(data, strlen(data));
}

/**
 *
 */
bstr *bstr_memdup(unsigned char *data, size_t len) {
    bstr *b = bstr_alloc(len);
    if (b == NULL) return NULL;
    memcpy(bstr_ptr(b), data, len);
    ((bstr_t *) b)->len = len;
    return b;
}

/**
 *
 */
bstr *bstr_strdup(bstr *b) {
    return bstr_strdup_ex(b, 0, bstr_len(b));
}

/**
 *
 */
bstr *bstr_strdup_ex(bstr *b, size_t offset, size_t len) {
    bstr *bnew = bstr_alloc(len);
    if (bnew == NULL) return NULL;
    memcpy(bstr_ptr(bnew), bstr_ptr(b) + offset, len);
    ((bstr_t *) bnew)->len = len;
    return bnew;
}

/**
 *
 */
char *bstr_memtocstr(unsigned char *data, size_t len) {
    // Count how many NUL bytes we have in the string.
    int i, nulls = 0;
    for (i = 0; i < len; i++) {
        if (data[i] == '\0') {
            nulls++;
        }
    }

    // Now copy the string into a NUL-terminated buffer.
    char *r, *t;
    r = t = malloc(len + nulls + 1);
    while (len--) {
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

/**
 *
 */
char *bstr_tocstr(bstr *b) {
    if (b == NULL) return NULL;
    return bstr_memtocstr(bstr_ptr(b), bstr_len(b));
}

/**
 *
 */
int bstr_chr(bstr *b, int c) {
    char *data = bstr_ptr(b);
    size_t len = bstr_len(b);

    int i = 0;
    while (i < len) {
        if (data[i] == c) {
            return i;
        }

        i++;
    }

    return -1;
}

/**
 *
 */
int bstr_rchr(bstr *b, int c) {
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

/**
 *
 */
int bstr_cmp_ex(unsigned char *s1, size_t l1, unsigned char *s2, size_t l2) {
    size_t p1 = 0, p2 = 0;

    // TODO Not tested properly

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

/**
 *
 */
int bstr_cmpc(bstr *b, char *c) {
    return bstr_cmp_ex(bstr_ptr(b), bstr_len(b), c, strlen(c));
}

/**
 *
 */
int bstr_cmp(bstr *b1, bstr *b2) {
    return bstr_cmp_ex(bstr_ptr(b1), bstr_len(b1), bstr_ptr(b2), bstr_len(b2));
}

/**
 *
 */
bstr *bstr_tolowercase(bstr *b) {
    if (b == NULL) return NULL;

    char *data = bstr_ptr(b);
    size_t len = bstr_len(b);

    int i = 0;
    while (i < len) {
        data[i] = tolower(data[i]);
        i++;
    }

    return b;
}

/**
 *
 */
bstr *bstr_dup_lower(bstr *b) {
    return bstr_tolowercase(bstr_strdup(b));
}

/**
 *
 */
int bstr_util_memtoip(char *data, size_t len, int base, size_t *lastlen) {
    int rval = 0, tval = 0, tflag = 0;

    int i = *lastlen = 0;
    for (i = 0; i < len; i++) {
        unsigned int d = data[i];

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

int bstr_indexof(bstr *haystack, bstr *needle) {
    return bstr_indexofmem(haystack, bstr_ptr(needle), bstr_len(needle));
}

int bstr_indexofc(bstr *haystack, char *needle) {
    return bstr_indexofmem(haystack, needle, strlen(needle));
}

int bstr_indexof_nocase(bstr *haystack, bstr *needle) {
    return bstr_indexofmem_nocase(haystack, bstr_ptr(needle), bstr_len(needle));
}

int bstr_indexofc_nocase(bstr *haystack, char *needle) {
    return bstr_indexofmem_nocase(haystack, needle, strlen(needle));
}

int bstr_indexofmem(bstr *haystack, char *data2, size_t len2) {
    char *data = bstr_ptr(haystack);
    size_t len = bstr_len(haystack);
    int i, j;

    // TODO Is an optimisation here justified?
    //      http://en.wikipedia.org/wiki/Knuth-Morris-Pratt_algorithm

    // TODO No need to inspect the last len2 - 1 bytes
    for (i = 0; i < len; i++) {
        int k = i;

        for (j = 0; ((j < len2) && (k < len)); j++) {
            if (data[k++] != data2[j]) break;
        }

        if ((k - i) == len2) {
            return i;
        }
    }

    return -1;
}

int bstr_indexofmem_nocase(bstr *haystack, char *data2, size_t len2) {
    char *data = bstr_ptr(haystack);
    size_t len = bstr_len(haystack);
    int i, j;

    // TODO No need to inspect the last len2 - 1 bytes
    for (i = 0; i < len; i++) {
        int k = i;

        for (j = 0; ((j < len2) && (k < len)); j++) {
            if (toupper(data[k++]) != toupper(data2[j])) break;
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

void bstr_len_adjust(bstr *s, size_t newlen) {
    bstr_t *b = (bstr_t *) s;
    b->len = newlen;
}

unsigned char bstr_char_at(bstr *s, size_t pos) {
    unsigned char *data = bstr_ptr(s);
    size_t len = bstr_len(s);

    if (pos > len) return -1;
    return data[pos];
}

