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

#ifndef _HTP_MULTIPART_H
#define	_HTP_MULTIPART_H

typedef struct htp_mpartp_t htp_mpartp_t;
typedef struct htp_mpart_part_t htp_mpart_part_t;

#include "bstr.h"
#include "dslib.h"
#include "htp.h"

#define MULTIPART_PART_UNKNOWN                  0
#define MULTIPART_PART_TEXT                     1
#define MULTIPART_PART_FILE                     2
#define MULTIPART_PART_PREAMBLE                 3
#define MULTIPART_PART_EPILOGUE                 4

#define MULTIPART_MODE_LINE                     0
#define MULTIPART_MODE_DATA                     1

#define MULTIPART_STATE_DATA                    1
#define MULTIPART_STATE_BOUNDARY                2
#define MULTIPART_STATE_BOUNDARY_IS_LAST1       3
#define MULTIPART_STATE_BOUNDARY_IS_LAST2       4
#define MULTIPART_STATE_BOUNDARY_EAT_LF         5

#define MULTIPART_DEFAULT_FILE_EXTRACT_LIMIT    16

#define HTP_MULTIPART_MIME_TYPE             "multipart/form-data"

#ifndef CR
#define CR '\r'
#endif

#ifndef LF
#define LF '\n'
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct htp_mpart_part_t {
    /** Pointer to the parser. */
    htp_mpartp_t *mpartp;

    /** Part type; see the MULTIPART_PART_* constants. */
    int type;   

    /** Raw part length. */
    size_t len;
   
    /** Part name, from the Content-Disposition header. */
    bstr *name;   

    /** Part value; currently only available for MULTIPART_PART_TEXT parts. */
    bstr *value;

    /** Part headers (htp_header_t instances), indexed by name. */
    table_t *headers;

    htp_file_t *file;
};

struct htp_mpartp_t {
    htp_connp_t *connp;

    /** Boundary to be used to extract parts. */
    char *boundary;

    /** Boundary length. */
    size_t boundary_len;
    
    /** How many boundaries were seen? */
    int boundary_count;

    /** Did we see the last boundary? */
    int seen_last_boundary;

    /** List of parts. */
    list_t *parts;

    int extract_files;
    int extract_limit;
    char *extract_dir;
    int file_count;

    // Parsing callbacks
    int (*handle_data)(htp_mpartp_t *mpartp, unsigned char *data, size_t len, int line_end);
    int (*handle_boundary)(htp_mpartp_t *mpartp);

    // Internal parsing fields
    // TODO Consider prefixing them with an underscore.
    int state;
    size_t bpos;
    unsigned char *current_data;
    htp_mpart_part_t *current_part;
    int current_mode;
    size_t current_len;
    bstr_builder_t *boundary_pieces;
    bstr_builder_t *part_pieces;
    int pieces_form_line;
    unsigned char first_boundary_byte;
    size_t boundarypos;
    int cr_aside;
};

htp_mpartp_t *htp_mpartp_create(htp_connp_t *connp, char *boundary);
void htp_mpartp_destroy(htp_mpartp_t **mpartp);

int htp_mpartp_parse(htp_mpartp_t *mpartp, unsigned char *data, size_t len);
int htp_mpartp_finalize(htp_mpartp_t *mpartp);

htp_mpart_part_t *htp_mpart_part_create(htp_mpartp_t *mpartp);
int htp_mpart_part_receive_data(htp_mpart_part_t *part, unsigned char *data, size_t len, int line);
int htp_mpart_part_finalize_data(htp_mpart_part_t *part);
void htp_mpart_part_destroy(htp_mpart_part_t *part);

int htp_mpartp_extract_boundary(bstr *content_type, char **boundary);

int htp_mpartp_run_request_file_data_hook(htp_mpart_part_t *part, unsigned char *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif	/* _HTP_MULTIPART_H */


