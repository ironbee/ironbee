
#include <stdlib.h>

#include "htp.h"

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
            // Send data to callbacks            
            if (hook_run_all(connp->cfg->hook_request_body_data, &d) != HOOK_OK) {
                return HTP_ERROR;
            }

            // Ask for more data
            return HTP_DATA;
        } else {
            connp->in_tx->request_message_len++;
            connp->in_tx->request_entity_len++;
            connp->in_chunked_length--;
            d.len++;

            if (connp->in_chunked_length == 0) {
                // End of data chunk

                // Send data to callbacks
                if (hook_run_all(connp->cfg->hook_request_body_data, &d) != HOOK_OK) {
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
                connp->in_state = htp_connp_REQ_BODY_CHUNKED_DATA;
            } else if (connp->in_chunked_length == 0) {
                // End of data
                connp->in_state = htp_connp_REQ_HEADERS;
                connp->in_tx->progress = TX_PROGRESS_REQ_TRAILER;
            } else {
                // Invalid chunk length
                htp_log(connp, LOG_MARK, LOG_ERROR, 0,
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

            if (d.len != 0) {
                // Send data to callbacks
                if (hook_run_all(connp->cfg->hook_request_body_data, &d) != HOOK_OK) {
                    return HTP_ERROR;
                }
            }

            // Ask for more data
            return HTP_DATA;
        } else {
            connp->in_tx->request_message_len++;
            connp->in_tx->request_entity_len++;
            connp->in_body_data_left--;
            d.len++;

            if (connp->in_body_data_left == 0) {
                // End of body

                if (d.len != 0) {
                    // Send data to callbacks
                    if (hook_run_all(connp->cfg->hook_request_body_data, &d) != HOOK_OK) {
                        return HTP_ERROR;
                    }
                }

                // Done
                connp->in_state = htp_connp_REQ_IDLE;
                connp->in_tx->progress = TX_PROGRESS_WAIT;

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
    htp_header_t *cl = table_getc(connp->in_tx->request_headers, "content-length");
    htp_header_t *te = table_getc(connp->in_tx->request_headers, "transfer-encoding");

    // First check for the Transfer-Encoding header, which
    // would indicate a chunked request body
    if (te != NULL) {
        // TODO Make sure it contains "chunked" only

        // Chunked encoding is a HTTP/1.1 feature. Check
        // that some other protocol is not used. The flag will
        // also be set if the protocol could not be parsed.
        //
        // TODO IIS 7.5, for example, would ignore the T-E header when it
        //      it is used with a protocol below HTTP 1.1.
        if (connp->in_tx->request_protocol_number < HTTP_1_1) {
            connp->in_tx->flags |= HTP_INVALID_CHUNKING;
            // TODO Log
        }

        // If the T-E header is present we are going to use it.
        connp->in_tx->request_transfer_coding = CHUNKED;

        // We are still going to check for the presence of C-L
        if (cl != NULL) {
            // This is a violation of the RFC
            connp->in_tx->flags |= HTP_REQUEST_SMUGGLING;
            // XXX
        }

        connp->in_state = htp_connp_REQ_BODY_CHUNKED_LENGTH;
        connp->in_tx->progress = TX_PROGRESS_REQ_BODY;
    } else
        // Next check for the presence of the Content-Length header
        if (cl != NULL) {
        // It seems that we have a request body.
        connp->in_tx->request_transfer_coding = IDENTITY;

        // Check for a folded C-L header
        if (cl->flags & HTP_FIELD_FOLDED) {
            connp->in_tx->flags |= HTP_REQUEST_SMUGGLING;
            // XXX
        }

        // Check for multiple C-L headers
        if (cl->flags & HTP_FIELD_REPEATED) {
            connp->in_tx->flags |= HTP_REQUEST_SMUGGLING;
            // XXX
        }

        // Get body length
        int i = htp_parse_content_length(cl->value);
        if (i < 0) {
            htp_log(connp, LOG_MARK, LOG_ERROR, 0, "Invalid C-L field in request");
            return HTP_ERROR;
        } else {
            connp->in_content_length = i;
            connp->in_body_data_left = connp->in_content_length;

            connp->in_state = htp_connp_REQ_BODY_IDENTITY;
            connp->in_tx->progress = TX_PROGRESS_REQ_BODY;
        }
    } else {
        // This request does not have a body, which
        // means that we're done with it
        connp->in_state = htp_connp_REQ_IDLE;
        connp->in_tx->progress = TX_PROGRESS_WAIT;
    }

    // Host resolution    
    htp_header_t *h = table_getc(connp->in_tx->request_headers, "host");
    if (h == NULL) {
        // No host information in the headers

        // HTTP/1.1 requires host information in the headers
        if (connp->in_tx->request_protocol_number >= HTTP_1_1) {
            connp->in_tx->flags |= HTP_HOST_MISSING;
            htp_log(connp, LOG_MARK, LOG_WARNING, 0, "Host information in request headers required by HTTP/1.1");
        }
    } else {
        // Host information available in the headers

        // Is there host information in the URI?
        if (connp->in_tx->parsed_uri->hostname == NULL) {
            // There is no host information in the URI. Place the
            // hostname from the headers into the parsed_uri structure.
            htp_replace_hostname(connp, connp->in_tx->parsed_uri, h->value);
        } else {
            // The host information is present both in the
            // headers and the URI. The HTTP RFC states that
            // we should ignore the headers copy.
            connp->in_tx->flags |= HTP_AMBIGUOUS_HOST;
            htp_log(connp, LOG_MARK, LOG_WARNING, 0, "Host information ambiguous");
        }
    }

    // Run hook REQUEST_HEADERS
    if (hook_run_all(connp->cfg->hook_request_headers, connp) != HOOK_OK) {
        return HTP_ERROR;
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
            // Should we terminate headers?
            if (htp_connp_is_line_terminator(connp, connp->in_line, connp->in_line_len)) {
                // Terminator line

                // Parse previous header, if any
                if (connp->in_header_line_index != -1) {
                    if (connp->cfg->process_request_header(connp) != HTP_OK) {
                        return HTP_ERROR;
                    }

                    // Reset index
                    connp->in_header_line_index = -1;
                }

                // Cleanup
                connp->in_line_len = 0;
                connp->in_header_line = NULL;
                connp->in_header_line_index = -1;
                connp->in_header_line_counter = 0;

                // We've seen all request headers
                if (connp->in_chunk_count != connp->in_chunk_request_index) {
                    connp->in_tx->flags |= HTP_MULTI_PACKET_HEAD;
                }

                // Move onto the next processing phase
                if (connp->in_tx->progress == TX_PROGRESS_REQ_HEADERS) {
                    // Determine if this request has a body
                    connp->in_state = htp_connp_REQ_BODY_DETERMINE;
                } else {
                    // Run hook REQUEST_TRAILER
                    if (hook_run_all(connp->cfg->hook_request_trailer, connp) != HOOK_OK) {
                        return HTP_ERROR;
                    }

                    // We've completed parsing this request
                    connp->in_state = htp_connp_REQ_IDLE;
                    connp->in_tx->progress = TX_PROGRESS_WAIT;
                }

                return HTP_OK;
            }

            // Prepare line for consumption
            htp_chomp(connp->in_line, &connp->in_line_len);

            // Check for header folding
            if (htp_connp_is_line_folded(connp, connp->in_line, connp->in_line_len) == 0) {
                // New header line

                // Parse previous header, if any
                if (connp->in_header_line_index != -1) {
                    if (connp->cfg->process_request_header(connp) != HTP_OK) {
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
                        htp_log(connp, LOG_MARK, LOG_WARNING, 0, "Invalid request field folding");
                    }
                }
            }

            // Add the raw header line to the list
            connp->in_header_line->line = bstr_memdup(connp->in_line, connp->in_line_len);
            list_add(connp->in_tx->request_header_lines, connp->in_header_line);
            connp->in_header_line = 0;

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
        connp->in_state = htp_connp_REQ_IDLE;
        connp->in_tx->progress = TX_PROGRESS_WAIT;
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

            htp_chomp(connp->in_line, &connp->in_line_len);
            connp->in_tx->request_line = bstr_memdup(connp->in_line, connp->in_line_len);

            // Parse request line
            if (connp->cfg->parse_request_line(connp) != HTP_OK) {
                return HTP_ERROR;
            }

            // Parse the request URI            
            if (htp_parse_uri(connp->in_tx->request_uri, &(connp->in_tx->parsed_uri_incomplete)) != HTP_OK) {
                return HTP_ERROR;
            }

            // Keep the original URI components, but 
            // create a copy which we can normalize
            if (htp_normalize_parsed_uri(connp, connp->in_tx->parsed_uri_incomplete, connp->in_tx->parsed_uri)) {
                return HTP_ERROR;
            }

            // Run hook REQUEST_LINE
            if (hook_run_all(connp->cfg->hook_request_line, connp) != HOOK_OK) {
                return HTP_ERROR;
            }

            // Clean up.
            connp->in_line_len = 0;

            // Move on to the next phase.
            connp->in_state = htp_connp_REQ_PROTOCOL;

            return HTP_OK;
        }
    }
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
    // If we're here and a transaction object exists that
    // means we've just completed parsing a request. We need
    // to run the final hook and start over.
    if (connp->in_tx != NULL) {
        // Run hook REQUEST
        if (hook_run_all(connp->cfg->hook_request, connp) != HOOK_OK) {
            return HTP_ERROR;
        }

        // Start afresh
        connp->in_tx = NULL;
    }

    // We want to start parsing the next request (and change
    // the state from IDLE) only if there's at least one
    // byte of data available. Otherwise we could be creating
    // new structures even if there's no more data on the
    // connection.
    IN_TEST_NEXT_BYTE_OR_RETURN(connp);

    // Detect pipelining
    if (list_size(connp->conn->transactions) > connp->out_next_tx_index) {
        connp->conn->flags |= PIPELINED_CONNECTION;
    }

    // Parsing a new request
    connp->in_tx = htp_tx_create(connp->cfg, CFG_SHARED, connp->conn);
    if (connp->in_tx == NULL) return HTP_ERROR;

    list_add(connp->conn->transactions, connp->in_tx);

    connp->in_content_length = -1;
    connp->in_body_data_left = -1;
    connp->in_header_line_index = -1;
    connp->in_header_line_counter = 0;
    connp->in_chunk_request_index = connp->in_chunk_count;

    // Run hook TRANSACTION_START
    if (hook_run_all(connp->cfg->hook_transaction_start, connp) != HOOK_OK) {
        return HTP_ERROR;
    }

    // Change state into request line parsing
    connp->in_state = htp_connp_REQ_LINE;
    connp->in_tx->progress = TX_PROGRESS_REQ_LINE;

    return HTP_OK;
}

/**
 * Process a chunk of inbound (client or request) data.
 * 
 * @param connp
 * @param timestamp
 * @param data
 * @param len
 * @return HTP_OK on state change, HTTP_ERROR on error, or HTP_DATA when more data is needed.
 */
int htp_connp_req_data(htp_connp_t *connp, htp_time_t timestamp, unsigned char *data, size_t len) {
    // Return straight away if there's no data to process
    if ((data == NULL) || (len == 0)) {
        return HTP_ERROR;
    }

    // Also return if the connection has had a fatal error
    if (connp->status != HTP_OK) {
        return HTP_ERROR;
    }

    // Store the current chunk information
    connp->in_timestamp = timestamp;
    connp->in_current_data = data;
    connp->in_current_len = len;
    connp->in_current_offset = 0;
    connp->in_chunk_count++;

    // Invoke a processor, in a loop, until an error
    // occurs or until we run out of data. Many processors
    // will process a request, each pointing to the next
    // processor that needs to run.
    for (;;) {
        // Return if there's been an error
        // or if we've run out of data. We are relying
        // on processors to add error messages, so we'll
        // keep quiet here.
        int status = connp->in_state(connp);
        if (status != HTP_OK) {
            // Do we need more data?
            if (status == HTP_DATA) {
                return HTP_DATA;
            }

            // Remember that we've had an error. Errors are
            // not possible to recover from.
            connp->status = HTP_ERROR;

            return HTP_ERROR;
        }
    }
}
