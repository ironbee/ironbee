/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
/* vi: set expandtab shiftwidth=4 tabstop=4: */

/**
 * B64FAST - High performance base64 encoder/decoder
 * Version 1.1 -- 20-Feb-2005
 *
 * Copyright 2005, 2006 Nick Galbreath -- nickg [at] modp [dot] com
 * All rights reserved.
 *
 * http://modp.com/release/base64
 *
 * Released under bsd license.  See b64fast.c for details.
 *
 * Data table generator.  This generates a ".h" file for use
 * in compiling b64fast.c.  This does not need to be exported.
 *
 */


/****************************/
/* To change the alphabet   */
/* Edit the following lines */
/* and do a 'make'          */
/****************************/

/****************************/


#include "arraytoc.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static unsigned char b64chars[64] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
    'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
    'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'
} ;

static unsigned char padchar = '=';

void printStart()
{
    printf("#include <stdint.h>\n");
    printf("#define CHAR62 '%c'\n", b64chars[62]);
    printf("#define CHAR63 '%c'\n", b64chars[63]);
    printf("#define CHARPAD '%c'\n", padchar);
}

void clearDecodeTable(uint32_t* ary)
{
    int i = 0;
    for (i = 0; i < 256; ++i) {
        ary[i] = 0x01FFFFFF;
    }
}

int main(int argc, char** argv)
{
    // over-ride standard alphabet
    if (argc == 2) {
        unsigned char* replacements = (unsigned char*) argv[1];
        if (strlen((char*)replacements) != 3) {
            fprintf(stderr, "input must be a string of 3 characters '-', '.' or '_'\n");
            exit(1);
        }
        fprintf(stderr, "fusing '%s' as replacements in base64 encoding\n", replacements);
        b64chars[62] = replacements[0];
        b64chars[63] = replacements[1];
        padchar = replacements[2];
    }

    uint32_t x;
    uint32_t i = 0;
    char cary[256];
    uint32_t ary[256];

    printStart();

    for (i = 0; i < 256; ++i) {
        cary[i] = (char)(b64chars[i >> 2 & 0x3f]);
    }
    char_array_to_c(cary, 256, "e0");

    for (i = 0; i < 256; ++i) {
        cary[i] = (char) b64chars[(i & 0x3F)]; }
    char_array_to_c(cary, 256, "e1");

    for (i = 0; i < 256; ++i) { cary[i] = (char) b64chars[(i & 0x3F)]; }
    char_array_to_c(cary, 256, "e2");


    printf("\n\n#ifdef WORDS_BIGENDIAN\n");
    printf("\n\n/* SPECIAL DECODE TABLES FOR BIG ENDIAN (IBM/MOTOROLA/SUN) CPUS */\n\n");


    clearDecodeTable(ary);
    for (i = 0; i < 64; ++i) {
        x = b64chars[i];
        ary[x] = i << 18;
    }
    uint32_array_to_c_hex(ary, 256,"d0");
    printf("\n\n");

    clearDecodeTable(ary);
    for (i = 0; i < 64; ++i) {
        x = b64chars[i];
        ary[x] = i << 12;
    }
    uint32_array_to_c_hex(ary, 256, "d1");
    printf("\n\n");

    clearDecodeTable(ary);
    for (i = 0; i < 64; ++i) {
        x = b64chars[i];
        ary[x] = i << 6;
    }
    uint32_array_to_c_hex(ary, 256, "d2");
    printf("\n\n");

    clearDecodeTable(ary);
    for (i = 0; i < 64; ++i) {
        x = b64chars[i];
        ary[x] = i;
    }
    uint32_array_to_c_hex(ary, 256, "d3");
    printf("\n\n");

    printf("#else\n");

    printf("\n\n/* SPECIAL DECODE TABLES FOR LITTLE ENDIAN (INTEL) CPUS */\n\n");

    clearDecodeTable(ary);
    for (i = 0; i < 64; ++i) {
        x = b64chars[i];
        ary[x] = i << 2;
    }
    uint32_array_to_c_hex(ary, 256,"d0");
    printf("\n\n");

    clearDecodeTable(ary);
    for (i = 0; i < 64; ++i) {
        x = b64chars[i];
        ary[x] = ((i & 0x30) >> 4) | ((i & 0x0F) << 12);
    }
    uint32_array_to_c_hex(ary, 256, "d1");
    printf("\n\n");

    clearDecodeTable(ary);
    for (i = 0; i < 64; ++i) {
        x = b64chars[i];
        ary[x] = ((i & 0x03) << 22) | ((i & 0x3c) << 6);
    }
    uint32_array_to_c_hex(ary, 256, "d2");
    printf("\n\n");

    clearDecodeTable(ary);
    for (i = 0; i < 64; ++i) {
        x = b64chars[i];
        ary[x] = i << 16;
    }
    uint32_array_to_c_hex(ary, 256, "d3");
    printf("\n\n");


    printf("#endif\n");

    return 0;
}
