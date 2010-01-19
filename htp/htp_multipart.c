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

#include "htp.h"
#include "htp_multipart.h"

/**
 *
 */
htp_mpart_part_t *htp_mpart_part_create(htp_mpartp_t *mpartp) {
    htp_mpart_part_t * part = calloc(1, sizeof (htp_mpart_part_t));
    if (part == NULL) return NULL;

    part->mpartp = mpartp;

    return part;
}

/**
 *
 */
void htp_mpart_part_destroy(htp_mpart_part_t *part) {
    if (part == NULL) return;

    free(part);
}

/**
 *
 */
int htp_mpart_part_finalize_data(htp_mpart_part_t *part) {
    fprintf(stderr, "FINALIZE DATA\n");

    // We currently do not process the preamble and epilogue parts
    if ((part->type == MULTIPART_PART_PREAMBLE) || (part->type == MULTIPART_PART_EPILOGUE)) return 1;

    if (part->type != MULTIPART_PART_FILE) {
        part->value = bstr_builder_to_str(part->mpartp->part_pieces);
        fprint_raw_data(stderr, "PART DATA", (unsigned char *) bstr_ptr(part->value), bstr_len(part->value));
        bstr_builder_clear(part->mpartp->part_pieces);
    }

    return 1;
}

/**
 *
 */
int htp_mpart_part_receive_data(htp_mpart_part_t *part, unsigned char *data, size_t len, int line) {
    if (part->mode == MULTIPART_MODE_LINE) {
        fprint_raw_data(stderr, "HANDLE PART DATA (LINE)", data, len);
    } else {
        fprint_raw_data(stderr, "HANDLE PART DATA (DATA)", data, len);
    }

    if (line) {
        fprintf(stderr, "LINE END\n");
    }

    part->len += len;

    // We currently do not process the preamble and epilogue parts
    if ((part->type == MULTIPART_PART_PREAMBLE) || (part->type == MULTIPART_PART_EPILOGUE)) return 1;

    if (part->mode == MULTIPART_MODE_LINE) {
        // Line mode

        if (line) {
            // End of line

            // Is it an empty line?
            if ((len == 0) && (bstr_builder_size(part->mpartp->part_pieces) == 0)) {
                // Yes, it's an empty line; switch to data mode
                part->mode = MULTIPART_MODE_DATA;
            } else {
                // Not an empty line
                bstr_builder_append_mem(part->mpartp->part_pieces, (char *) data, len);
                // XXX Do something with the line
                bstr_builder_clear(part->mpartp->part_pieces);
            }
        } else {
            // Not end of line; keep the data chunk for later
            bstr_builder_append_mem(part->mpartp->part_pieces, (char *) data, len);
        }
    } else {
        // Data mode; keep the data chunk for later (but not if it is a file)
        if (part->type != MULTIPART_PART_FILE) {
            bstr_builder_append_mem(part->mpartp->part_pieces, (char *) data, len);
        }
    }

    return 1;
}

/**
 *
 */
static int htp_mpartp_handle_data(htp_mpartp_t *mpartp, unsigned char *data, size_t len, int line) {
    //fprint_raw_data(stderr, "HANDLE DATA", data, len);

    // Do we have a part?
    if (mpartp->current_part == NULL) {
        // Create new part
        mpartp->current_part = htp_mpart_part_create(mpartp);
        if (mpartp->current_part == NULL) return -1; // TODO RC

        if (mpartp->boundary_count == 0) {
            mpartp->current_part->type = MULTIPART_PART_PREAMBLE;
            mpartp->current_part->mode = MULTIPART_MODE_DATA;
        } else {
            if (mpartp->seen_last_boundary) {
                mpartp->current_part->type = MULTIPART_PART_EPILOGUE;
                mpartp->current_part->mode = MULTIPART_MODE_DATA;
            }
        }
    }

    // Process data
    htp_mpart_part_receive_data(mpartp->current_part, data, len, line); // TODO RC

    return 1;
}

/**
 *
 */
static int htp_mpartp_handle_boundary(htp_mpartp_t * mpartp) {
    fprintf(stderr, "HANDLE BOUNDARY\n");

    // TODO Having mpartp->seen_last_boundary set here means that there's
    //      a boundary after the "last boundary".

    if (mpartp->current_part != NULL) {
        if (htp_mpart_part_finalize_data(mpartp->current_part) < 0) return -1; // TODO RC

        // Add part to the list
    }

    // Create new part
    mpartp->current_part = htp_mpart_part_create(mpartp);
    if (mpartp->current_part == NULL) return -1; // TODO RC

    return 1;
}

/**
 * Creates a new multipart/form-data parser.
 *
 * @param boundar
 * @return New parser, or NULL on memory allocation failure.
 */
htp_mpartp_t * htp_mpartp_create(char *boundary) {
    htp_mpartp_t *mpartp = calloc(1, sizeof (htp_mpartp_t));
    if (mpartp == NULL) return NULL;

    mpartp->boundary_pieces = bstr_builder_create();
    if (mpartp->boundary_pieces == NULL) {
        free(mpartp);
        return NULL;
    }

    mpartp->part_pieces = bstr_builder_create();
    if (mpartp->part_pieces == NULL) {
        bstr_builder_destroy(mpartp->part_pieces);
        free(mpartp);
        return NULL;
    }

    // XXX Lowercase boundary

    mpartp->boundary = malloc(strlen(boundary) + 3 + 1);
    if (mpartp->boundary == NULL) {
        bstr_builder_destroy(mpartp->boundary_pieces);
        free(mpartp);
        return NULL;
    }

    mpartp->boundary[0] = CR;
    mpartp->boundary[1] = LF;
    mpartp->boundary[2] = '-';
    mpartp->boundary[3] = '-';
    strcpy(mpartp->boundary + 4, boundary);
    mpartp->blen = strlen(mpartp->boundary);

    //mpartp->state = MULTIPART_STATE_DATA;
    mpartp->state = MULTIPART_STATE_BOUNDARY;
    mpartp->bpos = 2;

    mpartp->handle_data = htp_mpartp_handle_data;
    mpartp->handle_boundary = htp_mpartp_handle_boundary;

    mpartp->aside_buf[0] = '\0';
    mpartp->aside_buf[1] = '\0';
    mpartp->aside_buf[2] = '\0';

    return mpartp;
}

/**
 * Destroys a multipart/form-data parser.
 */
void htp_mpartp_destroy(htp_mpartp_t * mpartp) {
    if (mpartp == NULL) return;

    free(mpartp->boundary);

    bstr_builder_destroy(mpartp->part_pieces);
    bstr_builder_destroy(mpartp->boundary_pieces);

    free(mpartp);
}

/**
 *
 */
void htp_mpartp_finalize(htp_mpartp_t * mpartp) {
    // If some data was put aside, process it now
    if (mpartp->aside_len > 0) {
        mpartp->handle_data(mpartp, mpartp->aside_buf, mpartp->aside_len, 0);
    }

    // Process any pieces we have in the buffer as data
    bstr *b = NULL;
    list_iterator_reset(mpartp->boundary_pieces->pieces);
    while ((b = list_iterator_next(mpartp->boundary_pieces->pieces)) != NULL) {
        mpartp->handle_data(mpartp, (unsigned char *) bstr_ptr(b), bstr_len(b), 0);
    }

    bstr_builder_clear(mpartp->boundary_pieces);
}

/**
 * Parses a chunk of multipart/form-data data. This function should be called
 * as many times as necessary until all data has been consumed.
 *
 * @param mpartp
 * @parma data
 * @param len
 * @return Status indicator
 */
int htp_mpartp_parse(htp_mpartp_t *mpartp, unsigned char *data, size_t len) {
    //fprint_raw_data_ex(stderr, "INPUT", data, 0, len);

    size_t pos = 0; // Current position in the input chunk.
    size_t startpos = 0; // The starting position of data.
    size_t boundarypos = 0; // The position of the (possible) boundary.
    size_t local_aside_len = 0; // How many bytes have we put side from this chunk only?

    // Loop while there's data in the buffer
    while (pos < len) {
STATE_SWITCH:
        fprintf(stderr, "STATE %d pos %d\n", mpartp->state, pos);

        switch (mpartp->state) {
            case MULTIPART_STATE_DATA:
                // If we encounter any data set aside in this state, that means it's
                // a CR byte from the end of the previous chunk. If we don't
                // see LF as the first byte in this chunk, process the single
                // CR byte as data and clear the aside buffer.
                if ((pos < len) && (mpartp->aside_len > 0) && (data[len] != LF)) {
                    mpartp->handle_data(mpartp, (unsigned char *) &"\r", 1, 0);
                    mpartp->aside_len = 0;
                }

                // Loop through available data
                while (pos < len) {
                    if (data[pos] == CR) {
                        // We have a CR byte

                        // Is this CR the last byte?
                        if (pos + 1 == len) {
                            // We have CR as the last byte in input. We are going to process
                            // what we have in the buffer as data, except for the CR byte,
                            // which we're going to leave for later. If it happens that a
                            // CR is follewed by a LF and then a boundary, the CR is going
                            // to be discarded.
                            pos++; // Take CR from input
                            mpartp->aside_buf[0] = CR;
                            mpartp->aside_len = 1;
                            local_aside_len = 1;
                        } else {
                            // We have CR and at least one more byte in the buffer, so we
                            // are able to test for the LF byte too.
                            if (data[pos + 1] == LF) {
                                pos += 2; // Take CR and LF from input
                                mpartp->aside_buf[0] = CR;
                                mpartp->aside_buf[1] = LF;
                                mpartp->aside_len = 2;
                                local_aside_len = 2;

                                // Prepare to switch to boundary testing
                                boundarypos = pos;
                                mpartp->bpos = 2; // After LF/first dash
                                mpartp->state = MULTIPART_STATE_BOUNDARY;

                                // Eat line endings in line mode
                                if ((mpartp->current_part != NULL) && (mpartp->current_part->mode == MULTIPART_MODE_LINE)) {
                                    // We are in line mode, so we'll send the data contents for processing
                                    // and then eat the CR and LF
                                    mpartp->handle_data(mpartp, data + startpos, pos - startpos - 2, 1 /* End of line */);
                                    mpartp->aside_len = 0;
                                    startpos = pos;
                                }

                                goto STATE_SWITCH;
                            }
                        }
                    } else if (data[pos] == LF) {
                        // Possible boundary start position (LF line)
                        pos++; // Take LF from input

                        // We may see a CR that was set aside in the previous chunk.
                        if (mpartp->aside_len != 0) {
                            mpartp->aside_buf[0] = CR;
                            mpartp->aside_buf[1] = LF;
                            mpartp->aside_len = 2;
                            local_aside_len = 1;
                        } else {
                            mpartp->aside_buf[0] = LF;
                            mpartp->aside_len = 1;
                            local_aside_len = 1;
                        }

                        // Prepare to switch to boundary testing
                        boundarypos = pos;
                        mpartp->bpos = 2; // After LF/first dash
                        mpartp->state = MULTIPART_STATE_BOUNDARY;

                        // Eat line endings in line mode
                        if ((mpartp->current_part != NULL) && (mpartp->current_part->mode == MULTIPART_MODE_LINE)) {
                            // We are in line mode, so we'll send the data contents for processing
                            // and then eat the CR and LF
                            mpartp->handle_data(mpartp, data + startpos, pos - startpos - 1, 1 /* End of line */);
                            mpartp->aside_len = 0;
                            startpos = pos;
                        }

                        goto STATE_SWITCH;
                    } else {
                        // Take one unintersting byte from input
                        pos++;
                    }
                } // while

                // End of data; process data chunk (minus any locally consumed bytes)
                mpartp->handle_data(mpartp, data + startpos, pos - startpos - local_aside_len, 0);

                break;

            case MULTIPART_STATE_BOUNDARY:
                // Possible boundary
                while (pos < len) {
                    // fprintf(stderr, "B byte %d desired %d\n", data[pos], mpartp->boundary[mpartp->bpos]);

                    // Check if the bytes match
                    if (!(data[pos] == mpartp->boundary[mpartp->bpos])) {
                        // Mismatch
                        
                        // Do we have any pieces in our buffer (which would happen with a boundary
                        // that was spread across several input packets)?
                        if (bstr_builder_size(mpartp->boundary_pieces) > 0) {
                            // If a line ending was put aside and we are not in line mode,
                            // process the line ending as data now. Otherwise eat the line ending.
                            if (mpartp->aside_len > 0) {
                                if ((mpartp->current_part != NULL) && (mpartp->current_part->mode != MULTIPART_MODE_LINE)) {
                                    mpartp->handle_data(mpartp, mpartp->aside_buf, mpartp->aside_len, 0 /* Not end of line */);
                                }

                                mpartp->aside_len = 0;
                            }

                            // Process any pieces we have stored as data.
                            bstr *b = NULL;
                            list_iterator_reset(mpartp->boundary_pieces->pieces);
                            while ((b = list_iterator_next(mpartp->boundary_pieces->pieces)) != NULL) {
                                mpartp->handle_data(mpartp, (unsigned char *) bstr_ptr(b), bstr_len(b), 0);
                            }

                            bstr_builder_clear(mpartp->boundary_pieces);
                        } else if ((boundarypos == 0)&&(mpartp->aside_len > 0)) {
                            // The set-aside data is from a previous chunk
                            mpartp->handle_data(mpartp, mpartp->aside_buf, mpartp->aside_len, 0 /* Not end of line */);
                            mpartp->aside_len = 0;
                        }                       

                        // Revert state back to data parsing
                        pos = boundarypos;
                        mpartp->aside_len = 0;
                        mpartp->state = MULTIPART_STATE_DATA;

                        goto STATE_SWITCH;
                    }

                    // Consume boundary byte
                    pos++;

                    // Are we at boundary?
                    if (++mpartp->bpos == mpartp->blen) {
                        // Boundary match

                        // Process data chunk prior to the boundary
                        if (boundarypos - startpos > 0) {
                            mpartp->handle_data(mpartp, data + startpos, boundarypos - startpos - local_aside_len, 0);
                        }

                        // Clear any data we have set aside
                        mpartp->aside_len = 0;

                        // Remove the boundary pieces, if any.
                        bstr_builder_clear(mpartp->boundary_pieces);

                        // Keep track of how many boundaries we've seen.
                        mpartp->boundary_count++;

                        // Run boundary match.
                        mpartp->handle_boundary(mpartp);

                        // We now need to check if this is the last boundary in the payload
                        mpartp->state = MULTIPART_STATE_BOUNDARY_IS_LAST2;
                        goto STATE_SWITCH;
                    }
                } // while

                // End of data; process the data before (potential) boundary,
                // but leave the line ending alone, for when we dicover whether
                // what we're testing really is a boundary
                if (boundarypos - startpos > 0) {
                    mpartp->handle_data(mpartp, data + startpos, boundarypos - startpos - local_aside_len, 0);
                }

                // Preveserve the partial boundary; we'll need it later, if it
                // turns out that it isn't a boundary after all
                if (pos - boundarypos > 0) {
                    bstr_builder_append_mem(mpartp->boundary_pieces, (char *) data + boundarypos, pos - boundarypos);                    
                }

                break;

            case MULTIPART_STATE_BOUNDARY_IS_LAST2:
                // We're expecting two dashes
                if (data[pos] == '-') {
                    // Still hoping!
                    pos++;
                    mpartp->state = MULTIPART_STATE_BOUNDARY_IS_LAST1;
                } else {
                    // Hmpf, it's not the last boundary.
                    mpartp->state = MULTIPART_STATE_BOUNDARY_EAT_LF;
                }
                break;

            case MULTIPART_STATE_BOUNDARY_IS_LAST1:
                // One more dash left to go
                if (data[pos] == '-') {
                    // This is indeed the last boundary in the payload
                    pos++;
                    mpartp->seen_last_boundary = 1;
                    mpartp->state = MULTIPART_STATE_BOUNDARY_EAT_LF;
                } else {
                    // The second character is not a dash. This means that we have
                    // an error in the payload. We should report the error and
                    // continue to eat the rest of the line.
                    // TODO Error
                    mpartp->state = MULTIPART_STATE_BOUNDARY_EAT_LF;
                }
                break;

            case MULTIPART_STATE_BOUNDARY_EAT_LF:
                if (data[pos] == LF) {
                    pos++;
                    startpos = pos;
                    mpartp->state = MULTIPART_STATE_DATA;
                } else {
                    // Error!
                    // Unexpected byte; remain in the same state
                    pos++;
                }
                break;
        } // switch
    }

    return 1;
}
