
#ifndef _BSTR_H
#define	_BSTR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void * bstr;

bstr *bstr_alloc(size_t newsize);
// bstr *bstr_alloc_p(size_t newsize, pool_t *pool);
bstr *bstr_expand(bstr *s, size_t newsize);
// bstr *bstr_expand_p(bstr *s, size_t newsize, pool_t *pool);
bstr *bstr_cstrdup(unsigned char *);
// bstr *bstr_cstrdup_p(unsigned char *, pool_t *pool);
bstr *bstr_memdup(unsigned char *data, size_t len);
//bstr *bstr_memdup_p(unsigned char *data, size_t len, pool_t *pool);
bstr *bstr_strdup(bstr *b);
// bstr *bstr_strdup_p(bstr *b, pool_t *pool);
bstr *bstr_strdup_ex(bstr *b, size_t offset, size_t len);
//bstr *bstr_strdup_ex_p(bstr *b, size_t offset, size_t len, pool_t *pool);
char *bstr_tocstr(bstr *);

int bstr_chr(bstr *, int);
int bstr_rchr(bstr *, int);

int bstr_cmpc(bstr *, char *);
int bstr_cmp(bstr *, bstr *);

bstr *bstr_dup_lower(bstr *);
bstr *bstr_tolowercase(bstr *);

bstr *bstr_add_mem(bstr *, unsigned char *, size_t);
bstr *bstr_add_str(bstr *, bstr *);
bstr *bstr_add_cstr(bstr *, char *);

int bstr_util_memtoip(char *data, size_t len, int base, size_t *lastlen);
char *bstr_memtocstr(unsigned char *data, size_t len);

int bstr_indexof(bstr *haystack, bstr *needle);
int bstr_indexofc(bstr *haystack, char *needle);
int bstr_indexof_nocase(bstr *haystack, bstr *needle);
int bstr_indexofc_nocase(bstr *haystack, char *needle);
int bstr_indexofmem(bstr *haystack, char *data, size_t len);
int bstr_indexofmem_nocase(bstr *haystack, char *data, size_t len);

/*
 * Function candidates:
 *
 * - alloc from a pool
 * - compare
 * - compare binary string to a block of memory
 * - compareNoCase
 * - startsWithNoCase
 * - endsWithNoCase
 * - convert a binary string into a C string (e.g., escape NUL with \0)
 * - substring
 * - trim
 * - trimLeft
 * - trimRight
 * - concatenate
 * - replace character with character
 * - find character (first, last)
 * - find needle in a haystack
 * - find needle starting with index
 * - lowercase
 * - uppercase
 * - tokenize string
 * - convert string to number
 * - sprintf into a string
 *
 * Unicode:
 *
 * - get nth characters from an Unicode string (since bytes are not characters
 *   any more)
 * - I guess most of the above functions will have to be rewritten for
 *   Unicode
 *
 * Open issues and notes:
 *
 * - For the functions that accept two strings (e.g., comparison), we might
 *   want to allow one parameter to be a binary string and the other a C
 *   string. This is only safe if the second string is controlled by the
 *   programmer (e.g., it's static). This is useful in the cases where you
 *   want to look for something but you don't want to create a binary string
 *   out of it.
 * - Functions should be implemented to accept two sets of pointers and
 *   lengths, which would make them useful evenn in the cases where bstr is
 *   not used, and if we decide to change the implementation.
 * - Should we NUL-terminate all strings anyway?
 * - Implement binary strings that grow as needed, like ByteArrayOutputStream
 *   in Java.
 * - The current implementation makes wrapping (of available data) impossible.
 *   Perhaps we should use a proper structure, with pointers. Allocation can
 *   still be optimised in the way it is now (one allocation per string instead
 *   of two).
 * 
 */

typedef struct bstr_t bstr_t;

struct bstr_t {
    size_t len;
    size_t size;
    unsigned char *ptr;
};

#define bstr_len(X) ((*(bstr_t *)(X)).len)
#define bstr_size(X) ((*(bstr_t *)(X)).size)
#define bstr_ptr(X) ( ((*(bstr_t *)(X)).ptr == NULL) ? ((void *)(X) + sizeof(bstr_t)) : (*(bstr_t *)(X)).ptr )

#endif	/* _BSTR_H */

