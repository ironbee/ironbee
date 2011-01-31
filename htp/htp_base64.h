/*****************************************************************************
 * Licensed to Qualys, Inc. (QUALYS) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * QUALYS licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

/* Adapted from the libb64 project (http://sourceforge.net/projects/libb64), which is in public domain. */

#ifndef _HTP_BASE64_H
#define	_HTP_BASE64_H

#include "bstr.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    step_a, step_b, step_c, step_d
} htp_base64_decodestep;

typedef struct {
    htp_base64_decodestep step;
    char plainchar;
} htp_base64_decoder;

void htp_base64_decoder_init(htp_base64_decoder* state_in);

int htp_base64_decode_single(char value_in);

int htp_base64_decode(htp_base64_decoder* decoder, const char* code_in, const int length_in,
    char* plaintext_out, const int length_out);

bstr *htp_base64_decode_bstr(bstr *input);
bstr *htp_base64_decode_mem(char *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif	/* _HTP_BASE64_H */

