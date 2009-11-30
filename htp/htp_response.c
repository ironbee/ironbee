
#include <stdlib.h>

#include "htp.h"

/**
 * Consumes bytes until the end of the current line.
 *
 * @param connp
 * @returns HTP_OK on state change, HTTP_ERROR on error, or HTP_DATA when more data is needed.
 */
int htp_connp_RES_BODY_CHUNKED_DATA_END(htp_connp_t *connp) {
    // TODO We shouldn't really see anything apart from CR and LF,
    // so we should warn about anything else.

    for (;;) {
        OUT_NEXT_BYTE_OR_RETURN(connp);

        connp->out_tx->request_message_len++;

        if (connp->out_next_byte == LF) {
            connp->out_state = htp_connp_RES_BODY_CHUNKED_LENGTH;

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
int htp_connp_RES_BODY_CHUNKED_DATA(htp_connp_t *connp) {
    htp_tx_data_t d;

    d.tx = connp->out_tx;
    d.data = &connp->out_current_data[connp->out_current_offset];
    d.len = 0;

    for (;;) {
        OUT_NEXT_BYTE_OR_RETURN(connp);

        if (connp->out_next_byte == -1) {
            // Send data to callbacks
            if (hook_run_all(connp->cfg->hook_response_body_data, &d) != HOOK_OK) {
                return HTP_ERROR;
            }

            // Ask for more data
            return HTP_DATA;
        } else {
            connp->out_tx->response_message_len++;
            connp->out_tx->response_entity_len++;
            connp->out_chunked_length--;
            d.len++;

            if (connp->out_chunked_length == 0) {
                // End of data chunk

                // Send data to callbacks
                if (hook_run_all(connp->cfg->hook_response_body_data, &d) != HOOK_OK) {
                    return HTP_ERROR;
                }

                connp->out_state = htp_connp_RES_BODY_CHUNKED_DATA_END;

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
int htp_connp_RES_BODY_CHUNKED_LENGTH(htp_connp_t *connp) {
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
                connp->out_state = htp_connp_RES_BODY_CHUNKED_DATA;
            } else if (connp->out_chunked_length == 0) {
                // End of data
                connp->out_state = htp_connp_RES_HEADERS;
                connp->out_tx->progress = TX_PROGRESS_RES_TRAILER;
            } else {
                // Invalid chunk length
                htp_log(connp, LOG_MARK, LOG_ERROR, 0,
                    "Response chunk encoding: Invalid chunk length: %i", connp->out_chunked_length);

                return HTP_ERROR;
            }

            return HTP_OK;
        }
    }
}

/**
 * Processes identity response body.
 *
 * @param connp
 * @returns HTP_OK on state change, HTTP_ERROR on error, or HTP_DATA when more data is needed.
 */
int htp_connp_RES_BODY_IDENTITY(htp_connp_t *connp) {
    htp_tx_data_t d;

    d.tx = connp->out_tx;
    d.data = &connp->out_current_data[connp->out_current_offset];
    d.len = 0;

    for (;;) {
        OUT_NEXT_BYTE(connp);       

        if (connp->out_next_byte == -1) {
            // End of chunk

            // Send data to callbacks
            if (d.len != 0) {
                if (hook_run_all(connp->cfg->hook_response_body_data, &d) != HOOK_OK) {
                    return HTP_ERROR;
                }
            }

            // If we don't know the length, then we must check
            // to see if the stream closed; that would signal the
            // end of the response body (and the end of the transaction).
            if ((connp->out_content_length == -1)&&(connp->out_status == STREAM_STATE_CLOSED)) {                
                connp->out_state = htp_connp_RES_IDLE;
                connp->out_tx->progress = TX_PROGRESS_WAIT;

                return HTP_OK;
           } else {
                // Ask for more data
                return HTP_DATA;
           }
        } else {
            connp->out_tx->response_message_len++;
            connp->out_tx->response_entity_len++;

            if (connp->out_body_data_left > 0) {
                // We know the length of response body

                connp->out_body_data_left--;
                d.len++;

                if (connp->out_body_data_left == 0) {
                    // End of body

                    // Send data to callbacks
                    if (d.len != 0) {
                        if (hook_run_all(connp->cfg->hook_response_body_data, &d) != HOOK_OK) {
                            return HTP_ERROR;
                        }
                    }

                    // Done
                    connp->out_state = htp_connp_RES_IDLE;
                    connp->out_tx->progress = TX_PROGRESS_WAIT;

                    return HTP_OK;
                }
            } else {
                // We don't know the length of the response body, which means
                // that the body will consume all data until the connection
                // is closed.
                //
                // We don't need to do anything here.
            }
        }
    }
}

/**
 * Determines presence (and encoding) of a response body.
 *
 * @param connp
 * @returns HTP_OK on state change, HTTP_ERROR on error, or HTP_DATA when more data is needed.
 */
int htp_connp_RES_BODY_DETERMINE(htp_connp_t *connp) {
    // First check for an interim "100 Continue"
    // response. Ignore it if found, and revert back to RES_FIRST_LINE.
    if (connp->out_tx->response_status_number == 100) {
        if (connp->out_tx->seen_100continue != 0) {
            htp_log(connp, LOG_MARK, LOG_ERROR, 0, "Already seen 100-Continue");
            return HTP_ERROR;
        }

        // Ignore any response headers set
        // XXX table_erase(connp->out_tx->response_headers);

        connp->out_state = htp_connp_RES_FIRST_LINE;
        connp->out_tx->progress = TX_PROGRESS_RES_LINE;
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
        || (connp->out_tx->request_method_number == M_HEAD)) {
        // There's no response body        
        connp->out_state = htp_connp_RES_IDLE;
    } else {
        // We have a response body

        htp_header_t *cl = table_getc(connp->out_tx->response_headers, "content-length");
        htp_header_t *te = table_getc(connp->out_tx->response_headers, "transfer-encoding");

        // 2. If a Transfer-Encoding header field (section 14.40) is present and
        //   indicates that the "chunked" transfer coding has been applied, then
        //   the length is defined by the chunked encoding (section 3.6).
        if (te != NULL) {
            // TODO Make sure it contains "chunked" only

            // If the T-E header is present we are going to use it.
            connp->out_tx->response_transfer_coding = CHUNKED;

            // We are still going to check for the presence of C-L
            if (cl != NULL) {
                // This is a violation of the RFC
                // TODO
            }

            connp->out_state = htp_connp_RES_BODY_CHUNKED_LENGTH;
            connp->out_tx->progress = TX_PROGRESS_RES_BODY;
        }
        // 3. If a Content-Length header field (section 14.14) is present, its
        //   value in bytes represents the length of the message-body.
        else if (cl != NULL) {
            // We know the exact length
            connp->out_tx->response_transfer_coding = IDENTITY;

            // Check for multiple C-L headers
            if (cl->flags & HTP_FIELD_REPEATED) {
                connp->out_tx->flags |= HTP_REQUEST_SMUGGLING;
                // TODO Log
            }

            // Get body length
            int i = htp_parse_content_length(cl->value);
            if (i < 0) {
                htp_log(connp, LOG_MARK, LOG_ERROR, 0, "Invalid C-L field in response");
                return HTP_ERROR;
            } else {
                connp->out_content_length = i;
                connp->out_body_data_left = connp->out_content_length;

                connp->out_state = htp_connp_RES_BODY_IDENTITY;
                connp->out_tx->progress = TX_PROGRESS_RES_BODY;
            }
        } else {
            // 4. If the message uses the media type "multipart/byteranges", which is
            //   self-delimiting, then that defines the length. This media type MUST
            //   NOT be used unless the sender knows that the recipient can parse it;
            //   the presence in a request of a Range header with multiple byte-range
            //   specifiers implies that the client can parse multipart/byteranges
            //   responses.
            htp_header_t *ct = table_getc(connp->out_tx->response_headers, "content-type");
            if (ct != NULL) {
                // TODO Handle multipart/byteranges

                if (bstr_indexofc_nocase(ct->value, "multipart/byteranges") != -1) {
                    htp_log(connp, LOG_MARK, LOG_ERROR, 0,
                        "C-T multipart/byteranges in responses not supported");
                    return HTP_ERROR;
                }
            }

            // 5. By the server closing the connection. (Closing the connection
            //   cannot be used to indicate the end of a request body, since that
            //   would leave no possibility for the server to send back a response.)
            connp->out_state = htp_connp_RES_BODY_IDENTITY;
            connp->out_tx->progress = TX_PROGRESS_RES_BODY;
        }
    }

    // NOTE We do not need to check for short-style HTTP/0.9 requests here because
    //      that is done earlier, before response line parsing begins

    // Run hook RESPONSE_HEADERS_COMPLETE
    if (hook_run_all(connp->cfg->hook_response_headers, connp) != HOOK_OK) {
        return HTP_ERROR;
    }

    return HTP_OK;
}

/**
 * Parses response headers.
 *
 * @param connp
 * @returns HTP_OK on state change, HTTP_ERROR on error, or HTP_DATA when more data is needed.
 */
int htp_connp_RES_HEADERS(htp_connp_t *connp) {
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
            // Should we terminate headers?
            if (htp_connp_is_line_terminator(connp, connp->out_line, connp->out_line_len)) {
                // Terminator line

                // Parse previous header, if any
                if (connp->out_header_line_index != -1) {
                    if (connp->cfg->process_response_header(connp) != HTP_OK) {
                        return HTP_ERROR;
                    }

                    // Reset index
                    connp->out_header_line_index = -1;
                }

                // Cleanup
                connp->out_line_len = 0;
                connp->out_header_line = NULL;
                connp->out_header_line_index = -1;
                connp->out_header_line_counter = 0;

                // We've seen all response headers
                if (connp->out_tx->progress == TX_PROGRESS_RES_HEADERS) {
                    // Determine if this response has a body
                    connp->out_state = htp_connp_RES_BODY_DETERMINE;
                } else {
                    // Run hook response_TRAILER
                    if (hook_run_all(connp->cfg->hook_response_trailer, connp) != HOOK_OK) {
                        return HTP_ERROR;
                    }

                    // We've completed parsing this response
                    connp->out_state = htp_connp_RES_IDLE;
                }

                return HTP_OK;
            }

            // Prepare line for consumption
            htp_chomp(connp->out_line, &connp->out_line_len);

            // Check for header folding
            if (htp_connp_is_line_folded(connp, connp->out_line, connp->out_line_len) == 0) {
                // New header line               

                // Parse previous header, if any
                if (connp->out_header_line_index != -1) {
                    if (connp->cfg->process_response_header(connp) != HTP_OK) {
                        return HTP_ERROR;
                    }

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
                        htp_log(connp, LOG_MARK, LOG_WARNING, 0, "Invalid response field folding");
                    }
                }
            }

            // Add the raw header line to the list
            connp->out_header_line->line = bstr_memdup(connp->out_line, connp->out_line_len);
            list_add(connp->out_tx->response_header_lines, connp->out_header_line);
            connp->out_header_line = 0;

            // Cleanup for the next line
            connp->out_line_len = 0;
            if (connp->out_header_line_index == -1) {
                connp->out_header_line_index = connp->out_header_line_counter;
            }
            connp->out_header_line_counter++;
        }
    }
}

/**
 * Parses response line.
 *
 * @param connp
 * @returns HTP_OK on state change, HTTP_ERROR on error, or HTP_DATA when more data is needed.
 */
int htp_connp_RES_FIRST_LINE(htp_connp_t *connp) {
    for (;;) {
        // Get one byte
        OUT_COPY_BYTE_OR_RETURN(connp);

        // Have we reached the end of the line?
        if (connp->out_next_byte == LF) {
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

            htp_chomp(connp->out_line, &connp->out_line_len);
            connp->out_tx->response_line = bstr_memdup(connp->out_line, connp->out_line_len);

            // Parse response line
            if (connp->cfg->parse_response_line(connp) != HTP_OK) {
                return HTP_ERROR;
            }

            // Run hook RESPONSE_LINE
            if (hook_run_all(connp->cfg->hook_response_line, connp) != HOOK_OK) {
                return HTP_ERROR;
            }

            // Clean up.
            connp->out_line_len = 0;

            // Move on to the next phase.
            connp->out_state = htp_connp_RES_HEADERS;
            connp->out_tx->progress = TX_PROGRESS_RES_HEADERS;

            return HTP_OK;
        }
    }
}

/**
 * The response idle state will initialize response processing, as well as
 * finalize each transactions after we are done with it.
 *
 * @param connp
 * @returns HTP_OK on state change, HTTP_ERROR on error, or HTP_DATA when more data is needed.
 */
int htp_connp_RES_IDLE(htp_connp_t * connp) {
    // If we're here and an outgoing transaction object exists that
    // means we've just completed parsing a response. We need
    // to run the final hook in a transaction and start over.
    if (connp->out_tx != NULL) {
        // Run hook RESPONSE
        if (hook_run_all(connp->cfg->hook_response, connp) != HOOK_OK) {
            return HTP_ERROR;
        }

        connp->out_tx->progress = TX_PROGRESS_DONE;

        // Start afresh
        connp->out_tx = NULL;
    }

    // We want to start parsing the next response (and change
    // the state from IDLE) only if there's at least one
    // byte of data available. Otherwise we could be creating
    // new structures even if there's no more data on the
    // connection.
    OUT_TEST_NEXT_BYTE_OR_RETURN(connp);

    // Parsing a new response

    // Find the next outgoing transaction
    connp->out_tx = list_get(connp->conn->transactions, connp->out_next_tx_index);
    if (connp->out_tx == NULL) {
        htp_log(connp, LOG_MARK, LOG_ERROR, 0,
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

    // Change state into response line parsing, except if we're following
    // a short HTTP/0.9 request, because such requests to not have a
    // response line and headers.
    if (connp->out_tx->protocol_is_simple) {
        connp->out_tx->response_transfer_coding = IDENTITY;
        connp->out_state = htp_connp_RES_BODY_IDENTITY;
        connp->out_tx->progress = TX_PROGRESS_RES_BODY;
    } else {
        connp->out_state = htp_connp_RES_FIRST_LINE;
        connp->out_tx->progress = TX_PROGRESS_RES_LINE;
    }

    return HTP_OK;
}

/**
 * Process a chunk of outbound (server or response) data.
 *
 * @param connp
 * @param timestamp
 * @param data
 * @param len
 * @return HTP_OK on state change, HTTP_ERROR on error, or HTP_DATA when more data is needed
 */
int htp_connp_res_data(htp_connp_t *connp, htp_time_t timestamp, unsigned char *data, size_t len) {
    // Return if the connection has had a fatal error
    if (connp->out_status != STREAM_STATE_OPEN) {
        // We allow calls that allow the parser to finalize their work
        if (!(connp->out_status == STREAM_STATE_CLOSED)&&(len == 0)) {
            return STREAM_STATE_ERROR;
        }
    }

    // Store the current chunk information
    connp->out_timestamp = timestamp;
    connp->out_current_data = data;
    connp->out_current_len = len;
    connp->out_current_offset = 0;

    // Invoke a processor, in a loop, until an error
    // occurs or until we run out of data. Many processors
    // will process a request, each pointing to the next
    // processor that needs to run.
    for (;;) {        
        // Return if there's been an error
        // or if we've run out of data. We are relying
        // on processors to add error messages, so we'll
        // keep quiet here.
        int rc = connp->out_state(connp);
        if (rc != HTP_OK) {
            // Do we need more data?
            if (rc == HTP_DATA) {
                return STREAM_STATE_DATA;
            }

            // Remember that we've had an error. Errors are
            // not possible to recover from.
            connp->out_status = STREAM_STATE_ERROR;

            return STREAM_STATE_ERROR;
        }
    }
}

