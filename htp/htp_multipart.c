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

#include <ctype.h>
#include <unistd.h>

#include "htp.h"
#include "htp_multipart.h"
#include "htp_multipart_private.h"

/**
 * Determines the type of a Content-Disposition parameter.
 *
 * @param[in] data
 * @param[in] startpos
 * @param[in] pos
 * @return CD_PARAM_OTHER, CD_PARAM_NAME or CD_PARAM_FILENAME.
 */
static int htp_mpartp_cd_param_type(unsigned char *data, size_t startpos, size_t pos) {
    if ((pos - startpos) == 4) {
        if (memcmp(data + startpos, "name", 4) == 0) return CD_PARAM_NAME;
    } else if ((pos - startpos) == 8) {
        if (memcmp(data + startpos, "filename", 8) == 0) return CD_PARAM_FILENAME;
    }

    return CD_PARAM_OTHER;
}

/**
 * Returns the main multipart structure produced by the parser.
 *
 * @param[in] parser
 * @return Multipart instance.
 */
htp_multipart_t *htp_mpartp_get_multipart(htp_mpartp_t *parser) {
	return &parser->multipart;
}

/**
 * Parses the Content-Disposition part header.
 *
 * @param[in] part
 * @return HTP_OK on success, HTP_ERROR on failure.
 */
static htp_status_t htp_mpart_part_parse_c_d(htp_multipart_part_t *part) {
    // Find C-D header
    htp_header_t *h = (htp_header_t *) htp_table_get_c(part->headers, "content-disposition");
    if (h == NULL) return HTP_DECLINED;

    if (bstr_index_of_c(h->value, "form-data") != 0) {
        return -1;
    }

    // The parsing starts here
    unsigned char *data = (unsigned char *) bstr_ptr(h->value);
    size_t len = bstr_len(h->value);
    size_t pos = 9; // Start after "form-data"

    // Main parameter parsing loop (once per parameter)
    while (pos < len) {
        // Find semicolon and go over it
        while ((pos < len) && ((data[pos] == '\t') || (data[pos] == ' '))) pos++;
        if (pos == len) return -2;

        // Semicolon
        if (data[pos] != ';') return -3;
        pos++;

        // Go over the whitespace before parameter name
        while ((pos < len) && ((data[pos] == '\t') || (data[pos] == ' '))) pos++;
        if (pos == len) return -4;

        // Found starting position (name)
        size_t start = pos;

        // Look for ending position
        while ((pos < len) && (data[pos] != '\t') && (data[pos] != ' ') && (data[pos] != '=')) pos++;
        if (pos == len) return -5;

        // Ending position is in "pos" now

        // Is it a parameter we are interested in?
        int param_type = htp_mpartp_cd_param_type(data, start, pos);

        // Ignore whitespace
        while ((pos < len) && ((data[pos] == '\t') || (data[pos] == ' '))) pos++;
        if (pos == len) return -6;

        // Equals
        if (data[pos] != '=') return -7;
        pos++;

        // Go over the whitespace before value
        while ((pos < len) && ((data[pos] == '\t') || (data[pos] == ' '))) pos++;
        if (pos == len) return -8;

        // Found starting point (value)
        start = pos;

        // Quoting char indicator
        int qchar = -1;

        // Different handling for quoted and bare strings
        if (data[start] == '"') {
            // Quoted string
            qchar = data[start];
            start = ++pos;

            // Find the end of the value
            while ((pos < len) && (data[pos] != qchar)) {
                if (data[pos] == '\\') {
                    // Ignore invalid quoting pairs
                    if (pos + 1 < len) return -9;
                    // Go over the quoted character
                    pos++;
                }

                pos++;
            }
        } else {
            // Bare string
            while ((pos < len) && (!htp_is_token(data[pos]))) pos++;
        }

        switch (param_type) {
            case CD_PARAM_NAME:
                // TODO Unquote quoted characters
                part->name = bstr_dup_mem(data + start, pos - start);
                if (part->name == NULL) return -1;
                break;
            case CD_PARAM_FILENAME:
                // TODO Unquote quoted characters
                part->file = calloc(1, sizeof (htp_file_t));
                if (part->file == NULL) return -1;
                part->file->fd = -1;
                part->file->filename = bstr_dup_mem(data + start, pos - start);
                if (part->file->filename == NULL) return -1;
                part->file->source = HTP_FILE_MULTIPART;
                break;
            default:
                // Ignore unknown parameter
                // TODO Warn/log?
                break;
        }

        // Skip over the quoting character
        if (qchar != -1) {
            pos++;
        }

        // Continue to parse the next parameter, if any
    }

    return HTP_OK;
}

/**
 * Parses the Content-Type part header.
 *
 * @param[in] part
 * @return HTP_OK on success, HTP_ERROR on failure.
 */
static htp_status_t htp_mpart_part_parse_c_t(htp_multipart_part_t *part) {
    // Find C-D header
    htp_header_t *h = (htp_header_t *) htp_table_get_c(part->headers, "content-type");
    if (h == NULL) return HTP_DECLINED;

    part->content_type = h->value;

    return HTP_OK;
}

/**
 * Processes part headers.
 *
 * @param[in] part
 * @return HTP_OK on success, HTP_ERROR on failure.
 */
htp_status_t htp_mpart_part_process_headers(htp_multipart_part_t *part) {
    htp_mpart_part_parse_c_d(part);
    htp_mpart_part_parse_c_t(part);
    return HTP_OK;
}

/**
 * Parses one part header.
 *
 * @param[in] data
 * @param[in] len
 * @param[in] HTP_OK on success, HTP_ERROR on failure.
 */
htp_status_t htp_mpartp_parse_header(htp_multipart_part_t *part, const unsigned char *data, size_t len) {
    size_t name_start, name_end;
    size_t value_start, value_end;

    name_start = 0;

    // Look for the colon
    size_t colon_pos = 0;

    while ((colon_pos < len) && (data[colon_pos] != ':')) colon_pos++;

    if (colon_pos == len) {
        // Missing colon
        // TODO Error message
        return HTP_ERROR;
    }

    if (colon_pos == 0) {
        // Empty header name
        // TODO Error message
    }

    name_end = colon_pos;

    // Ignore LWS after field-name
    size_t prev = name_end;
    while ((prev > name_start) && (htp_is_lws(data[prev - 1]))) {
        prev--;
        name_end--;

        // LWS after field name
        // TODO Error message
    }

    // Value

    value_start = colon_pos;

    // Go over the colon
    if (value_start < len) {
        value_start++;
    }

    // Ignore LWS before field-content
    while ((value_start < len) && (htp_is_lws(data[value_start]))) {
        value_start++;
    }

    // Look for the end of field-content
    value_end = value_start;

    while (value_end < len) value_end++;

    // Ignore LWS after field-content
    prev = value_end - 1;
    while ((prev > value_start) && (htp_is_lws(data[prev]))) {
        prev--;
        value_end--;
    }

    // Check that the header name is a token
    size_t i = name_start;
    while (i < name_end) {
        if (!htp_is_token(data[i])) {
            // Request field is not a token
            // TODO Error message

            break;
        }

        i++;
    }

    // Now extract the name and the value
    htp_header_t *h = calloc(1, sizeof (htp_header_t));
    if (h == NULL) return HTP_ERROR;

    h->name = bstr_dup_mem(data + name_start, name_end - name_start);
    h->value = bstr_dup_mem(data + value_start, value_end - value_start);

    // Check if the header already exists
    htp_header_t * h_existing = htp_table_get(part->headers, h->name);
    if (h_existing != NULL) {
        // Add to existing header
        bstr *new_value = bstr_expand(h_existing->value, bstr_len(h_existing->value)
                + 2 + bstr_len(h->value));
        if (new_value == NULL) {
            bstr_free(&h->name);
            bstr_free(&h->value);
            free(h);
            return HTP_ERROR;
        }

        h_existing->value = new_value;
        bstr_add_mem_noex(h_existing->value, (unsigned char *) ", ", 2);
        bstr_add_noex(h_existing->value, h->value);

        // The header is no longer needed
        bstr_free(&h->name);
        bstr_free(&h->value);
        free(h);

        // Keep track of same-name headers
        h_existing->flags |= HTP_FIELD_REPEATED;
    } else {
        // Add as a new header
        htp_table_add(part->headers, h->name, h);
    }

    return HTP_OK;
}

/**
 * Creates a new multipart part.
 *
 * @param[in] mpartp
 */
htp_multipart_part_t *htp_mpart_part_create(htp_mpartp_t *parser) {
    htp_multipart_part_t * part = calloc(1, sizeof (htp_multipart_part_t));
    if (part == NULL) return NULL;

    part->headers = htp_table_create(4);
    if (part->headers == NULL) {
        free(part);
        return NULL;
    }

    part->parser = parser;
    bstr_builder_clear(parser->part_data_pieces);

    return part;
}

/**
 * Destroys multipart part.
 *
 * @param[in] part
 */
void htp_mpart_part_destroy(htp_multipart_part_t *part, int gave_up_data) {
    if (part == NULL) return;

    if (part->file != NULL) {
        bstr_free(&part->file->filename);

        if (part->file->tmpname != NULL) {
            unlink(part->file->tmpname);
            free(part->file->tmpname);
        }

        free(part->file);
        part->file = NULL;
    }

    if ((!gave_up_data) || (part->type != MULTIPART_PART_TEXT)) {
        bstr_free(&part->name);
        bstr_free(&part->value);
    }

    // Content-Type is currently only an alias for the
    // value stored in the headers structure.

    if (part->headers != NULL) {
        // Destroy request_headers
        htp_header_t *h = NULL;
        for (int i = 0, n = htp_table_size(part->headers); i < n; i++) {
            htp_table_get_index(part->headers, i, NULL, (void **) &h);
            bstr_free(&h->name);
            bstr_free(&h->value);
            free(h);
        }

        htp_table_destroy(&part->headers);
    }

    free(part);
}

/**
 * Finalizes part processing.
 *
 * @param[in] part
 * @return HTP_OK on success, HTP_ERROR on failure.
 */
htp_status_t htp_mpart_part_finalize_data(htp_multipart_part_t *part) {
    // We currently do not process or store the preamble part.
    if (part->type == MULTIPART_PART_PREAMBLE) return HTP_OK;

    // If we have seen the last boundary, and this part does not have
    // a name, then it is probably a genuine epilogue part. Otherwise,
    // it might be an evasion attempt.
    if (part->parser->multipart.flags & HTP_MULTIPART_SEEN_LAST_BOUNDARY) {
        if (part->type == MULTIPART_PART_UNKNOWN) {
            part->parser->current_part->type = MULTIPART_PART_EPILOGUE;
            part->parser->multipart.flags |= HTP_MULTIPART_HAS_EPILOGUE;
            return HTP_OK;
        } else {
            part->parser->multipart.flags |= HTP_MULTIPART_PART_AFTER_LAST_BOUNDARY;
            // Continue to set part value.
        }       
    }

    if ((part->type == MULTIPART_PART_TEXT)||(part->type == MULTIPART_PART_UNKNOWN)) {
        if (bstr_builder_size(part->parser->part_data_pieces) > 0) {
            part->value = bstr_builder_to_str(part->parser->part_data_pieces);
            bstr_builder_clear(part->parser->part_data_pieces);
        }
    } else if (part->type == MULTIPART_PART_FILE) {
        htp_mpartp_run_request_file_data_hook(part, NULL, 0);

        if (part->file->fd != -1) {
            close(part->file->fd);
        }
    }

    return HTP_OK;
}

htp_status_t htp_mpartp_run_request_file_data_hook(htp_multipart_part_t *part, const unsigned char *data, size_t len) {
    if (part->parser->cfg == NULL) return HTP_OK;

    // Keep track of the file length.
    part->file->len += len;

    // Package data for the callbacks.
    htp_file_data_t file_data;
    file_data.file = part->file;
    file_data.data = data;
    file_data.len = (const size_t)len;

    // Send data to callbacks
    htp_status_t rc = htp_hook_run_all(part->parser->cfg->hook_request_file_data, &file_data);
    if (rc != HTP_OK) return rc;

    return HTP_OK;
}

/**
 * Handles part data.
 *
 * @param[in] part
 * @param[in] data
 * @param[in] len
 * @param[in] is_line
 * @return HTP_OK on success, HTP_ERROR on failure.
 */
htp_status_t htp_mpart_part_handle_data(htp_multipart_part_t *part, const unsigned char *data, size_t len, int is_line) {
    #if HTP_DEBUG
    fprint_raw_data(stderr, "htp_mpart_part_handle_data: data chunk", (unsigned char *) data, len);
    fprintf(stderr, "Part type: %d\n", part->type);
    #endif   

    // Keep track of part length
    part->len += len;

    // We currently do not process or store the preamble and epilogue parts.
    if ((part->type == MULTIPART_PART_PREAMBLE) || (part->type == MULTIPART_PART_EPILOGUE)) {
        return HTP_OK;
    }

    if (part->parser->current_part_mode == MODE_LINE) {
        // Line mode

        // TODO Remove the extra characters from folded lines

        if (is_line) {
            // End of line

            // Ignore the line ending
            if (len > 1) {
                if (data[len - 1] == LF) len--;
                if (data[len - 1] == CR) len--;
            } else if (len > 0) {
                if (data[len - 1] == LF) len--;
            }

            // Is it an empty line?
            if ((len == 0) && ((bstr_builder_size(part->parser->part_data_pieces) == 0))) {
                // Empty line; process headers and switch to data mode.               
                
                htp_mpart_part_process_headers(part);
                // TODO RC
                
                part->parser->current_part_mode = MODE_DATA;               

                if (part->file != NULL) {                    
                    part->type = MULTIPART_PART_FILE;

                    if ((part->parser->extract_files) && (part->parser->file_count < part->parser->extract_limit)) {
                        char buf[255];
                        strncpy(buf, part->parser->extract_dir, 254);
                        strncat(buf, "/libhtp-multipart-file-XXXXXX", 254 - strlen(buf));
                        part->file->tmpname = strdup(buf);
                        if (part->file->tmpname == NULL) return HTP_ERROR;
                        part->file->fd = mkstemp(part->file->tmpname);
                        if (part->file->fd < 0) return HTP_ERROR;

                        part->parser->file_count++;
                    }
                }
                else if (part->name != NULL) {
                    part->type = MULTIPART_PART_TEXT;
                } else {
                    // The type stays MULTIPART_PART_UNKNOWN.
                }
            } else {
                // Not an empty line

                // Is there a folded line coming after this one?
                if ((part->parser->next_line_first_byte != ' ') && (part->parser->next_line_first_byte != '\t')) {
                    // No folded lines after this one, so process header

                    // Do we have more than once piece?
                    if (bstr_builder_size(part->parser->part_data_pieces) > 0) {
                        // Line in pieces

                        bstr_builder_append_mem(part->parser->part_data_pieces, data, len);

                        bstr *line = bstr_builder_to_str(part->parser->part_data_pieces);
                        if (line == NULL) return HTP_ERROR;
                        htp_mpartp_parse_header(part, (unsigned char *) bstr_ptr(line), bstr_len(line));
                        // TODO RC
                        bstr_free(&line);

                        bstr_builder_clear(part->parser->part_data_pieces);
                    } else {
                        // Just this line
                        htp_mpartp_parse_header(part, data, len);
                        // TODO RC
                    }
                } else {
                    // Folded line, just store this piece for later
                    bstr_builder_append_mem(part->parser->part_data_pieces, data, len);
                }
            }
        } else {
            // Not end of line; keep the data chunk for later
            bstr_builder_append_mem(part->parser->part_data_pieces, data, len);
        }
    } else {
        // Data mode; keep the data chunk for later (but not if it is a file)
        switch (part->type) {
            case MULTIPART_PART_TEXT:
            case MULTIPART_PART_UNKNOWN:
                bstr_builder_append_mem(part->parser->part_data_pieces, data, len);
                break;
            case MULTIPART_PART_FILE:
                htp_mpartp_run_request_file_data_hook(part, data, len);

                // Store data to disk
                // TODO Make blocking I/O optional.
                if (part->file->fd != -1) {
                    if (write(part->file->fd, data, len) < 0) {
                        return HTP_ERROR;
                    }
                }
                break;
            default :
                // Internal error.
                return HTP_ERROR;
                break;
        }
    }

    return HTP_OK;
}

/**
 * Handles data, creating new parts as necessary.
 *
 * @param[in] mpartp
 * @param[in] data
 * @param[in] len
 * @param[in] is_line
 * @return HTP_OK on success, HTP_ERROR on failure.
 */
static htp_status_t htp_mpartp_handle_data(htp_mpartp_t *parser, const unsigned char *data, size_t len, int is_line) {
    if (len == 0) return HTP_OK;

    // Do we have a part already?
    if (parser->current_part == NULL) {
        // Create a new part.
        parser->current_part = htp_mpart_part_create(parser);
        if (parser->current_part == NULL) return HTP_ERROR;

        if (parser->multipart.boundary_count == 0) {
            // We haven't seen a boundary yet, so this must be the preamble part.
            parser->current_part->type = MULTIPART_PART_PREAMBLE;
            parser->multipart.flags |= HTP_MULTIPART_HAS_PREAMBLE;
            parser->current_part_mode = MODE_DATA;
        } else {
            // Part after preamble.
            parser->current_part_mode = MODE_LINE;
        }

        // Add part to the list.        
        htp_list_push(parser->multipart.parts, parser->current_part);

        #ifdef HTP_DEBUG
        fprintf(stderr, "Created new part type %d\n", parser->current_part->type);
        #endif
    }

    // Send data to the part.
    return htp_mpart_part_handle_data(parser->current_part, data, len, is_line);
}

/**
 * Handles a boundary event, which means that it will finalize a part
 * if one exists.
 *
 * @param[in] mpartp
 * @return HTP_OK on success, HTP_ERROR on failure.
 */
static htp_status_t htp_mpartp_handle_boundary(htp_mpartp_t *parser) {
    #if HTP_DEBUG
    fprintf(stderr, "htp_mpartp_handle_boundary\n");
    #endif   

    if (parser->current_part != NULL) {
        if (htp_mpart_part_finalize_data(parser->current_part) != HTP_OK) {
            return HTP_ERROR;
        }

        // We're done with this part
        parser->current_part = NULL;

        // Revert to line mode
        parser->current_part_mode = MODE_LINE;
    }

    return HTP_OK;
}

htp_mpartp_t *htp_mpartp_create(htp_cfg_t *cfg) {
    if (cfg == NULL )
        return NULL ;

    htp_mpartp_t *parser = calloc(1, sizeof(htp_mpartp_t));
    if (parser == NULL )
        return NULL ;

    parser->cfg = cfg;

    parser->boundary_pieces = bstr_builder_create();
    if (parser->boundary_pieces == NULL ) {
        htp_mpartp_destroy(&parser);
        return NULL ;
    }

    parser->part_data_pieces = bstr_builder_create();
    if (parser->part_data_pieces == NULL ) {
        htp_mpartp_destroy(&parser);
        return NULL ;
    }

    parser->multipart.parts = htp_list_create(64);
    if (parser->multipart.parts == NULL ) {
        htp_mpartp_destroy(&parser);
        return NULL ;
    }

    parser->parser_state = STATE_INIT;    
    parser->extract_limit = DEFAULT_FILE_EXTRACT_LIMIT;
    parser->handle_data = htp_mpartp_handle_data;
    parser->handle_boundary = htp_mpartp_handle_boundary;

    return parser;
}

static htp_status_t _htp_mpartp_init_boundary(htp_mpartp_t *parser, unsigned char *data, size_t len) {
    if ((parser == NULL )||(data == NULL))return HTP_ERROR;

    // Copy the boundary and convert it to lowercase

    parser->multipart.boundary_len = len + 4;
    parser->multipart.boundary = malloc(parser->multipart.boundary_len + 1);
    if (parser->multipart.boundary == NULL) return HTP_ERROR;

    parser->multipart.boundary[0] = CR;
    parser->multipart.boundary[1] = LF;
    parser->multipart.boundary[2] = '-';
    parser->multipart.boundary[3] = '-';

    for (size_t i = 0; i < len; i++) {
        parser->multipart.boundary[i + 4] = tolower(data[i]);
    }

    parser->multipart.boundary[parser->multipart.boundary_len] = '\0';

    // We're starting in boundary-matching mode, where the
    // initial CRLF is not needed.
    parser->parser_state = STATE_BOUNDARY;
    parser->boundary_match_pos = 2;

    return HTP_OK;
}

htp_status_t htp_mpartp_init_boundary(htp_mpartp_t *parser, bstr *c_t_header) {
    bstr *boundary = NULL;
    htp_status_t rc = htp_mpartp_extract_boundary(c_t_header, &boundary);
    if (rc != HTP_OK) return rc;

    rc = _htp_mpartp_init_boundary(parser, bstr_ptr(boundary), bstr_len(boundary));

    bstr_free(&boundary);

    return rc;
}

htp_status_t htp_mpartp_init_boundary_ex(htp_mpartp_t *parser, char *boundary) {
    return _htp_mpartp_init_boundary(parser, (unsigned char *) boundary, strlen(boundary));
}

void htp_mpartp_destroy(htp_mpartp_t ** _parser) {
    if ((_parser == NULL) || (*_parser == NULL)) return;
    htp_mpartp_t * parser = *_parser;

    if (parser->multipart.boundary != NULL) {
        free(parser->multipart.boundary);
    }

    bstr_builder_destroy(parser->part_data_pieces);
    bstr_builder_destroy(parser->boundary_pieces);

    // Free parts
    if (parser->multipart.parts != NULL) {
        for (int i = 0, n = htp_list_size(parser->multipart.parts); i < n; i++) {
            htp_multipart_part_t * part = htp_list_get(parser->multipart.parts, i);
            htp_mpart_part_destroy(part, parser->gave_up_data);
        }

        htp_list_destroy(&parser->multipart.parts);
    }

    free(parser);

    *_parser = NULL;
}

/**
 * Processes set-aside data.
 *
 * @param[in] mpartp
 * @param[in] data
 * @param[in] pos
 * @param[in] startpos
 * @param[in] return_pos
 * @param[in] matched
 * @return HTP_OK on success, HTP_ERROR on failure.
 */
static htp_status_t htp_martp_process_aside(htp_mpartp_t *parser, int matched) {
    // The stored data pieces can contain up to one line. If we're in data mode and there
    // was no boundary match, things are straightforward -- we process everything as data.
    // If there was a match, we need to take care to not send the line ending as data, nor
    // anything that follows (because it's going to be a part of the boundary). Similarly,
    // when we are in line mode, we need to split the first data chunk, processing the first
    // part as line and the second part as data.

    // Do we need to do any chunk splitting?
    if (matched || (parser->current_part_mode == MODE_LINE)) {
        // Line mode or boundary match

        // In line mode, we don't care about the set-aside CR byte.
        parser->cr_aside = 0;

        // We know that we went to match a boundary because
        // we saw a new line. Now we have to find that line and
        // process it. It's either going to be in the current chunk,
        // or in the first stored chunk.
        if (bstr_builder_size(parser->boundary_pieces) > 0) {
            int first = 1;
            for (int i = 0, n = htp_list_size(parser->boundary_pieces->pieces); i < n; i++) {
                bstr *b = htp_list_get(parser->boundary_pieces->pieces, i);

                if (first) {
                    first = 0;

                    // Split the first chunk.

                    if (!matched) {
                        // In line mode, we are OK with line endings.
                        parser->handle_data(parser, bstr_ptr(b), parser->boundary_candidate_pos, /* line */ 1);
                    } else {
                        // But if there was a match, the line ending belongs to the boundary.
                        unsigned char *dx = bstr_ptr(b);
                        size_t lx = parser->boundary_candidate_pos;

                        // Remove LF or CRLF.
                        if ((lx > 0) && (dx[lx - 1] == LF)) {
                            lx--;
                            // Remove CR.
                            if ((lx > 0) && (dx[lx - 1] == CR)) {
                                lx--;
                            }
                        }

                        parser->handle_data(parser, dx, lx, /* not a line */ 0);
                    }

                    // The second part of the split chunks belongs to the boundary
                    // when matched, data otherwise.
                    if (!matched) {
                        parser->handle_data(parser, bstr_ptr(b) + parser->boundary_candidate_pos,
                                bstr_len(b) - parser->boundary_candidate_pos, /* not a line */ 0);
                    }                   
                } else {
                    // Do not send data if there was a boundary match. The stored
                    // data belongs to the boundary.
                    if (!matched) {
                        parser->handle_data(parser, (unsigned char *) bstr_ptr(b), bstr_len(b), /* not a line */ 0);
                    }
                }
            }

            bstr_builder_clear(parser->boundary_pieces);
        }
    } else {
        // Data mode and no match.       

        // In data mode, we process the lone CR byte as data.
        if (parser->cr_aside) {
            parser->handle_data(parser, (unsigned char *) &"\r", 1, /* not a line */ 0);
            parser->cr_aside = 0;
        }

        // We then process any pieces that we might have stored, also as data.
        if (bstr_builder_size(parser->boundary_pieces) > 0) {
            for (int i = 0, n = htp_list_size(parser->boundary_pieces->pieces); i < n; i++) {
                bstr *b = htp_list_get(parser->boundary_pieces->pieces, i);
                parser->handle_data(parser, bstr_ptr(b), bstr_len(b), /* not a line */ 0);
            }

            bstr_builder_clear(parser->boundary_pieces);
        }
    }

    return HTP_OK;
}


htp_status_t htp_mpartp_finalize(htp_mpartp_t *parser) {
    if (parser->current_part != NULL) {
        htp_martp_process_aside(parser, 0);

        if (htp_mpart_part_finalize_data(parser->current_part) != HTP_OK) return HTP_ERROR; // TODO RC
    }

    bstr_builder_clear(parser->boundary_pieces);

    return HTP_OK;
}

htp_status_t htp_mpartp_parse(htp_mpartp_t *parser, const unsigned char *data, size_t len) {
    // The current position in the entire input buffer.
    size_t pos = 0;

    // The position of the first unprocessed byte of data. We split the
    // input buffer into smaller chunks, according to their purpose. Once
    // an entire such smaller chunk is processed, we move to the next
    // and update startpos.
    size_t startpos = 0;

    // The position of the (possible) boundary. We investigate for possible
    // boundaries whenever we encounter CRLF or just LF. If we don't find a
    // boundary we need to go back, and this is what data_return_pos helps with.
    size_t data_return_pos = 0;

    #if HTP_DEBUG
    fprint_raw_data(stderr, "htp_mpartp_parse: data chunk", (unsigned char *) data, len);
    #endif

    // While there's data in the input buffer.

    while (pos < len) {

STATE_SWITCH:
        #if HTP_DEBUG        
        fprintf(stderr, "htp_mpartp_parse: state %d pos %d startpos %d\n", parser->parser_state, pos, startpos);
        #endif

        switch (parser->parser_state) {

            case STATE_INIT:
                // Incomplete initialization.
                return HTP_ERROR;
                break;

            case STATE_DATA: // Handle part data.
                
                // While there's data in the input buffer.

                while (pos < len) {
                    // Check for a CRLF-terminated line.
                    if (data[pos] == CR) {
                        // We have a CR byte.

                        // Is this CR the last byte in the input buffer?
                        if (pos + 1 == len) {
                            // We have CR as the last byte in input. We are going to process
                            // what we have in the buffer as data, except for the CR byte,
                            // which we're going to leave for later. If it happens that a
                            // CR is followed by a LF and then a boundary, the CR is going
                            // to be discarded.
                            pos++; // Advance over CR.
                            parser->cr_aside = 1;
                        } else {
                            // We have CR and at least one more byte in the buffer, so we
                            // are able to test for the LF byte too.
                            if (data[pos + 1] == LF) {
                                pos += 2; // Advance over CR and LF.

                                parser->multipart.flags |= HTP_MULTIPART_CRLF_LINE;

                                // Prepare to switch to boundary testing.
                                data_return_pos = pos;
                                parser->boundary_candidate_pos = pos - startpos;
                                parser->boundary_match_pos = 2; // After LF; position of the first dash.
                                parser->parser_state = STATE_BOUNDARY;

                                goto STATE_SWITCH;
                            } else {
                                // This is not a new line; advance over the
                                // byte and clear the CR set-aside flag.
                                pos++;
                                parser->cr_aside = 0;
                            }
                        }
                    } else if (data[pos] == LF) { // Check for a LF-terminated line.
                        pos++; // Advance over LF.

                        // Did we have a CR in the previous input chunk?
                        if (parser->cr_aside == 0) {
                            parser->multipart.flags |= HTP_MULTIPART_LF_LINE;
                        } else {
                            parser->multipart.flags |= HTP_MULTIPART_CRLF_LINE;
                        }
                        
                        // Prepare to switch to boundary testing.
                        data_return_pos = pos;
                        parser->boundary_candidate_pos = pos - startpos;
                        parser->boundary_match_pos = 2; // After LF; position of the first dash.
                        parser->parser_state = STATE_BOUNDARY;

                        goto STATE_SWITCH;
                    } else {
                        // Take one byte from input
                        pos++;

                        // Earlier we might have set aside a CR byte not knowing if the next
                        // byte is a LF. Now we know that it is not, and so we can release the CR.
                        if (parser->cr_aside) {
                            parser->handle_data(parser, (unsigned char *) &"\r", 1, /* not a line */ 0);
                            parser->cr_aside = 0;
                        }
                    }
                } // while               

                // No more data in the input buffer; process the data chunk.
                parser->handle_data(parser, data + startpos, pos - startpos - parser->cr_aside, /* not a line */ 0);

                break;

            case STATE_BOUNDARY: // Handle a possible boundary.
                while (pos < len) {
                    #ifdef HTP_DEBUG
                    fprintf(stderr, "boundary (len %d pos %d char %d) data char %d\n", parser->multipart.boundary_len,
                            parser->boundary_match_pos, parser->multipart.boundary[parser->boundary_match_pos], tolower(data[pos]));
                    #endif

                    // Remember the first byte in the new line; we'll need to
                    // determine if the line is a part of a folded header.
                    if (parser->boundary_match_pos == 2) {
                        parser->next_line_first_byte = data[pos];
                    }

                    // Check if the bytes match.
                    if (!(tolower((int) data[pos]) == parser->multipart.boundary[parser->boundary_match_pos])) {
                        // Boundary mismatch.

                        // Process stored (buffered) data.
                        htp_martp_process_aside(parser, /* no match */ 0);

                        // Return back where data parsing left off.
                        if (parser->current_part_mode == MODE_LINE) {
                            // In line mode, we process the line.
                            parser->handle_data(parser, data + startpos, data_return_pos - startpos, /* line */ 1);
                            startpos = data_return_pos;
                        } else {
                            // In data mode, we go back where we left off.
                            pos = data_return_pos;
                        }

                        parser->parser_state = STATE_DATA;

                        goto STATE_SWITCH;
                    }

                    // Consume one matched boundary byte
                    pos++;
                    parser->boundary_match_pos++;

                    // Have we seen all boundary bytes?
                    if (parser->boundary_match_pos == parser->multipart.boundary_len) {
                        // Boundary match!

                        // Process stored (buffered) data.
                        htp_martp_process_aside(parser, /* boundary match */ 1);

                        // Process data prior to the boundary in the current input buffer.
                        // Because we know this is the last chunk before boundary, we can
                        // remove the line endings.
                        size_t dlen = data_return_pos - startpos;
                        if ((dlen > 0) && (data[startpos + dlen - 1] == LF)) dlen--;
                        if ((dlen > 0) && (data[startpos + dlen - 1] == CR)) dlen--;
                        parser->handle_data(parser, data + startpos, dlen, /* line */ 1);

                        // Keep track of how many boundaries we've seen.
                        parser->multipart.boundary_count++;

                        if (parser->multipart.flags & HTP_MULTIPART_SEEN_LAST_BOUNDARY) {
                            parser->multipart.flags |= HTP_MULTIPART_PART_AFTER_LAST_BOUNDARY;
                        }

                        // Run boundary match.
                        parser->handle_boundary(parser);

                        // We now need to check if this is the last boundary in the payload
                        parser->parser_state = STATE_BOUNDARY_IS_LAST2;

                        goto STATE_SWITCH;
                    }
                } // while

                // No more data in the input buffer; store (buffer) the unprocessed
                // part for later, for after we find out if this is a boundary.
                bstr_builder_append_mem(parser->boundary_pieces, data + startpos, len - startpos);

                break;
                
            case STATE_BOUNDARY_IS_LAST2:
                // Examine the first byte after the last boundary character. If it is
                // a dash, then we maybe processing the last boundary in the payload. If
                // it is not, move to eat all bytes until the end of the line.

                if (data[pos] == '-') {
                    // Found one dash, now go to check the next position.
                    pos++;
                    parser->parser_state = STATE_BOUNDARY_IS_LAST1;
                } else {
                    // This is not the last boundary. Change state but
                    // do not advance the position, allowing the next
                    // state to process the byte.
                    parser->parser_state = STATE_BOUNDARY_EAT_LWS;
                }
                break;               

            case STATE_BOUNDARY_IS_LAST1:
                // Examine the byte after the first dash; expected to be another dash.
                // If not, eat all bytes until the end of the line.
                
                if (data[pos] == '-') {
                    // This is indeed the last boundary in the payload.
                    pos++;
                    parser->multipart.flags |= HTP_MULTIPART_SEEN_LAST_BOUNDARY;
                    parser->parser_state = STATE_BOUNDARY_EAT_LWS;
                } else {
                    // The second character is not a dash. This means that we have
                    // an error in the payload. We should report the error and
                    // continue to eat the rest of the line.
                    // TODO Error
                    parser->parser_state = STATE_BOUNDARY_EAT_LWS;
                }
                break;

            case STATE_BOUNDARY_EAT_LWS:
                if (data[pos] == CR) {
                    // CR byte, which could indicate a CRLF line ending.
                    pos++;
                    parser->parser_state = STATE_BOUNDARY_EAT_LWS_CR;
                }
                else if (data[pos] == LF) {
                    // LF line ending; we're done with boundary processing; data bytes follow.
                    pos++;
                    startpos = pos;
                    parser->multipart.flags |= HTP_MULTIPART_LF_LINE;
                    parser->parser_state = STATE_DATA;
                } else {
                    if (htp_is_lws(data[pos])) {
                        // Linear white space is allowed here.
                        parser->multipart.flags |= HTP_MULTIPART_BOUNDARY_LWS_AFTER;
                        pos++;
                    } else {
                        // Unexpected byte; consume, but remain in the same state.
                        parser->multipart.flags |= HTP_MULTIPART_BOUNDARY_NLWS_AFTER;
                        pos++;
                    }
                }
                break;

            case STATE_BOUNDARY_EAT_LWS_CR:
                if (data[pos] == LF) {
                    // CRLF line ending; we're done with boundary processing; data bytes follow.
                    pos++;
                    startpos = pos;
                    parser->multipart.flags |= HTP_MULTIPART_CRLF_LINE;
                    parser->parser_state = STATE_DATA;
                } else {
                    // Not a line ending; start again, but do not process this byte.
                    parser->parser_state = STATE_BOUNDARY_EAT_LWS;
                }
                break;
        } // switch
    }

    return HTP_OK;
}

/**
 * Determines if the supplied character is allowed in boundary.
 *
 * @param[in] c
 * @return 1 if the character is a valid boundary character, and 0 if it is not.
 */
int htp_mpartp_is_boundary_character(int c) {
    if ((c < 32) || (c > 126)) {
        return 0;
    }

    switch (c) {
        case '(':
        case ')':
        case '<':
        case '>':
        case '@':
        case ',':
        case ';':
        case ':':
        case '\\':
        case '"':
        case '/':
        case '[':
        case ']':
        case '?':
        case '=':
            return 0;
    }

    return 1;
}

/**
 * Extract boundary from the supplied Content-Type request header. The extracted
 * boundary will be allocated on the heap.
 *
 * @param[in] content_type
 * @param[in] boundary
 * @return HTP_OK on success, HTP_ERROR on failure.
 */
htp_status_t htp_mpartp_extract_boundary(bstr *content_type, bstr **boundary) {
    unsigned char *data = bstr_ptr(content_type);
    size_t len = bstr_len(content_type);
    size_t pos, start;

    pos = 0;

    // Look for the semicolon
    while ((pos < len) && (data[pos] != ';')) pos++;
    if (pos == len) {
        // Error: missing semicolon
        return -1;
    }

    // Skip over semicolon
    pos++;

    // Skip over whitespace
    while ((pos < len) && (data[pos] == ' '))
        pos++;
    if (pos == len) {
        // Error: missing boundary parameter
        return -2;
    }

    if (pos + 8 >= len) {
        // Error: invalid parameter
        return -3;
    }

    if ((data[pos] != 'b') || (data[pos + 1] != 'o') || (data[pos + 2] != 'u') || (data[pos + 3] != 'n')
            || (data[pos + 4] != 'd') || (data[pos + 5] != 'a') || (data[pos + 6] != 'r') || (data[pos + 7] != 'y')) {
        // Error invalid parameter
        return -4;
    }

    // Skip over "boundary"
    pos += 8;

    // Skip over whitespace, if any
    while ((pos < len) && (data[pos] == ' '))
        pos++;
    if (pos == len) {
        // Error: invalid parameter
        return -5;
    }

    // Expecting "=" next
    if (data[pos] != '=') {
        // Error: invalid parameter
        return -6;
    }

    // Skip over "="
    pos++;

    start = pos;

    while ((pos < len) && (htp_mpartp_is_boundary_character(data[pos]))) pos++;
    if (pos != len) {
        // Error: invalid character in boundary
        return -7;
    }

    *boundary = bstr_dup_mem(data + start, pos - start);
    if (*boundary == NULL) return -8;

    #if HTP_DEBUG
    fprint_bstr(stderr, "htp_mpartp_extract_boundary", *boundary);
    #endif

    return HTP_OK;
}
