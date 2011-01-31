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

#ifndef _HTP_DECOMPRESSORS_H
#define	_HTP_DECOMPRESSORS_H

typedef struct htp_decompressor_gzip_t htp_decompressor_gzip_t;
typedef struct htp_decompressor_t htp_decompressor_t;

#include "htp.h"
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

