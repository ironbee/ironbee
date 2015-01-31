/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
/* vi: set expandtab shiftwidth=4 tabstop=4: */

#include "modp_ascii.h"
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include "modp_ascii_data.h"
#include <ctype.h>

//extern const char gsToUpperMap[256];
/**
 * This is standard clib implementation of uppercasing a string.
 * It has an unfair advantage since it's inside the test file
 * so the optimizer can inline it.
 */
void toupper_copy1(char* dest, const char* str, int len)
{
    int i;
    for (i = 0; i < len; ++i) {
        // toupper is defined in <ctype.h>
        *dest++ = toupper(str[i]);
    }
    *dest = 0;
}


/**
 * Skipping ctype, and doing the compare directly
 *
 */
void toupper_copy2(char* dest, const char* str, int len)
{
    int i;
    char c;
    for (i = 0; i < len; ++i) {
        c = str[i];
        *dest++ = (c >= 'a' && c <= 'z') ? c : (c -32);
    }
    *dest = 0;
}

/** 
 * Sequential table lookup
 */
void toupper_copy3(char* dest, const char* str, int len)
{
    int i;
    unsigned char c;
    for (i = 0; i < len; ++i) {
        c = str[i];
        *dest++ = gsToUpperMap[c];
    }
    *dest = 0;
}

/** \brief toupper Version 4 -- parallel table lookup
 *
 *
 */
void toupper_copy4(char* dest, const char* str, int len)
{
    /*
     *  int i;
     *  for (i = 0; i < len; ++i) {
     *      char c = str[i];
     *      *dest++ = (c >= 'a' && c <= 'z') ? c - 32 : c;
     *  }
     */

    int i;
    uint8_t c1,c2,c3,c4;

    const int leftover = len % 4;
    const int imax = len - leftover;
    const uint8_t* s = (const uint8_t*) str;
    for (i = 0; i != imax ; i+=4) {
        /*
         *  it's important to make these variables
         *  it helps the optimizer to figure out what to do
         */
        c1 = s[i], c2=s[i+1], c3=s[i+2], c4=s[i+3];
        dest[0] = gsToUpperMap[c1];
        dest[1] = gsToUpperMap[c2];
        dest[2] = gsToUpperMap[c3];
        dest[3] = gsToUpperMap[c4];
        dest += 4;
    }

    switch (leftover) {
        case 3: *dest++ = gsToUpperMap[s[i++]];
        case 2: *dest++ = gsToUpperMap[s[i++]];
        case 1: *dest++ = gsToUpperMap[s[i]];
        case 0: *dest = '\0';
    }
}

/** \brief toupper Versions 5 -- hsieh alternate
 * Based code from Paul Hsieh
 * http://www.azillionmonkeys.com/qed/asmexample.html
 *
 * This was his "improved" version, but it appears to either run just
 * as fast, or a bit slower than his original version
 */
void toupper_copy5(char* dest, const char* str, int len)
{
    int i;
    uint32_t eax,ebx,ecx,edx;
    const uint8_t* ustr = (const uint8_t*) str;
    const int leftover = len % 4;
    const int imax = len / 4;
    const uint32_t* s = (const uint32_t*) str;
    uint32_t* d = (uint32_t*) dest;
    for (i = 0; i != imax; ++i) {
        eax = s[i];
        ebx = 0x80808080ul | eax;
        ecx = ebx - 0x61616161ul;
        edx = ~(ebx - 0x7b7b7b7bul);
        ebx = (ecx & edx) & (~eax & 0x80808080ul);
        *d++ = eax - (ebx >> 2);
    }

    i = imax*4;
    dest = (char*) d;
    switch (leftover) {
        case 3: *dest++ = gsToUpperMap[ustr[i++]];
        case 2: *dest++ = gsToUpperMap[ustr[i++]];
        case 1: *dest++ = gsToUpperMap[ustr[i]];
        case 0: *dest = '\0';
    }
}

/** \brief ToUpper Version 6 -- Hsieh original, ASM style
 * Based code from Paul Hsieh
 * http://www.azillionmonkeys.com/qed/asmexample.html
 *
 * This is almost a direct port of the original ASM code, on some
 * platforms/compilers it does run faster then the "de-asm'ed" version
 * used in the modp library.
 *
 */
void toupper_copy6(char* dest, const char* str, int len)
{
    int i=0;
    uint32_t eax,ebx,ecx,edx;
    const uint8_t* ustr = (const uint8_t*) str;
    const int leftover = len % 4;
    const int imax = len / 4;
    const uint32_t* s = (const uint32_t*) str;
    uint32_t* d = (uint32_t*) dest;
    for (i = 0; i != imax; ++i) {
#if 1
        /*
         * as close to original asm code as possible
         */
        eax = s[i];
        ebx = 0x7f7f7f7f;
        edx = 0x7f7f7f7f;
        ebx = ebx & eax;
        ebx = ebx + 0x05050505;
        ecx = eax;
        ecx = ~ecx;
        ebx = ebx & edx;
        ebx = ebx + 0x1a1a1a1a;
        ebx = ebx & ecx;
        ebx = ebx >> 2;
        ebx = ebx & 0x20202020;
        eax = eax - ebx;
        *d++ = eax;
#else
        /*
         * "de-asm'ed" version, this is what is used in the modp library
         */
        eax = s[i];
        ebx = (0x7f7f7f7ful & eax) + 0x05050505ul;
        ebx = (0x7f7f7f7ful & ebx) + 0x1a1a1a1aul;
        ebx = ((ebx & ~eax) >> 2 ) & 0x20202020ul;
        *d++ = eax - ebx;
#endif
    }

    i = imax*4;
    dest = (char*) d;
    switch (leftover) {
        case 3: *dest++ = gsToUpperMap[ustr[i++]];
        case 2: *dest++ = gsToUpperMap[ustr[i++]];
        case 1: *dest++ = gsToUpperMap[ustr[i]];
        case 0: *dest = '\0';
    }
}

void modp_toupper_copy_a2(char* dest, const char* str, int len)
{
    int i=0;
    uint32_t eax, ebx;
    const uint8_t* ustr = (const uint8_t*) str;
    const int leftover = len % 4;
    const int imax = len / 4;
    const uint32_t* s = (const uint32_t*) str;
    uint32_t* d = (uint32_t*) dest;
    for (i = 0; i != imax; ++i) {
        eax = s[i];

        ebx = (0x7f7f7f7ful & eax) + 0x05050505ul;
        ebx = (0x7f7f7f7ful & ebx) + 0x1a1a1a1aul;
        ebx = ((ebx & ~eax) >> 2 ) & 0x20202020ul;
        *d++ = eax - ebx;
    }

    i = imax*4;
    dest = (char*) d;
    switch (leftover) {
        case 3: *dest++ = gsToUpperMap[ustr[i++]];
        case 2: *dest++ = gsToUpperMap[ustr[i++]];
        case 1: *dest++ = gsToUpperMap[ustr[i]];
        case 0: *dest = '\0';
    }
}

int main()
{
    double last = 0.0;
    size_t i = 0;
    char buf[256];
    char obuf[300];

    for (i = 0; i < 256; ++i) {
        buf[i] = (char)i;
    }

    uint32_t max = 1000000;
    clock_t t0, t1;
    printf("%s", "type\tclib\tdirect\tmap\tpara\thsieh1\thsieh2\tFinal\timprovement\n");

    printf("toupper\t");
    fflush(stdout);

    /**
     ** V1
     **/
    t0 = clock();
    for (i = 0; i < max; ++i) {
        toupper_copy1(obuf, buf, sizeof(buf));
    }
    t1 = clock();
    last = t1 -t0;
    printf("%lu\t", (t1-t0));
    fflush(stdout);

    /**
     ** V2
     **/
    t0 = clock();
    for (i = 0; i < max; ++i) {
        toupper_copy2(obuf, buf, sizeof(buf));
    }
    t1 = clock();
    printf("%lu\t", (t1-t0));
    fflush(stdout);

    /**
     ** V3
     **/
    t0 = clock();
    for (i = 0; i < max; ++i) {
        toupper_copy3(obuf, buf, sizeof(buf));
    }
    t1 = clock();
    printf("%lu\t", (t1-t0));
    fflush(stdout);

    /**
     ** V4 -- Parallel Table Lookup
     **/
    t0 = clock();
    for (i = 0; i < max; ++i) {
        toupper_copy4(obuf, buf, sizeof(buf));
    }
    t1 = clock();
    printf("%lu\t", (t1-t0));
    fflush(stdout);


    /**
     ** V5 -- Hsieh Alternate
     **/
    t0 = clock();
    for (i = 0; i < max; ++i) {
        toupper_copy5(obuf, buf, sizeof(buf));
    }
    t1 = clock();
    printf("%lu\t", (t1-t0));
    fflush(stdout);

    /**
     ** HSEIH -- asm style
     **/
    t0 = clock();
    for (i = 0; i < max; ++i) {
        toupper_copy6(obuf, buf, sizeof(buf));
    }
    t1 = clock();
    printf("%lu\t", (t1-t0));
    fflush(stdout);

    /**
     ** MODP FINAL
     **/
    t0 = clock();
    for (i = 0; i < max; ++i) {
        modp_toupper_copy(obuf, buf, sizeof(buf));
    }
    t1 = clock();

    printf("%lu\t", (t1-t0));
    fflush(stdout);

    printf("%.1fx\n", last/(t1-t0));
    fflush(stdout);

    return 0;
}
