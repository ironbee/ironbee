/***************************************************************************
 * Copyright (c) 2009-2010 Open Information Security Foundation
 * Copyright (c) 2010-2013 Qualys, Inc.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.

 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.

 * - Neither the name of the Qualys, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
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

#ifndef _HTP_MULTIPART_H
#define	_HTP_MULTIPART_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct htp_mpartp_t htp_mpartp_t;
typedef struct htp_mpart_part_t htp_mpart_part_t;

#include "bstr.h"
#include "htp.h"
#include "htp_table.h"

#define HTP_FILE_MULTIPART                      1
#define HTP_FILE_PUT                            2

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

#define HTP_MULTIPART_MIME_TYPE                 "multipart/form-data"

struct htp_mpart_part_t {
    /** Pointer to the parser that created this part. */
    htp_mpartp_t *mpartp;

    /** Part type; see the MULTIPART_PART_* constants. */
    int type;   

    /** Raw part length. */
    size_t len;
   
    /** Part name, from the Content-Disposition header. */
    bstr *name;   

    /** Part value; currently available only for MULTIPART_PART_TEXT parts. */
    bstr *value;

    /** Part headers (htp_header_t instances), indexed by name. */
    htp_table_t *headers;

    /** Further file data, available only for MULTIPART_PART_FILE parts. */
    htp_file_t *file;
};

struct htp_mpartp_t {
    htp_cfg_t *cfg;

    /** Multipart boundary. */
    char *boundary;

    /** Boundary length. */
    size_t boundary_len;
    
    /** How many boundaries were seen? */
    int boundary_count;

    /** Did we see the last boundary? */
    int seen_last_boundary;

    /** List of parts. */
    htp_list_t *parts;

    int extract_files;
    int extract_limit;
    char *extract_dir;
    int file_count;

    // Parsing callbacks
    int (*handle_data)(htp_mpartp_t *mpartp, const unsigned char *data, size_t len, int line_end);
    int (*handle_boundary)(htp_mpartp_t *mpartp);

    // Internal parsing fields; move into a private structure

    /**
     * Parser state; one of MULTIPART_STATE_* constants.
     */
    int parser_state;

    /**
     * Keeps track of the current position in the boundary matching progress.
     * When this field reaches boundary_len, we have a boundary match.
     */
    size_t boundary_match_pos;

    /**
     * Pointer to the part that is currently being processed.
     */
    htp_mpart_part_t *current_part;

    /**
     * This parser consists of two layers: the outer layer is charged with
     * finding parts, and the internal layer handles part data. There is an
     * interesting interaction between the two parsers. Because the
     * outer layer is seeing every line (it has to, in order to test for
     * boundaries), it also effectively also splits input into lines. The
     * inner parser deals with two areas: first is the headers, which are
     * line based, followed by binary data. When parsing headers, the inner
     * parser can reuse the lines identified by the outer parser. In this
     * variable we keep the current parsing mode of the part, which helps
     * us process input data more efficiently. The possible values are
     * MULTIPART_MODE_LINE and MULTIPART_MODE_DATA.
     */
    int current_part_mode;

    /**
     * Used for buffering when a potential boundary is fragmented
     * across many input data buffers. On a match, the data stored here is
     * discarded. When there is no match, the buffer is processed as data
     * (belonging to the currently active part).
     */
    bstr_builder_t *boundary_pieces;

    /**
     * Stores text part pieces until the entire part is seen, at which
     * point the pieces are assembled into a single buffer, and the
     * builder cleared.
     */
    bstr_builder_t *part_data_pieces;

    /**
     * Whenever a new line is encountered, the parser needs to examine it
     * in order to determine if it contains a boundary. While the examination
     * is taking place, the parser will store the first byte of the new
     * line in this structure, which comes handy during the processing of
     * part headers, in order to efficiently determine if the header is folded.
     */
    unsigned char next_line_first_byte;

    /**
     * The offset of the current boundary candidate, relative to the most
     * recent data chunk (first unprocessed chunk of data).
     */
    size_t boundary_candidate_pos;

    /**
     * When we encounter a CR as the last byte in a buffer, we don't know
     * if the byte is part of a CRLF combination. If it is, then the CR
     * might be a part of a boundary. But if it is not, it's current
     * part's data. Because we know how to handle everything before the
     * CR, we do, and we use this flag to indicate that a CR byte is
     * effectively being buffered. This is probably a case of premature
     * optimization, but I am going to leave it in for now.
     */
    int cr_aside;

    /**
     * When set, indicates that this parser no longer owns names and
     * values of MULTIPART_PART_TEXT parts. It is used to avoid data
     * duplication when the parser is used by LibHTP internally.
     */
    int gave_up_data;
};

htp_mpartp_t *htp_mpartp_create(htp_cfg_t *cfg, char *boundary);

void htp_mpartp_destroy(htp_mpartp_t **mpartp);

// TODO Associate with the parser instance.
htp_status_t htp_mpartp_extract_boundary(bstr *content_type, char **boundary);

htp_status_t htp_mpartp_finalize(htp_mpartp_t *mpartp);

htp_status_t htp_mpartp_parse(htp_mpartp_t *mpartp, const unsigned char *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif	/* _HTP_MULTIPART_H */
