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

#ifndef _HTP_BASE64_H
#define	_HTP_BASE64_H

#include "bstr.h"

typedef enum {
    step_a, step_b, step_c, step_d
} htp_base64_decodestep;

typedef struct {
    htp_base64_decodestep step;
    char plainchar;
} htp_base64_decoder;

void htp_base64_decoder_init(htp_base64_decoder* state_in);

int htp_base64_decode_single(char value_in);

int htp_base64_decode(htp_base64_decoder* decoder, const char* code_in, const int length_in, char* plaintext_out);

bstr *htp_base64_decode_bstr(bstr *input);
bstr *htp_base64_decode_mem(char *data, size_t len);

#endif	/* _HTP_BASE64_H */

