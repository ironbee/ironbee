/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
/* vi: set expandtab shiftwidth=4 tabstop=4: */
#include <stdio.h>
#include "arraytoc.h"

void hexencodemap_char()
{
    static const unsigned char sHexChars[] = "0123456789ABCDEF";
    int i;
    unsigned char e1[256];
    unsigned char e2[256];

    for (i = 0;i < 256; ++i) {
        e1[i] = 0;
        e2[i] = 0;
    }

    for (i = 0; i < 256; ++i) {
        e1[i] = sHexChars[i >> 4];
        e2[i] = sHexChars[i &  0x0f];
    }

    char_array_to_c((char*)e1, 256, "gsHexEncodeC1");
    char_array_to_c((char*)e2, 256, "gsHexEncodeC2");
}


// exact same thing as one used on urlencode
void hexdecodemap()
{
    uint32_t i;
    uint32_t map1[256];
    uint32_t map2[256];
    for (i = 0; i < 256; ++i) {
        map1[i] = 256;
        map2[i] = 256;
    }

    // digits
    for (i = '0'; i <= '9'; ++i) {
        map1[i] = i - '0';
        map2[i] = map1[i] << 4;
    }

    // upper
    for (i = 'A'; i <= 'F'; ++i) {
        map1[i] = i - 'A' + 10;
        map2[i] = map1[i] << 4;
    }

    // lower
    for (i = 'a'; i <= 'f'; ++i) {
        map1[i] = i - 'a' + 10;
        map2[i] = map1[i] << 4;
    }


    uint32_array_to_c(map1, 256, "gsHexDecodeMap");

    uint32_array_to_c(map2, 256, "gsHexDecodeD2");
}

int main()
{
    hexencodemap_char();

    hexdecodemap();
    return 0;
}
