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

#include <stdlib.h>

#include "htp.h"
#include "htp_private.h"
#include "htp_hybrid.h"

/**
 * Performs check for a CONNECT transaction to decide whether inbound
 * parsing needs to be suspended.
 *
 * @param connp
 * @return HTP_OK if the request does not use CONNECT, HTP_DATA_OTHER if
 *          inbound parsing needs to be suspended until we hear from the
 *          other side
 */
int htp_connp_REQ_CONNECT_CHECK(htp_connp_t *connp) {
    // If the request uses the CONNECT method, then there will
    // not be a request body, but first we need to wait to see the
    // response in order to determine if the tunneling request
    // was a success.
    if (connp->in_tx->request_method_number == HTP_M_CONNECT) {
        connp->in_state = htp_connp_REQ_CONNECT_WAIT_RESPONSE;
        connp->in_status = STREAM_STATE_DATA_OTHER;
        connp->in_tx->progress = TX_PROGRESS_WAIT;

        return HTP_DATA_OTHER;
    }

    // Continue to the next step to determine 
    // the presence of request body
    connp->in_state = htp_connp_REQ_BODY_DETERMINE;

    return HTP_OK;
}

/**
 * Determines whether inbound parsing, which was suspended after
 * encountering a CONNECT transaction, can proceed (after receiving
 * the response).
 *
 * @param connp
 * @return HTP_OK if the parser can resume parsing, HTP_DATA_OTHER if
 *         it needs to continue waiting.
 */
int htp_connp_REQ_CONNECT_WAIT_RESPONSE(htp_connp_t *connp) {
    // Check that we saw the response line of the current
    // inbound transaction.
    if (connp->in_tx->progress <= TX_PROGRESS_RES_LINE) {
        return HTP_DATA_OTHER;
    }

    // A 2xx response means a tunnel was established. Anything
    // else means we continue to follow the HTTP stream.
    if ((connp->in_tx->response_status_number >= 200) && (connp->in_tx->response_status_number <= 299)) {
        // TODO Check that the server did not accept a connection
        //      to itself.

        // The requested tunnel was established: we are going
        // to ignore the remaining data on this stream
        connp->in_status = STREAM_STATE_TUNNEL;
        connp->in_state = htp_connp_REQ_FINALIZE;
    } else {
        // No tunnel; continue to the next transaction
        connp->in_state = htp_connp_REQ_FINALIZE;
    }

    return HTP_OK;
}

/**
 * Consumes bytes until the end of the current line.
 *
 * @param connp
 * @returns HTP_OK on state change, HTTP_ERROR on error, or HTP_DATA when more data is needed.
 */
int htp_connp_REQ_BODY_CHUNKED_DATA_END(htp_connp_t *connp) {
    // TODO We shouldn't really see anything apart from CR and LF,
    // so we should warn about anything else.

    for (;;) {
        IN_NEXT_BYTE_OR_RETURN(connp);

        connp->in_tx->request_message_len++;

        if (connp->in_next_byte == LF) {
            connp->in_state = htp_connp_REQ_BODY_CHUNKED_LENGTH;
            return HTP_OK;
        }
    }
}

/**
 * Processes a chunk of data.
 *
 * @param connp
 * @returns HTP_OK on state change, HTTP_ERROR on error, or HTP_DATA when more data is needed.
 */
int htp_connp_REQ_BODY_CHUNKED_DATA(htp_connp_t *connp) {
    htp_tx_data_t d;

    d.tx = connp->in_tx;
    d.data = &connp->in_current_data[connp->in_current_offset];
    d.len = 0;

    for (;;) {
        IN_NEXT_BYTE(connp);

        if (connp->in_next_byte == -1) {
            // Keep track of actual data length
            connp->in_tx->request_entity_len += d.len;

            // Send data to callbacks            
            int rc = htp_req_run_hook_body_data(connp, &d);
            if (rc != HOOK_OK) {
                htp_log(connp, HTP_LOG_MARK, HTP_LOG_ERROR, 0,
                        "Request body data callback returned error (%d)", rc);
                return HTP_ERROR;
            }

            // Ask for more data
            return HTP_DATA;
        } else {
            connp->in_tx->request_message_len++;
            connp->in_chunked_length--;
            d.len++;

            if (connp->in_chunked_length == 0) {
                // End of data chunk

                // Keep track of actual data length
                connp->in_tx->request_entity_len += d.len;

                // Send data to callbacks               
                int rc = htp_req_run_hook_body_data(connp, &d);
                if (rc != HOOK_OK) {
                    htp_log(connp, HTP_LOG_MARK, HTP_LOG_ERROR, 0,
                            "Request body data callback returned error (%d)", rc);
                    return HTP_ERROR;
                }

                connp->in_state = htp_connp_REQ_BODY_CHUNKED_DATA_END;

                return HTP_OK;
            }
        }
    }
}

/**
 * Extracts chunk length.
 *
 * @param connp
 * @returns HTP_OK on state change, HTTP_ERROR on error, or HTP_DATA when more data is needed.
 */
int htp_connp_REQ_BODY_CHUNKED_LENGTH(htp_connp_t *connp) {
    for (;;) {
        IN_COPY_BYTE_OR_RETURN(connp);

        connp->in_tx->request_message_len++;

        // Have we reached the end of the line?
        if (connp->in_next_byte == LF) {
            htp_chomp(connp->in_line, &connp->in_line_len);

            // Extract chunk length
            connp->in_chunked_length = htp_parse_chunked_length(connp->in_line, connp->in_line_len);

            // Cleanup for the next line
            connp->in_line_len = 0;

            // Handle chunk length
            if (connp->in_chunked_length > 0) {
                // More data available
                // TODO Add a check for chunk length
                connp->in_state = htp_connp_REQ_BODY_CHUNKED_DATA;
            } else if (connp->in_chunked_length == 0) {
                // End of data
                connp->in_state = htp_connp_REQ_HEADERS;
                connp->in_tx->progress = TX_PROGRESS_REQ_TRAILER;
            } else {
                // Invalid chunk length
                htp_log(connp, HTP_LOG_MARK, HTP_LOG_ERROR, 0,
                        "Request chunk encoding: Invalid chunk length");
                return HTP_ERROR;
            }

            return HTP_OK;
        }
    }
}

/**
 * Processes identity request body.
 *
 * @param connp
 * @returns HTP_OK on state change, HTTP_ERROR on error, or HTP_DATA when more data is needed.
 */
int htp_connp_REQ_BODY_IDENTITY(htp_connp_t *connp) {
    htp_tx_data_t d;

    d.tx = connp->in_tx;
    d.data = &connp->in_current_data[connp->in_current_offset];
    d.len = 0;

    for (;;) {
        IN_NEXT_BYTE(connp);

        if (connp->in_next_byte == -1) {
            // End of chunk

            // Keep track of actual data length
            connp->in_tx->request_entity_len += d.len;

            int rc = htp_req_run_hook_body_data(connp, &d);
            if (rc != HOOK_OK) {
                htp_log(connp, HTP_LOG_MARK, HTP_LOG_ERROR, 0,
                        "Request body data callback returned error (%d)", rc);
                return HTP_ERROR;
            }

            // Ask for more data
            return HTP_DATA;
        } else {
            connp->in_tx->request_message_len++;
            connp->in_body_data_left--;
            d.len++;

            if (connp->in_body_data_left == 0) {
                // End of body

                // Keep track of actual data length
                connp->in_tx->request_entity_len += d.len;

                int rc = htp_req_run_hook_body_data(connp, &d);
                if (rc != HOOK_OK) {
                    htp_log(connp, HTP_LOG_MARK, HTP_LOG_ERROR, 0,
                            "Request body data callback returned error (%d)", rc);
                    return HTP_ERROR;
                }

                // Move to finalize the request
                connp->in_state = htp_connp_REQ_FINALIZE;

                return HTP_OK;
            }
        }
    }
}

/**
 * Determines presence (and encoding) of a request body.
 *
 * @param connp
 * @returns HTP_OK on state change, HTTP_ERROR on error, or HTP_DATA when more data is needed.
 */
int htp_connp_REQ_BODY_DETERMINE(htp_connp_t *connp) {
    if (connp->in_tx->request_transfer_coding == HTP_CODING_CHUNKED) {
        connp->in_state = htp_connp_REQ_BODY_CHUNKED_LENGTH;
        connp->in_tx->progress = TX_PROGRESS_REQ_BODY;
    } else if (connp->in_tx->request_transfer_coding == HTP_CODING_IDENTITY) {
        connp->in_content_length = connp->in_tx->request_content_length;
        connp->in_body_data_left = connp->in_content_length;

        if (connp->in_content_length != 0) {
            connp->in_state = htp_connp_REQ_BODY_IDENTITY;
            connp->in_tx->progress = TX_PROGRESS_REQ_BODY;
        } else {
            connp->in_tx->connp->in_state = htp_connp_REQ_FINALIZE;
        }
    } else {
        // This request does not have a body, which
        // means that we're done with it
        connp->in_state = htp_connp_REQ_FINALIZE;
    }

    return HTP_OK;
}

/**
 * Parses request headers.
 *
 * @param connp
 * @returns HTP_OK on state change, HTTP_ERROR on error, or HTP_DATA when more data is needed.
 */
int htp_connp_REQ_HEADERS(htp_connp_t *connp) {
    for (;;) {
        IN_COPY_BYTE_OR_RETURN(connp);

        // Allocate structure to hold one header line
        if (connp->in_header_line == NULL) {
            connp->in_header_line = calloc(1, sizeof (htp_header_line_t));
            if (connp->in_header_line == NULL) return HTP_ERROR;
            connp->in_header_line->first_nul_offset = -1;
        }

        // Keep track of NUL bytes
        if (connp->in_next_byte == 0) {
            // Store the offset of the first NUL
            if (connp->in_header_line->has_nulls == 0) {
                connp->in_header_line->first_nul_offset = connp->in_line_len;
            }

            // Remember how many NULs there were
            connp->in_header_line->flags |= HTP_FIELD_NUL_BYTE;
            connp->in_header_line->has_nulls++;
        }

        // Have we reached the end of the line?
        if (connp->in_next_byte == LF) {
            #ifdef HTP_DEBUG
            fprint_raw_data(stderr, __FUNCTION__, connp->in_line, connp->in_line_len);
            #endif

            // Should we terminate headers?
            if (htp_connp_is_line_terminator(connp, connp->in_line, connp->in_line_len)) {
                // Terminator line
                connp->in_tx->request_headers_sep = bstr_dup_mem((char *) connp->in_line, connp->in_line_len);
                if (connp->in_tx->request_headers_sep == NULL) {
                    return HTP_ERROR;
                }

                // Parse previous header, if any
                if (connp->in_header_line_index != -1) {
                    if (connp->cfg->process_request_header(connp) != HTP_OK) {
                        // Note: downstream responsible for error logging
                        return HTP_ERROR;
                    }

                    // Reset index
                    connp->in_header_line_index = -1;
                }

                // Cleanup
                free(connp->in_header_line);
                connp->in_line_len = 0;
                connp->in_header_line = NULL;

                // We've seen all request headers
                return htp_txh_state_request_headers(connp->in_tx);
            }

            // Prepare line for consumption
            int chomp_result = htp_chomp(connp->in_line, &connp->in_line_len);

            // Check for header folding
            if (htp_connp_is_line_folded(connp->in_line, connp->in_line_len) == 0) {
                // New header line

                // Parse previous header, if any
                if (connp->in_header_line_index != -1) {
                    if (connp->cfg->process_request_header(connp) != HTP_OK) {
                        // Note: downstream responsible for error logging
                        return HTP_ERROR;
                    }

                    // Reset index
                    connp->in_header_line_index = -1;
                }

                // Remember the index of the fist header line
                connp->in_header_line_index = connp->in_header_line_counter;
            } else {
                // Folding; check that there's a previous header line to add to
                if (connp->in_header_line_index == -1) {
                    if (!(connp->in_tx->flags & HTP_INVALID_FOLDING)) {
                        connp->in_tx->flags |= HTP_INVALID_FOLDING;
                        htp_log(connp, HTP_LOG_MARK, HTP_LOG_WARNING, 0,
                                "Invalid request field folding");
                    }
                }
            }

            // Add the raw header line to the list
            connp->in_header_line->line = bstr_dup_mem((char *) connp->in_line, connp->in_line_len + chomp_result);
            if (connp->in_header_line->line == NULL) {
                return HTP_ERROR;
            }

            list_add(connp->in_tx->request_header_lines, connp->in_header_line);
            connp->in_header_line = NULL;

            // Cleanup for the next line
            connp->in_line_len = 0;
            if (connp->in_header_line_index == -1) {
                connp->in_header_line_index = connp->in_header_line_counter;
            }

            connp->in_header_line_counter++;
        }
    }
}

/**
 * Determines request protocol.
 *
 * @param connp
 * @returns HTP_OK on state change, HTTP_ERROR on error, or HTP_DATA when more data is needed.
 */
int htp_connp_REQ_PROTOCOL(htp_connp_t *connp) {
    // Is this a short-style HTTP/0.9 request? If it is,
    // we will not want to parse request headers.
    if (connp->in_tx->protocol_is_simple == 0) {
        // Switch to request header parsing.
        connp->in_state = htp_connp_REQ_HEADERS;
        connp->in_tx->progress = TX_PROGRESS_REQ_HEADERS;
    } else {
        // We're done with this request.
        connp->in_state = htp_connp_REQ_FINALIZE;
    }

    return HTP_OK;
}

/**
 * Parses request line.
 *
 * @param connp
 * @returns HTP_OK on state change, HTTP_ERROR on error, or HTP_DATA when more data is needed.
 */
int htp_connp_REQ_LINE(htp_connp_t *connp) {
    for (;;) {
        // Get one byte
        IN_COPY_BYTE_OR_RETURN(connp);

        // Keep track of NUL bytes
        if (connp->in_next_byte == 0) {
            // Remember how many NULs there were
            connp->in_tx->request_line_nul++;

            // Store the offset of the first NUL byte
            if (connp->in_tx->request_line_nul_offset == -1) {
                connp->in_tx->request_line_nul_offset = connp->in_line_len;
            }
        }

        // Have we reached the end of the line?
        if (connp->in_next_byte == LF) {
            #ifdef HTP_DEBUG
            fprint_raw_data(stderr, __FUNCTION__, connp->in_line, connp->in_line_len);
            #endif

            // Is this a line that should be ignored?
            if (htp_connp_is_line_ignorable(connp, connp->in_line, connp->in_line_len)) {
                // We have an empty/whitespace line, which we'll note, ignore and move on
                connp->in_tx->request_ignored_lines++;

                // TODO How many empty lines are we willing to accept?

                // Start again
                connp->in_line_len = 0;

                return HTP_OK;
            }

            // Process request line

            connp->in_tx->request_line_raw = bstr_dup_mem((char *) connp->in_line, connp->in_line_len);
            if (connp->in_tx->request_line_raw == NULL) {
                return HTP_ERROR;
            }

            /// @todo Would be nice to reference request_line_raw data
            htp_chomp(connp->in_line, &connp->in_line_len);
            connp->in_tx->request_line = bstr_dup_ex(connp->in_tx->request_line_raw, 0, connp->in_line_len);
            if (connp->in_tx->request_line == NULL) {
                return HTP_ERROR;
            }

            // Parse request line
            if (connp->cfg->parse_request_line(connp) != HTP_OK) {
                // Note: downstream responsible for error logging
                return HTP_ERROR;
            }

            // Finalize request line parsing

            if (htp_txh_state_request_line(connp->in_tx) != HTP_OK) {
                return HTP_ERROR;
            }

            // Clean up.
            connp->in_line_len = 0;



            return HTP_OK;
        }
    }
}

int htp_connp_REQ_FINALIZE(htp_connp_t *connp) {
    // Run the last REQUEST_BODY_DATA HOOK, but
    // only if there was a request body.
    if (connp->in_tx->request_transfer_coding != -1) {
        htp_tx_data_t d;

        d.data = NULL;
        d.len = 0;
        d.tx = connp->in_tx;

        htp_req_run_hook_body_data(connp, &d);
    }

    // Run hook REQUEST
    int rc = hook_run_all(connp->cfg->hook_request_done, connp);
    if (rc != HOOK_OK) return rc;

    // Clean-up
    if (connp->put_file != NULL) {
        bstr_free(&connp->put_file->filename);
        free(connp->put_file);
        connp->put_file = NULL;
    }

    // We're done with this request
    connp->in_state = htp_connp_REQ_IDLE;

    // Update the transaction status, but only if it did already
    // move on. This may happen when we're processing a CONNECT
    // request and need to wait for the response to determine how
    // to continue to treat the rest of the TCP stream.
    if (connp->in_tx->progress < TX_PROGRESS_WAIT) {
        connp->in_tx->progress = TX_PROGRESS_WAIT;
    }

    connp->in_tx = NULL;

    return HTP_OK;
}

/**
 * The idle state is invoked before and after every transaction. Consequently,
 * it will start a new transaction when data is available and finalise a transaction
 * which has been processed.
 *
 * @param connp
 * @returns HTP_OK on state change, HTTP_ERROR on error, or HTP_DATA when more data is needed.
 */
int htp_connp_REQ_IDLE(htp_connp_t * connp) {
    // We want to start parsing the next request (and change
    // the state from IDLE) only if there's at least one
    // byte of data available. Otherwise we could be creating
    // new structures even if there's no more data on the
    // connection.
    IN_TEST_NEXT_BYTE_OR_RETURN(connp);

    connp->in_tx = htp_txh_create(connp);
    if (connp->in_tx == NULL) return HTP_ERROR;

    // Change state to TRANSACTION_START
    htp_txh_state_request_start(connp->in_tx);

    return HTP_OK;
}

/**
 * Returns how many bytes from the current data chunks were consumed so far.
 *
 * @param connp
 * @return The number of bytes consumed.
 */
size_t htp_connp_req_data_consumed(htp_connp_t *connp) {
    return connp->in_current_offset;
}

/**
 * Process a chunk of inbound (client or request) data.
 * 
 * @param connp
 * @param timestamp Optional.
 * @param data
 * @param len
 * @return STREAM_STATE_DATA, STREAM_STATE_ERROR or STEAM_STATE_DATA_OTHER (see QUICK_START).  STREAM_STATE_CLOSED and STREAM_STATE_TUNNEL are also possible.
 */
int htp_connp_req_data(htp_connp_t *connp, htp_time_t *timestamp, unsigned char *data, size_t len) {
    #ifdef HTP_DEBUG
    fprintf(stderr, "htp_connp_req_data(connp->in_status %x)\n", connp->in_status);
    fprint_raw_data(stderr, __FUNCTION__, data, len);
    #endif

    // Return if the connection is in stop state.
    if (connp->in_status == STREAM_STATE_STOP) {
        htp_log(connp, HTP_LOG_MARK, HTP_LOG_INFO, 0, "Inbound parser is in STREAM_STATE_STOP");

        return STREAM_STATE_STOP;
    }

    // Return if the connection had a fatal error earlier
    if (connp->in_status == STREAM_STATE_ERROR) {
        htp_log(connp, HTP_LOG_MARK, HTP_LOG_ERROR, 0, "Inbound parser is in STREAM_STATE_ERROR");

        #ifdef HTP_DEBUG
        fprintf(stderr, "htp_connp_req_data: returning STREAM_STATE_DATA (previous error)\n");
        #endif

        return STREAM_STATE_ERROR;
    }

    // If the length of the supplied data chunk is zero, proceed
    // only if the stream has been closed. We do not allow zero-sized
    // chunks in the API, but we use them internally to force the parsers
    // to finalize parsing.
    if ((len == 0) && (connp->in_status != STREAM_STATE_CLOSED)) {
        htp_log(connp, HTP_LOG_MARK, HTP_LOG_ERROR, 0, "Zero-length data chunks are not allowed");

        #ifdef HTP_DEBUG
        fprintf(stderr, "htp_connp_req_data: returning STREAM_STATE_DATA (zero-length chunk)\n");
        #endif

        return STREAM_STATE_CLOSED;
    }

    // Remember the timestamp of the current request data chunk
    if (timestamp != NULL) {
        memcpy(&connp->in_timestamp, timestamp, sizeof (*timestamp));
    }

    // Store the current chunk information    
    connp->in_current_data = data;
    connp->in_current_len = len;
    connp->in_current_offset = 0;
    connp->in_chunk_count++;
    connp->conn->in_data_counter += len;
    connp->conn->in_packet_counter++;

    // Return without processing any data if the stream is in tunneling
    // mode (which it would be after an initial CONNECT transaction).
    if (connp->in_status == STREAM_STATE_TUNNEL) {
        #ifdef HTP_DEBUG
        fprintf(stderr, "htp_connp_req_data: returning STREAM_STATE_TUNNEL\n");
        #endif

        return STREAM_STATE_TUNNEL;
    }

    if (connp->out_status == STREAM_STATE_DATA_OTHER) {
        connp->out_status = STREAM_STATE_DATA;
    }

    // Invoke a processor, in a loop, until an error
    // occurs or until we run out of data. Many processors
    // will process a request, each pointing to the next
    // processor that needs to run.
    for (;;) {
        #ifdef HTP_DEBUG
        fprintf(stderr, "htp_connp_req_data: in state=%s, progress=%s\n",
                htp_connp_in_state_as_string(connp),
                htp_tx_progress_as_string(connp->in_tx));
        #endif

        // Return if there's been an error
        // or if we've run out of data. We are relying
        // on processors to add error messages, so we'll
        // keep quiet here.
        int rc = connp->in_state(connp);
        if (rc == HTP_OK) {
            if (connp->in_status == STREAM_STATE_TUNNEL) {
                #ifdef HTP_DEBUG
                fprintf(stderr, "htp_connp_req_data: returning STREAM_STATE_TUNNEL\n");
                #endif

                return STREAM_STATE_TUNNEL;
            }
        } else {
            // Do we need more data?
            if (rc == HTP_DATA) {
                #ifdef HTP_DEBUG
                fprintf(stderr, "htp_connp_req_data: returning STREAM_STATE_DATA\n");
                #endif

                connp->in_status = STREAM_STATE_DATA;

                return STREAM_STATE_DATA;
            }

            // Check for suspended parsing
            if (rc == HTP_DATA_OTHER) {
                // We might have actually consumed the entire data chunk?
                if (connp->in_current_offset >= connp->in_current_len) {
                    // Do not send STREAM_DATE_DATA_OTHER if we've
                    // consumed the entire chunk
                    #ifdef HTP_DEBUG
                    fprintf(stderr, "htp_connp_req_data: returning STREAM_STATE_DATA (suspended parsing)\n");
                    #endif

                    connp->in_status = STREAM_STATE_DATA;

                    return STREAM_STATE_DATA;
                } else {
                    // Partial chunk consumption
                    #ifdef HTP_DEBUG
                    fprintf(stderr, "htp_connp_req_data: returning STREAM_STATE_DATA_OTHER\n");
                    #endif

                    connp->in_status = STREAM_STATE_DATA_OTHER;

                    return STREAM_STATE_DATA_OTHER;
                }
            }

            // Check for stop
            if (rc == HTP_STOP) {
                #ifdef HTP_DEBUG
                fprintf(stderr, "htp_connp_req_data: returning STREAM_STATE_STOP\n");
                #endif

                connp->in_status = STREAM_STATE_STOP;

                return STREAM_STATE_STOP;
            }

            // If we're here that means we've encountered an error.
            connp->in_status = STREAM_STATE_ERROR;

            #ifdef HTP_DEBUG
            fprintf(stderr, "htp_connp_req_data: returning STREAM_STATE_ERROR (state response)\n");
            #endif

            return STREAM_STATE_ERROR;
        }
    }
}
