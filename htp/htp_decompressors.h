/***************************************************************************
 * Copyright (c) 2009-2010, Open Information Security Foundation
 * Copyright (c) 2009-2012, Qualys, Inc.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * * Neither the name of the Qualys, Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ***************************************************************************/

/**
 * @file
 * @author Ivan Ristic <ivanr@webkreator.com>
 */

#ifndef _HTP_DECOMPRESSORS_H
#define	_HTP_DECOMPRESSORS_H

typedef struct htp_decompressor_gzip_t htp_decompressor_gzip_t;
typedef struct htp_decompressor_t htp_decompressor_t;

#include "zlib.h"

#define GZIP_BUF_SIZE       8192
#define GZIP_WINDOW_SIZE    -15

#define DEFLATE_MAGIC_1     0x1f
#define DEFLATE_MAGIC_2     0x8b

#define COMPRESSION_NONE        0
#define COMPRESSION_GZIP        1
#define COMPRESSION_DEFLATE     2

#ifdef __cplusplus
extern "C" {
#endif

struct htp_decompressor_t {
    int (*decompress)(htp_decompressor_t *, htp_tx_data_t *);
    int (*callback)(htp_tx_data_t *);
    void (*destroy)(htp_decompressor_t *);
};

struct htp_decompressor_gzip_t {
    htp_decompressor_t super;
    int initialized;
    int zlib_initialized;
    uint8_t header[10];
    uint8_t header_len;
    z_stream stream;
    unsigned char *buffer;
    unsigned long crc;    
};

htp_decompressor_t * htp_gzip_decompressor_create(htp_connp_t *connp, int format);

#ifdef __cplusplus
}
#endif

#endif	/* _HTP_DECOMPRESSORS_H */

