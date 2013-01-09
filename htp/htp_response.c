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

#include <stdlib.h>

#include "htp.h"
#include "htp_private.h"
#include "htp_transaction.h"

/**
 * Consumes bytes until the end of the current line.
 *
 * @param[in] connp
 * @returns HTP_OK on state change, HTP_ERROR on error, or HTP_DATA when more data is needed.
 */
htp_status_t htp_connp_RES_BODY_CHUNKED_DATA_END(htp_connp_t *connp) {
    // TODO We shouldn't really see anything apart from CR and LF,
    // so we should warn about anything else.

    for (;;) {
        OUT_NEXT_BYTE_OR_RETURN(connp);

        connp->out_tx->response_message_len++;

        if (connp->out_next_byte == LF) {
            connp->out_state = htp_connp_RES_BODY_CHUNKED_LENGTH;

            return HTP_OK;
        }
    }

    return HTP_ERROR;
}

/**
 * Processes a chunk of data.
 *
 * @param[in] connp
 * @returns HTP_OK on state change, HTP_ERROR on error, or HTP_DATA when more data is needed.
 */
htp_status_t htp_connp_RES_BODY_CHUNKED_DATA(htp_connp_t *connp) {
    const unsigned char *data = connp->out_current_data + connp->out_current_offset;
    size_t len = 0;

    for (;;) {
        OUT_NEXT_BYTE(connp);

        if (connp->out_next_byte == -1) {
            int rc = htp_tx_res_process_body_data(connp->out_tx, data, len);
            if (rc != HTP_OK) return rc;

            // Ask for more data
            return HTP_DATA;
        } else {            
            connp->out_chunked_length--;
            len++;

            if (connp->out_chunked_length == 0) {
                // End of data chunk
                int rc = htp_tx_res_process_body_data(connp->out_tx, data, len);
                if (rc != HTP_OK) return rc;

                connp->out_state = htp_connp_RES_BODY_CHUNKED_DATA_END;

                return HTP_OK;
            }
        }
    }

    return HTP_ERROR;
}

/**
 * Extracts chunk length.
 *
 * @param[in] connp
 * @returns HTP_OK on state change, HTP_ERROR on error, or HTP_DATA when more data is needed.
 */
htp_status_t htp_connp_RES_BODY_CHUNKED_LENGTH(htp_connp_t *connp) {
    for (;;) {
        OUT_COPY_BYTE_OR_RETURN(connp);

        connp->out_tx->response_message_len++;

        // Have we reached the end of the line?
        if (connp->out_next_byte == LF) {
            htp_chomp(connp->out_line, &connp->out_line_len);

            // Extract chunk length
            connp->out_chunked_length = htp_parse_chunked_length(connp->out_line, connp->out_line_len);

            // Cleanup for the next line
            connp->out_line_len = 0;

            // Handle chunk length
            if (connp->out_chunked_length > 0) {
                // More data available
                // TODO Add a check for chunk length
                connp->out_state = htp_connp_RES_BODY_CHUNKED_DATA;
            } else if (connp->out_chunked_length == 0) {
                // End of data
                connp->out_state = htp_connp_RES_HEADERS;
                connp->out_tx->progress = HTP_RESPONSE_TRAILER;
            } else {
                // Invalid chunk length
                htp_log(connp, HTP_LOG_MARK, HTP_LOG_ERROR, 0,
                        "Response chunk encoding: Invalid chunk length: %d", connp->out_chunked_length);
                return HTP_ERROR;
            }

            return HTP_OK;
        }
    }

    return HTP_ERROR;
}

/**
 * Processes identity response body.
 *
 * @param[in] connp
 * @returns HTP_OK on state change, HTP_ERROR on error, or HTP_DATA when more data is needed.
 */
htp_status_t htp_connp_RES_BODY_IDENTITY(htp_connp_t *connp) {
    unsigned char *data = connp->out_current_data + connp->out_current_offset;
    size_t len = 0;

    for (;;) {
        OUT_NEXT_BYTE(connp);

        if (connp->out_next_byte == -1) {
            // End of chunk

            // Send data to callbacks
            if (len != 0) {
                int rc = htp_tx_res_process_body_data(connp->out_tx, data, len);
                if (rc != HTP_OK) return rc;
            }

            // If we don't know the length, then we must check
            // to see if the stream closed; that would signal the
            // end of the response body (and the end of the transaction).
            if ((connp->out_content_length == -1) && (connp->out_status == HTP_STREAM_CLOSED)) {
                connp->out_state = htp_connp_RES_FINALIZE;

                return HTP_OK;
            } else {
                // Ask for more data
                return HTP_DATA;
            }
        } else {            
            if (connp->out_body_data_left > 0) {
                // We know the length of response body

                connp->out_body_data_left--;
                len++;

                if (connp->out_body_data_left == 0) {
                    // End of body

                    // Send data to callbacks
                    if (len != 0) {
                        int rc = htp_tx_res_process_body_data(connp->out_tx, data, len);
                        if (rc != HTP_OK) return rc;                        
                    }

                    // Done
                    connp->out_state = htp_connp_RES_FINALIZE;

                    return HTP_OK;
                }
            } else {
                // We don't know the length of the response body, which means
                // that the body will consume all data until the connection
                // is closed.
                len++;
            }
        }
    }

    return HTP_ERROR;
}

/**
 * Determines presence (and encoding) of a response body.
 *
 * @param[in] connp
 * @returns HTP_OK on state change, HTP_ERROR on error, or HTP_DATA when more data is needed.
 */
htp_status_t htp_connp_RES_BODY_DETERMINE(htp_connp_t *connp) {
    // If the request uses the CONNECT method, then not only are we
    // to assume there's no body, but we need to ignore all
    // subsequent data in the stream.
    if (connp->out_tx->request_method_number == HTP_M_CONNECT) {
        if ((connp->out_tx->response_status_number >= 200)
                && (connp->out_tx->response_status_number <= 299)) {
            // This is a successful CONNECT stream, which means
            // we need to switch into tunnelling mode.
            connp->in_status = HTP_STREAM_TUNNEL;
            connp->out_status = HTP_STREAM_TUNNEL;
            connp->out_state = htp_connp_RES_FINALIZE;
            return HTP_OK;
        } else {
            // This is a failed CONNECT stream, which means that
            // we can unblock request parsing
            connp->in_status = HTP_STREAM_DATA;

            // We are going to continue processing this transaction,
            // adding a note for ourselves to stop at the end (because
            // we don't want to see the beginning of a new transaction).
            connp->out_data_other_at_tx_end = 1;
        }
    }

    // Check for an interim "100 Continue"
    // response. Ignore it if found, and revert back to RES_FIRST_LINE.
    if (connp->out_tx->response_status_number == 100) {
        if (connp->out_tx->seen_100continue != 0) {
            htp_log(connp, HTP_LOG_MARK, HTP_LOG_ERROR, 0, "Already seen 100-Continue");
            return HTP_ERROR;
        }

        // Ignore any response headers seen so far.
        htp_header_t *h = NULL;
        for (int i = 0, n = htp_table_size(connp->out_tx->response_headers); i < n; i++) {
            htp_table_get_index(connp->out_tx->response_headers, i, NULL, (void **) &h);
            bstr_free(&h->name);
            bstr_free(&h->value);
            free(h);
        }

        htp_table_clear(connp->out_tx->response_headers);

        // Expecting to see another response line next.
        connp->out_state = htp_connp_RES_LINE;
        connp->out_tx->progress = HTP_RESPONSE_LINE;
        connp->out_tx->seen_100continue++;

        return HTP_OK;
    }   

    // 1. Any response message which MUST NOT include a message-body
    //  (such as the 1xx, 204, and 304 responses and any response to a HEAD
    //  request) is always terminated by the first empty line after the
    //  header fields, regardless of the entity-header fields present in the
    //  message.
    if (((connp->out_tx->response_status_number >= 100) && (connp->out_tx->response_status_number <= 199))
            || (connp->out_tx->response_status_number == 204) || (connp->out_tx->response_status_number == 304)
            || (connp->out_tx->request_method_number == HTP_M_HEAD)) {
        // There's no response body
        connp->out_tx->response_transfer_coding = HTP_CODING_NO_BODY;
        connp->out_state = htp_connp_RES_FINALIZE;
    } else {
        // We have a response body

        htp_header_t *ct = htp_table_get_c(connp->out_tx->response_headers, "content-type");
        htp_header_t *cl = htp_table_get_c(connp->out_tx->response_headers, "content-length");
        htp_header_t *te = htp_table_get_c(connp->out_tx->response_headers, "transfer-encoding");

        if (ct != NULL) {
            connp->out_tx->response_content_type = bstr_dup_lower(ct->value);
            if (connp->out_tx->response_content_type == NULL) return HTP_ERROR;

            // Ignore parameters
            unsigned char *data = bstr_ptr(connp->out_tx->response_content_type);
            size_t len = bstr_len(ct->value);
            size_t newlen = 0;
            while (newlen < len) {
                // TODO Some platforms may do things differently here
                if (htp_is_space(data[newlen]) || (data[newlen] == ';')) {
                    bstr_adjust_len(connp->out_tx->response_content_type, newlen);
                    break;
                }

                newlen++;
            }
        }

        // 2. If a Transfer-Encoding header field (section 14.40) is present and
        //   indicates that the "chunked" transfer coding has been applied, then
        //   the length is defined by the chunked encoding (section 3.6).
        if ((te != NULL) && (bstr_cmp_c(te->value, "chunked") == 0)) {
            // If the T-E header is present we are going to use it.
            connp->out_tx->response_transfer_coding = HTP_CODING_CHUNKED;

            // We are still going to check for the presence of C-L
            if (cl != NULL) {
                // This is a violation of the RFC
                connp->out_tx->flags |= HTP_REQUEST_SMUGGLING;
                // TODO
            }

            connp->out_state = htp_connp_RES_BODY_CHUNKED_LENGTH;
            connp->out_tx->progress = HTP_RESPONSE_BODY;
        }
        // 3. If a Content-Length header field (section 14.14) is present, its
        //   value in bytes represents the length of the message-body.
        else if (cl != NULL) {
            // We know the exact length
            connp->out_tx->response_transfer_coding = HTP_CODING_IDENTITY;

            // Check for multiple C-L headers
            if (cl->flags & HTP_FIELD_REPEATED) {
                connp->out_tx->flags |= HTP_REQUEST_SMUGGLING;
                // TODO Log
            }

            // Get body length
            int i = htp_parse_content_length(cl->value);
            if (i < 0) {
                htp_log(connp, HTP_LOG_MARK, HTP_LOG_ERROR, 0, "Invalid C-L field in response: %d", i);
                return HTP_ERROR;
            } else {
                connp->out_content_length = i;
                connp->out_body_data_left = connp->out_content_length;

                if (connp->out_content_length != 0) {
                    connp->out_state = htp_connp_RES_BODY_IDENTITY;
                    connp->out_tx->progress = HTP_RESPONSE_BODY;
                } else {
                    connp->out_state = htp_connp_RES_FINALIZE;
                }
            }
        } else {
            // 4. If the message uses the media type "multipart/byteranges", which is
            //   self-delimiting, then that defines the length. This media type MUST
            //   NOT be used unless the sender knows that the recipient can parse it;
            //   the presence in a request of a Range header with multiple byte-range
            //   specifiers implies that the client can parse multipart/byteranges
            //   responses.
            if (ct != NULL) {
                // TODO Handle multipart/byteranges
                if (bstr_index_of_c_nocase(ct->value, "multipart/byteranges") != -1) {
                    htp_log(connp, HTP_LOG_MARK, HTP_LOG_ERROR, 0,
                            "C-T multipart/byteranges in responses not supported");
                    return HTP_ERROR;
                }

                // TODO Set connp->out_tx->response_transfer_coding
            }

            // 5. By the server closing the connection. (Closing the connection
            //   cannot be used to indicate the end of a request body, since that
            //   would leave no possibility for the server to send back a response.)
            connp->out_state = htp_connp_RES_BODY_IDENTITY;
            connp->out_tx->response_transfer_coding = HTP_CODING_IDENTITY;
            connp->out_tx->progress = HTP_RESPONSE_BODY;
        }
    }

    // NOTE We do not need to check for short-style HTTP/0.9 requests here because
    //      that is done earlier, before response line parsing begins

    int rc = htp_tx_state_response_headers(connp->out_tx);
    if (rc != HTP_OK) return rc;    

    return HTP_OK;
}

/**
 * Parses response headers.
 *
 * @param[in] connp
 * @returns HTP_OK on state change, HTP_ERROR on error, or HTP_DATA when more data is needed.
 */
htp_status_t htp_connp_RES_HEADERS(htp_connp_t * connp) {
    for (;;) {
        OUT_COPY_BYTE_OR_RETURN(connp);

        if (connp->out_header_line == NULL) {
            connp->out_header_line = calloc(1, sizeof (htp_header_line_t));
            if (connp->out_header_line == NULL) return HTP_ERROR;
            connp->out_header_line->first_nul_offset = -1;
        }

        // Keep track of NUL bytes
        if (connp->out_next_byte == 0) {
            // Store the offset of the first NUL
            if (connp->out_header_line->has_nulls == 0) {
                connp->out_header_line->first_nul_offset = connp->out_line_len;
            }

            // Remember how many NULs there were
            connp->out_header_line->flags |= HTP_FIELD_NUL_BYTE;
            connp->out_header_line->has_nulls++;
        }

        // Have we reached the end of the line?
        if (connp->out_next_byte == LF) {
            #ifdef HTP_DEBUG
            fprint_raw_data(stderr, __FUNCTION__, connp->out_line, connp->out_line_len);
            #endif

            // Should we terminate headers?
            if (htp_connp_is_line_terminator(connp, connp->out_line, connp->out_line_len)) {
                // Terminator line
                if (connp->out_tx->response_headers_sep == NULL) {
                    connp->out_tx->response_headers_sep = bstr_dup_mem(connp->out_line, connp->out_line_len);
                    if (connp->out_tx->response_headers_sep == NULL) {
                        return HTP_ERROR;
                    }
                }

                // Parse previous header, if any
                if (connp->out_header_line_index != -1) {
                    // Only try to parse a header, but ignore
                    // any problems. That's what browsers do.
                    connp->cfg->process_response_header(connp);

                    // Reset index
                    connp->out_header_line_index = -1;
                }

                // Cleanup
                free(connp->out_header_line);
                connp->out_line_len = 0;
                connp->out_header_line = NULL;

                // We've seen all response headers
                if (connp->out_tx->progress == HTP_RESPONSE_HEADERS) {
                    // Determine if this response has a body
                    connp->out_state = htp_connp_RES_BODY_DETERMINE;
                } else {
                    // Run hook response_TRAILER
                    int rc = htp_hook_run_all(connp->cfg->hook_response_trailer, connp);
                    if (rc != HTP_OK) return rc;

                    // We've completed parsing this response
                    connp->out_state = htp_connp_RES_FINALIZE;
                }

                return HTP_OK;
            }

            // Prepare line for consumption
            int chomp_result = htp_chomp(connp->out_line, &connp->out_line_len);

            // Check for header folding
            if (htp_connp_is_line_folded(connp->out_line, connp->out_line_len) == 0) {
                // New header line

                // Parse previous header, if any
                if (connp->out_header_line_index != -1) {
                    // Only try to parse a header, but ignore
                    // any problems. That's what browsers do.
                    connp->cfg->process_response_header(connp);

                    // Reset index
                    connp->out_header_line_index = -1;
                }

                // Remember the index of the fist header line
                connp->out_header_line_index = connp->out_header_line_counter;
            } else {
                // Folding; check that there's a previous header line to add to
                if (connp->out_header_line_index == -1) {
                    if (!(connp->out_tx->flags & HTP_INVALID_FOLDING)) {
                        connp->out_tx->flags |= HTP_INVALID_FOLDING;
                        htp_log(connp, HTP_LOG_MARK, HTP_LOG_WARNING, 0, "Invalid response field folding");
                    }
                }
            }

            // Add the raw header line to the list
            connp->out_header_line->line = bstr_dup_mem(connp->out_line, connp->out_line_len + chomp_result);
            if (connp->out_header_line->line == NULL) {
                return HTP_ERROR;
            }

            htp_list_add(connp->out_tx->response_header_lines, connp->out_header_line);
            connp->out_header_line = NULL;

            // Cleanup for the next line
            connp->out_line_len = 0;
            if (connp->out_header_line_index == -1) {

                connp->out_header_line_index = connp->out_header_line_counter;
            }

            connp->out_header_line_counter++;
        }
    }

    return HTP_ERROR;
}

/**
 * Parses response line.
 *
 * @param[in] connp
 * @returns HTP_OK on state change, HTP_ERROR on error, or HTP_DATA when more data is needed.
 */
htp_status_t htp_connp_RES_LINE(htp_connp_t * connp) {
    for (;;) {
        // Get one byte
        OUT_COPY_BYTE_OR_RETURN(connp);

        // Have we reached the end of the line?
        if (connp->out_next_byte == LF) {
            #ifdef HTP_DEBUG
            fprint_raw_data(stderr, __FUNCTION__, connp->out_line, connp->out_line_len);
            #endif

            // Is this a line that should be ignored?
            if (htp_connp_is_line_ignorable(connp, connp->out_line, connp->out_line_len)) {
                // We have an empty/whitespace line, which we'll note, ignore and move on
                connp->out_tx->response_ignored_lines++;

                // TODO How many lines are we willing to accept?

                // Start again
                connp->out_line_len = 0;

                return HTP_OK;
            }

            // Process response line

            // Deallocate previous response line allocations, which we would have on a 100 response            
            if (connp->out_tx->response_line != NULL) {
                bstr_free(&connp->out_tx->response_line);
            }

            if (connp->out_tx->response_protocol != NULL) {
                bstr_free(&connp->out_tx->response_protocol);
            }

            if (connp->out_tx->response_status != NULL) {
                bstr_free(&connp->out_tx->response_status);
            }

            if (connp->out_tx->response_message != NULL) {
                bstr_free(&connp->out_tx->response_message);
            }

            connp->out_tx->response_line_raw = bstr_dup_mem(connp->out_line, connp->out_line_len);
            if (connp->out_tx->response_line_raw == NULL) {
                return HTP_ERROR;
            }

            int chomp_result = htp_chomp(connp->out_line, &connp->out_line_len);
            connp->out_tx->response_line = bstr_dup_ex(connp->out_tx->response_line_raw, 0, connp->out_line_len);
            if (connp->out_tx->response_line == NULL) {
                return HTP_ERROR;
            }

            // Parse response line
            if (connp->cfg->parse_response_line(connp) != HTP_OK) {
                // Note: downstream responsible for error logging
                return HTP_ERROR;
            }

            // If the response line is invalid, determine if it _looks_ like
            // a response line. If it does not look like a line, process the
            // data as a response body because that is what browsers do.
            if (htp_treat_response_line_as_body(connp->out_tx)) {
                int rc = htp_tx_res_process_body_data(connp->out_tx, connp->out_line, connp->out_line_len + chomp_result);
                if (rc != HTP_OK) return rc;

                // Continue to process response body
                connp->out_tx->response_transfer_coding = HTP_CODING_IDENTITY;
                connp->out_state = htp_connp_RES_BODY_IDENTITY;
                connp->out_tx->progress = HTP_RESPONSE_BODY;

                return HTP_OK;
            }

            int rc = htp_tx_state_response_line(connp->out_tx);
            if (rc != HTP_OK) return rc;

            // Clean up.
            connp->out_line_len = 0;

            // Move on to the next phase.
            connp->out_state = htp_connp_RES_HEADERS;
            connp->out_tx->progress = HTP_RESPONSE_HEADERS;

            return HTP_OK;
        }
    }

    return HTP_ERROR;
}

size_t htp_connp_res_data_consumed(htp_connp_t * connp) {
    return connp->out_current_offset;
}

htp_status_t htp_connp_RES_FINALIZE(htp_connp_t * connp) {
    int rc = htp_tx_state_response_complete(connp->out_tx);
    if (rc != HTP_OK) return rc;

    // XXX Document when the response parser needs to yield to the request
    //     parser.

    // Check if the inbound parser is waiting on us. If it is that means that
    // there might be request data that the inbound parser hasn't consumed yet.
    // If we don't stop parsing we might encounter a response without a request.
    if ((connp->in_status == HTP_STREAM_DATA_OTHER) && (connp->in_tx == connp->out_tx)) {
        return HTP_DATA_OTHER;
    }

    // Do we have a signal to yield to inbound processing at
    // the end of the next transaction?
    if (connp->out_data_other_at_tx_end) {
        // We do. Let's yield then.
        connp->out_data_other_at_tx_end = 0;
        return HTP_DATA_OTHER;
    }

    // In streaming processing, we destroy the transaction
    // because it will not be needed any more.
    if (connp->cfg->tx_auto_destroy) {
        htp_tx_destroy(connp->out_tx);
    }

    // Disconnect from the transaction
    connp->out_tx = NULL;

    connp->out_state = htp_connp_RES_IDLE;

    return HTP_OK;
}

/**
 * The response idle state will initialize response processing, as well as
 * finalize each transactions after we are done with it.
 *
 * @param[in] connp
 * @returns HTP_OK on state change, HTP_ERROR on error, or HTP_DATA when more data is needed.
 */
htp_status_t htp_connp_RES_IDLE(htp_connp_t * connp) {
    // We want to start parsing the next response (and change
    // the state from IDLE) only if there's at least one
    // byte of data available. Otherwise we could be creating
    // new structures even if there's no more data on the
    // connection.
    OUT_TEST_NEXT_BYTE_OR_RETURN(connp);

    // Parsing a new response

    // Find the next outgoing transaction
    connp->out_tx = htp_list_get(connp->conn->transactions, connp->out_next_tx_index);
    if (connp->out_tx == NULL) {
        htp_log(connp, HTP_LOG_MARK, HTP_LOG_ERROR, 0,
                "Unable to match response to request");
        return HTP_ERROR;
    }

    // We've used one transaction
    connp->out_next_tx_index++;

    // TODO Detect state mismatch

    connp->out_content_length = -1;
    connp->out_body_data_left = -1;
    connp->out_header_line_index = -1;
    connp->out_header_line_counter = 0;

    int rc = htp_tx_state_response_start(connp->out_tx);
    if (rc != HTP_OK) return rc;

    return HTP_OK;
}

int htp_connp_res_data(htp_connp_t *connp, htp_time_t *timestamp, unsigned char *data, size_t len) {
    #ifdef HTP_DEBUG
    fprintf(stderr, "htp_connp_res_data(connp->out_status %x)\n", connp->out_status);
    fprint_raw_data(stderr, __FUNCTION__, data, len);
    #endif

    // Return if the connection is in stop state
    if (connp->out_status == HTP_STREAM_STOP) {
        htp_log(connp, HTP_LOG_MARK, HTP_LOG_INFO, 0, "Outbound parser is in HTP_STREAM_STOP");

        return HTP_STREAM_STOP;
    }

    // Return if the connection has had a fatal error
    if (connp->out_status == HTP_STREAM_ERROR) {
        htp_log(connp, HTP_LOG_MARK, HTP_LOG_ERROR, 0, "Outbound parser is in HTP_STREAM_ERROR");

        #ifdef HTP_DEBUG
        fprintf(stderr, "htp_connp_res_data: returning HTP_STREAM_DATA (previous error)\n");
        #endif

        return HTP_STREAM_ERROR;
    }

    // If the length of the supplied data chunk is zero, proceed
    // only if the stream has been closed. We do not allow zero-sized
    // chunks in the API, but we use it internally to force the parsers
    // to finalize parsing.
    if ((len == 0) && (connp->out_status != HTP_STREAM_CLOSED)) {
        htp_log(connp, HTP_LOG_MARK, HTP_LOG_ERROR, 0, "Zero-length data chunks are not allowed");

        #ifdef HTP_DEBUG
        fprintf(stderr, "htp_connp_res_data: returning HTP_STREAM_DATA (zero-length chunk)\n");
        #endif

        return HTP_STREAM_CLOSED;
    }

    // Remember the timestamp of the current response data chunk
    if (timestamp != NULL) {
        memcpy(&connp->out_timestamp, timestamp, sizeof (*timestamp));
    }

    // Store the current chunk information
    connp->out_current_data = data;
    connp->out_current_len = len;
    connp->out_current_offset = 0;

    htp_conn_track_outbound_data(connp->conn, len, timestamp);

    // Return without processing any data if the stream is in tunneling
    // mode (which it would be after an initial CONNECT transaction.
    if (connp->out_status == HTP_STREAM_TUNNEL) {
        #ifdef HTP_DEBUG
        fprintf(stderr, "htp_connp_res_data: returning HTP_STREAM_TUNNEL\n");
        #endif

        return HTP_STREAM_TUNNEL;
    }

    // Invoke a processor, in a loop, until an error
    // occurs or until we run out of data. Many processors
    // will process a request, each pointing to the next
    // processor that needs to run.
    for (;;) {
        #ifdef HTP_DEBUG
        fprintf(stderr, "htp_connp_res_data: out state=%s, progress=%s\n",
                htp_connp_out_state_as_string(connp),
                htp_tx_progress_as_string(connp->out_tx));
        #endif

        // Return if there's been an error
        // or if we've run out of data. We are relying
        // on processors to add error messages, so we'll
        // keep quiet here.
        int rc = connp->out_state(connp);
        if (rc == HTP_OK) {
            if (connp->out_status == HTP_STREAM_TUNNEL) {
                #ifdef HTP_DEBUG
                fprintf(stderr, "htp_connp_res_data: returning HTP_STREAM_TUNNEL\n");
                #endif

                return HTP_STREAM_TUNNEL;
            }
        } else {
            // Do we need more data?
            if (rc == HTP_DATA) {
                #ifdef HTP_DEBUG
                fprintf(stderr, "htp_connp_res_data: returning HTP_STREAM_DATA\n");
                #endif

                connp->out_status = HTP_STREAM_DATA;

                return HTP_STREAM_DATA;
            }

            // Check for stop
            if (rc == HTP_STOP) {
                #ifdef HTP_DEBUG
                fprintf(stderr, "htp_connp_res_data: returning HTP_STREAM_STOP\n");
                #endif

                connp->out_status = HTP_STREAM_STOP;

                return HTP_STREAM_STOP;
            }

            // Check for suspended parsing
            if (rc == HTP_DATA_OTHER) {
                // We might have actually consumed the entire data chunk?
                if (connp->out_current_offset >= connp->out_current_len) {
                    #ifdef HTP_DEBUG
                    fprintf(stderr, "htp_connp_res_data: returning HTP_STREAM_DATA (suspended parsing)\n");
                    #endif

                    connp->out_status = HTP_STREAM_DATA;

                    // Do not send STREAM_DATE_DATA_OTHER if we've
                    // consumed the entire chunk
                    return HTP_STREAM_DATA;
                } else {
                    #ifdef HTP_DEBUG
                    fprintf(stderr, "htp_connp_res_data: returning HTP_STREAM_DATA_OTHER\n");
                    #endif

                    connp->out_status = HTP_STREAM_DATA_OTHER;

                    // Partial chunk consumption
                    return HTP_STREAM_DATA_OTHER;
                }
            }

            #ifdef HTP_DEBUG
            fprintf(stderr, "htp_connp_res_data: returning HTP_STREAM_ERROR\n");
            #endif

            // Remember that we've had an error. Errors are
            // not possible to recover from.
            connp->out_status = HTP_STREAM_ERROR;

            return HTP_STREAM_ERROR;
        }
    }

    return HTP_STREAM_ERROR;
}
