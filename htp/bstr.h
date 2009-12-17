
#ifndef _BSTR_H
#define	_BSTR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// IMPORTANT This binary string library is used internally by the parser and you should
//           not rely on it in your code. The implementation may change at some point
//           in the future.
//
// TODO
//
//           - Add a function that wraps an existing data
//           - Support Unicode bstrings

typedef void * bstr;

bstr *bstr_alloc(size_t newsize);
void  bstr_free(bstr *s);
bstr *bstr_expand(bstr *s, size_t newsize);
bstr *bstr_cstrdup(unsigned char *);
bstr *bstr_memdup(unsigned char *data, size_t len);
bstr *bstr_strdup(bstr *b);
bstr *bstr_strdup_ex(bstr *b, size_t offset, size_t len);
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

void bstr_chop(bstr *b);
void bstr_len_adjust(bstr *s, size_t newlen);

unsigned char bstr_char_at(bstr *s, size_t pos);
 
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

