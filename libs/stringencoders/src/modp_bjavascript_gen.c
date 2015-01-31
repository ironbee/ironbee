/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
/* vi: set expandtab shiftwidth=4 tabstop=4: */

#include "arraytoc.h"

void hexencodemap()
{
    static const char sHexChars[] = "0123456789ABCDEF";
    int i;
    char hexEncode1[256];
    char hexEncode2[256];
    for (i = 0; i < 256; ++i) {
        hexEncode1[i] = sHexChars[i >> 4];
        hexEncode2[i] = sHexChars[i & 0x0f];
    }

    char_array_to_c(hexEncode1, 256, "gsHexEncodeMap1");
    char_array_to_c(hexEncode2, 256, "gsHexEncodeMap2");
}

void jsencodemap()
{
    int i;
    char jsEncodeMap[256];

    // set everything to "as is"
    for (i = 0; i < 256; ++i) {
        jsEncodeMap[i] = 0;
    }

    // chars that need hex escaping
    for (i = 0; i < 32; ++i) {
        jsEncodeMap[i] = 'A';
    }
    for (i = 127; i < 256; ++i) {
        jsEncodeMap[i] = 'A';
    }

    // items that have special escaping
    jsEncodeMap[0x08] = 'b';
    jsEncodeMap[0x09] = 't';
    jsEncodeMap[0x0a] = 'n';
    jsEncodeMap[0x0b] = 'v';
    jsEncodeMap[0x0c] = 'f';
    jsEncodeMap[0x0d] = 'r';
    jsEncodeMap[0x5c] = '\\';  /* blackslash gets escaped */
    jsEncodeMap[0x22] = '"';   /* dquote gets escaped */
    jsEncodeMap[0x27] = '\'';  /* squote gets escaped */

    char_array_to_c(jsEncodeMap, 256, "gsJavascriptEncodeMap");
};

int main()
{
    jsencodemap();
    hexencodemap();
    return 0;
}
