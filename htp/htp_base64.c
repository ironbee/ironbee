/*
 * LibHTP (http://www.libhtp.org)
 * Copyright 2009,2010 Ivan Ristic <ivanr@webkreator.com>
 *
 * LibHTP is an open source product, released under terms of the General Public Licence
 * version 2 (GPLv2). Please refer to the file LICENSE, which contains the complete text
 * of the license.
 *
 * In addition, there is a special exception that allows LibHTP to be freely
 * used with any OSI-approved open source licence. Please refer to the file
 * LIBHTP_LICENSING_EXCEPTION for the full text of the exception.
 *
 */

/* Adapted from the libb64 project (http://sourceforge.net/projects/libb64), which is in public domain. */

#include "htp_base64.h"
#include "bstr.h"

int htp_base64_decode_single(char value_in) {
    static const char decoding[] = {62, -1, -1, -1, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,
        -1, -1, -1, -2, -1, -1, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17,
        18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1, -1, 26, 27, 28, 29, 30, 31, 32, 33, 34,
        35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51};
    static const char decoding_size = sizeof (decoding);

    value_in -= 43;

    if (value_in < 0 || value_in > decoding_size) return -1;

    return decoding[(int) value_in];
}

void htp_base64_decoder_init(htp_base64_decoder* decoder) {
    decoder->step = step_a;
    decoder->plainchar = 0;
}

int htp_base64_decode(htp_base64_decoder* decoder, const char* code_in, const int length_in,
    char* plaintext_out, const int length_out) {
    const char* codechar = code_in;
    char* plainchar = plaintext_out;
    char fragment;

    if (length_out <= 0) return 0;

    *plainchar = decoder->plainchar;

    switch (decoder->step) {
            while (1) {
                case step_a:
                do {
                    if (codechar == code_in + length_in) {
                        decoder->step = step_a;
                        decoder->plainchar = *plainchar;
                        return plainchar - plaintext_out;
                    }
                    fragment = (char) htp_base64_decode_single(*codechar++);
                } while (fragment < 0);
                *plainchar = (fragment & 0x03f) << 2;

                case step_b:
                do {
                    if (codechar == code_in + length_in) {
                        decoder->step = step_b;
                        decoder->plainchar = *plainchar;
                        return plainchar - plaintext_out;
                    }
                    fragment = (char) htp_base64_decode_single(*codechar++);
                } while (fragment < 0);
                *plainchar++ |= (fragment & 0x030) >> 4;
                *plainchar = (fragment & 0x00f) << 4;                
                if (--length_out == 0) {
                    return plainchar - plaintext_out;
                }

                case step_c:
                do {
                    if (codechar == code_in + length_in) {
                        decoder->step = step_c;
                        decoder->plainchar = *plainchar;
                        return plainchar - plaintext_out;
                    }
                    fragment = (char) htp_base64_decode_single(*codechar++);
                } while (fragment < 0);
                *plainchar++ |= (fragment & 0x03c) >> 2;
                *plainchar = (fragment & 0x003) << 6;
                if (--length_out == 0) {
                    return plainchar - plaintext_out;
                }

                case step_d:
                do {
                    if (codechar == code_in + length_in) {
                        decoder->step = step_d;
                        decoder->plainchar = *plainchar;
                        return plainchar - plaintext_out;
                    }
                    fragment = (char) htp_base64_decode_single(*codechar++);
                } while (fragment < 0);
                *plainchar++ |= (fragment & 0x03f);
                if (--length_out == 0) {
                    return plainchar - plaintext_out;
                }
            }
    }

    /* control should not reach here */
    return plainchar - plaintext_out;
}

bstr *htp_base64_decode_bstr(bstr *input) {
    return htp_base64_decode_mem(bstr_ptr(input), bstr_len(input));
}

bstr *htp_base64_decode_mem(char *data, size_t len) {
    htp_base64_decoder decoder;
    bstr *r = NULL;

    htp_base64_decoder_init(&decoder);

    char *tmpstr = malloc(len);
    if (tmpstr == NULL) return NULL;

    int resulting_len = htp_base64_decode(&decoder, data, len, tmpstr);
    if (resulting_len > 0) {
        r = bstr_memdup(tmpstr, resulting_len);
    }

    free(tmpstr);

    return r;
}