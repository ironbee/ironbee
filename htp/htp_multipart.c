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

    return 1;
}

/**
 *
 */
int htp_mpart_part_receive_data(htp_mpart_part_t *part, unsigned char *data, size_t len) {
    fprint_raw_data(stderr, "HANDLE PART DATA", data, len);

    part->len += len;

    // We currently do not process the preamble and epilogue parts
    if ((part->type == MULTIPART_PART_PREAMBLE) || (part->type == MULTIPART_PART_EPILOGUE)) return 1;

    return 1;
}

/**
 *
 */
static int htp_mpartp_handle_data(htp_mpartp_t *mpartp, unsigned char *data, size_t len) {
    //fprint_raw_data(stderr, "HANDLE DATA", data, len);

    // Do we have a part?
    if (mpartp->current_part == NULL) {
        // Create new part
        mpartp->current_part = htp_mpart_part_create(mpartp);
        if (mpartp->current_part == NULL); // TODO RC

        if (mpartp->boundary_count == 0) {
            mpartp->current_part->type = MULTIPART_PART_PREAMBLE;
        } else {
            if (mpartp->seen_last_boundary) {
                mpartp->current_part->type = MULTIPART_PART_EPILOGUE;
            }
        }
    }

    // Process data
    htp_mpart_part_receive_data(mpartp->current_part, data, len); // TODO RC

    return 1;
}

/**
 *
 */
static int htp_mpartp_handle_boundary(htp_mpartp_t *mpartp) {
    fprintf(stderr, "HANDLE BOUNDARY\n");

    // TODO Having mpartp->seen_last_boundary set here means that there's
    //      a boundary after the "last boundary".

    if (mpartp->current_part != NULL) {
        htp_mpart_part_finalize_data(mpartp->current_part); // TODO RC

        // Add part to the list
    }

    // Create new part
    mpartp->current_part = htp_mpart_part_create(mpartp);
    if (mpartp->current_part == NULL); // TODO RC

    return 1;
}

/**
 * Creates a new multipart/form-data parser.
 *
 * @param boundar
 * @return New parser, or NULL on memory allocation failure.
 */
htp_mpartp_t *htp_mpartp_create(char *boundary) {
    htp_mpartp_t *mpartp = calloc(1, sizeof (htp_mpartp_t));
    if (mpartp == NULL) return NULL;

    mpartp->boundary_pieces = bstr_builder_create();
    if (mpartp->boundary_pieces == NULL) {
        free(mpartp);
        return NULL;
    }

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

    return mpartp;
}

/**
 * Destroys a multipart/form-data parser.
 */
void htp_mpartp_destroy(htp_mpartp_t *mpartp) {
    if (mpartp == NULL) return;

    free(mpartp->boundary);

    free(mpartp);
}

/**
 *
 */
void htp_mpartp_finalize(htp_mpartp_t *mpartp) {
    // If a CR byte was put aside, process it now as a single data byte
    if (mpartp->cr_aside) {
        mpartp->handle_data(mpartp, (unsigned char *) &"\r", 1);
        mpartp->cr_aside = 0;
    }

    // Process any pieces we have in the buffer as data
    bstr *b = NULL;
    list_iterator_reset(mpartp->boundary_pieces->pieces);
    while ((b = list_iterator_next(mpartp->boundary_pieces->pieces)) != NULL) {
        mpartp->handle_data(mpartp, (unsigned char *) bstr_ptr(b), bstr_len(b));
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

    size_t pos = 0;
    size_t startpos = 0;
    size_t boundarypos = 0;

    // Loop while there's data in the buffer
    while (pos < len) {

        // fprintf(stderr, "STATE %d pos %d\n", mpartp->state, pos);

STATE_SWITCH:
        switch (mpartp->state) {
            case MULTIPART_STATE_DATA:
                // If a CR byte was put aside, but we don't have a boundary
                // condition, process the byte now as a single data byte
                if ((mpartp->cr_aside) && (data[pos] != LF)) {
                    mpartp->handle_data(mpartp, (unsigned char *) &"\r", 1);
                    mpartp->cr_aside = 0;
                }

                // Process data
                while (pos < len) {
                    if (data[pos] == CR) {
                        if (pos + 1 == len) {
                            // We have CR as the last byte in input. We are going to process
                            // what we have in the buffer as data, except for the CR byte,
                            // which we're going to leave for later
                            mpartp->cr_aside = 1;
                        } else {
                            // We have at least one more byte in the buffer, so we are
                            // able to test for the LF byte
                            if (data[pos + 1] == LF) {
                                // Possible boundary start position (CRLF line)
                                boundarypos = pos;
                                mpartp->bpos = 0;
                                mpartp->state = MULTIPART_STATE_BOUNDARY;
                                goto STATE_SWITCH;
                            }
                        }
                    } else if (data[pos] == LF) {
                        // Possible boundary start position (LF line)
                        boundarypos = pos;
                        mpartp->bpos = 1;
                        mpartp->state = MULTIPART_STATE_BOUNDARY;
                        goto STATE_SWITCH;
                    }

DATA_CONTINUE:
                    // Consume one data byte
                    pos++;
                } // while

                // End of data; process data chunk (one byte less if we've put a CR byte aside)                
                mpartp->handle_data(mpartp, data + startpos, pos - startpos - ((mpartp->cr_aside == 1) ? 1 : 0));
                break;

            case MULTIPART_STATE_BOUNDARY:
                // Possible boundary
                while (pos < len) {
                    //fprintf(stderr, "B byte %d desired %d\n", data[pos], mpartp->boundary[mpartp->bpos]);

                    // Check if the bytes match
                    if (!(data[pos] == mpartp->boundary[mpartp->bpos])) {
                        // Mismatch

                        // If a CR byte was put aside, process it now as a single data byte
                        if (mpartp->cr_aside) {
                            mpartp->handle_data(mpartp, (unsigned char *) &"\r", 1);
                            mpartp->cr_aside = 0;
                        }

                        // Process any pieces we have in the buffer as data
                        bstr *b = NULL;
                        list_iterator_reset(mpartp->boundary_pieces->pieces);
                        while ((b = list_iterator_next(mpartp->boundary_pieces->pieces)) != NULL) {
                            mpartp->handle_data(mpartp, (unsigned char *) bstr_ptr(b), bstr_len(b));
                        }
                        bstr_builder_clear(mpartp->boundary_pieces);

                        // Revert state back to data parsing
                        pos = boundarypos;
                        mpartp->state = MULTIPART_STATE_DATA;

                        // If the first byte was a CR we want to skip over it to
                        // avoid the newline being detected as a potential boundary
                        // again (that's because we support both CRLF and LF line endings). The
                        // CR byte is still going to be consumed as data.
                        if (data[pos] == CR) {
                            pos++;
                        }

                        goto DATA_CONTINUE;
                    }

                    // Consume boundary byte
                    pos++;

                    // Are we at boundary?
                    if (++mpartp->bpos == mpartp->blen) {
                        // Boundary match

                        // Process data chunk prior to the boundary, if any
                        if (boundarypos - startpos > 0) {
                            mpartp->handle_data(mpartp, data + startpos, boundarypos - startpos);
                        }

                        // Remove the boundary pieces (we might use them in the future, though).
                        bstr_builder_clear(mpartp->boundary_pieces);

                        // Keep track of how many boundaries we've seen
                        mpartp->boundary_count++;

                        // Run boundary match
                        mpartp->handle_boundary(mpartp);

                        // We now need to check if this is the last boundary in the payload
                        mpartp->state = MULTIPART_STATE_BOUNDARY_IS_LAST2;
                        goto STATE_SWITCH;
                    }
                } // while

                // End of data; process data chunk (if any)
                if (boundarypos - startpos > 0) {
                    mpartp->handle_data(mpartp, data + startpos, boundarypos - startpos);
                }

                // Preveserve the partial boundary; we'll need it later, if it
                // turns out that it isn't a boundary after all
                bstr_builder_append_mem(mpartp->boundary_pieces, (char *) data + boundarypos, pos - boundarypos);
                //fprint_raw_data(stderr, "SET ASIDE", data + boundarypos, pos - boundarypos);

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

                //case MULTIPART_STATE_BOUNDARY_EAT_CRLF:
                //    if (data[pos] == CR) {
                //        pos++;
                //        mpartp->state = MULTIPART_STATE_BOUNDARY_EAT_LF;
                //    } else {
                //        // Error!
                //        // Unexpected byte; remain in the same state
                //        pos++;
                //    }
                //    break;

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
