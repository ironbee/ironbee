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

/* 
 * @file
 * @author Ivan Ristic <ivanr@webkreator.com>
 */

#ifndef HTP_HYBRID_H
#define	HTP_HYBRID_H

#include "htp.h"

#ifdef	__cplusplus
extern "C" {
#endif
    
/**
 * 
 */
enum alloc_strategy {
    /** Make copies of all data. This strategy should be used when
     *  the supplied buffers are transient and will go away after
     *  the invoked function returns.
     */
    ALLOC_COPY  = 1,

    /** Reuse buffers, without a change of ownership. We assume the
     *  buffers will continue to be available until the transaction
     *  is deleted by the container.
     */
    ALLOC_REUSE = 2
};
    
/**
 * Initialize hybrid parsing mode, change state to TRANSACTION_START,
 * and invoke all registered callbacks.
 * 
 * @param[in] tx
 */
int htp_txh_state_transaction_start(htp_tx_t *tx);
     
/**
 * Set transaction request method.
 * 
 * @param[in] tx
 * @param[in] method
 * @param[in] alloc
 */
void htp_txh_req_set_method_c(htp_tx_t *tx, const char *method, enum alloc_strategy alloc);

/**
 * Set transaction request method. This additional function is used to
 * account for any differences in method interpretation between LibHTP and
 * the container.
 * 
 * @param[in] tx
 * @param[in] method_number
 */
void htp_txh_req_set_method_number(htp_tx_t *tx, int method_numer);
     
/**
 * Set transaction request URI. If the URI contains a query string, it will
 * be extracted from it and the parameter parsed.
 * 
 * @param[in] tx
 * @param[in] uri
 * @param[in] alloc
 */
void htp_txh_req_set_uri_c(htp_tx_t *tx, char *uri, enum alloc_strategy alloc);

/**
 * Sets transaction query string. Any available parameters will be parsed
 * and processed.
 * 
 * @param[in] tx
 * @param[in] query_string
 * @param[in] alloc
 */
void htp_txh_req_set_query_string_c(htp_tx_t *tx, char *query_string, enum alloc_strategy alloc);

/**
 * Set request protocol string (e.g., "HTTP/1.0"), which will then be parsed
 * to extract protocol name and version. Do not invoke when HTTP/0.9 is used
 * (because this protocol version does not actually use the protocol string).
 * Must be invoked before htp_txh_set_req_protocol_number().
 * 
 * @param[in] tx
 * @param[in] protocol
 * @param[in] alloc
 */
void htp_txh_req_set_protocol_c(htp_tx_t *tx, const char *protocol, enum alloc_strategy alloc);

/**
 * Set request protocol version number. Must be invoked after
 * htp_txh_set_req_protocol_c(), because it will overwrite the previously
 * extracted version number. Convert the protocol version number to an integer
 * by multiplying it with 100. For example, 1.1 becomes 110. Alternatively,
 * use the HTTP_0_9, HTTP_1_0, and HTTP_1_1 constants.
 * 
 * @param[in] tx
 * @param[in] protocol
 */
void htp_txh_req_set_protocol_number(htp_tx_t *tx, int protocol);

/**
 * Forces HTTP/0.9 as the transaction protocol. This method exists to ensure
 * that both LibHTP and the container treat the transaction as HTTP/0.9, despite
 * potential differences in how protocol version is determined.
 *
 * @param[in] tx
 * @param[in] is_http_0_9
 */
void htp_txh_req_set_protocol_http_0_9(htp_tx_t *tx, int is_http_0_9);
     
/**
 * Change transaction state to REQUEST_LINE and invoke all
 * registered callbacks.
 * 
 * @param[in] tx
*/
void htp_txh_state_request_line(htp_tx_t *tx);
     
/**
 * Set one request header. This function should be invoked once for
 * each available header, and in the order in which headers were
 * seen in the request.
 * 
 * @param[in] tx
 * @param[in] name
 * @param[in] value
 * @param[in] alloc
 */
void htp_txh_req_set_header_c(htp_tx_t *tx, const char *name, const char *value, enum alloc_strategy alloc);

/**
 * Removes all request headers associated with this transaction. This
 * function is needed because in some cases the container does not
 * differentiate between standard and trailing headers. In that case,
 * you set request headers once at the beginning of the transaction,
 * read the body, clear all headers, and then set them all again. After
 * the headers are set for the second time, they will potentially contain
 * a mixture of standard and trailing headers.
 * 
 * @param[in] tx
 */
void htp_txh_req_headers_clear(htp_tx_t *tx);
     
/**
 * Change transaction state to REQUEST_HEADERS and invoke all
 * registered callbacks.
 * 
 * @param[in] tx
 */
void htp_txh_state_request_headers(htp_tx_t *tx);    
     
/**
 * Sets desired (de)compression method for the request body. The
 * default for hybrid parsing is COMPRESSION_DISABLED, which assumes that
 * the container will decompress if necessary. Set to COMPRESSION_AUTO to
 * instruct LibHTP to attempt decompress if the headers indicate that
 * compression was used by the client.
 * 
 * @param[in] tx
 * @param[in] compression_method
 */
void htp_txh_req_set_compression(htp_tx_t *tx, int compression_method);

/**
 * Process a chunk of request body data. This function assumes that
 * handling of chunked encoding is implemented by the container. The
 * supplied body will be decompressed if instructed by a previous
 * invocation of htp_txh_req_set_compression(). When you're done
 * submitting body data, invoke a state change (to REQUEST) to
 * finalize any processing that might be pending. The supplied data is
 * fully consumed and there is no expectation that it will be available
 * afterwards. The protocol parsing code makes no copies of the data,
 * but some parsers might.
 * 
 * @param[in] tx
 */
void htp_txh_req_process_body_data(htp_tx_t *tx, const char *data, size_t len);

/**
 * Change transaction state to REQUEST and invoke all
 * registered callbacks.
 *
 * @param[in] tx
 */
void htp_txh_state_request(htp_tx_t *tx);

/**
 * Change transaction state to RESPONSE_START and invoke all
 * registered callbacks.
 *
 * @param[in] tx
 */
void htp_txh_state_response_start(htp_tx_t *tx);
     
/**
 * Set response line.
 * 
 * @param[in] tx
 * @param[in] line
 * @param[in] alloc
 */     
void htp_txh_res_set_status_line_c(htp_tx_t *tx, const char *line, enum alloc_strategy alloc);

/**
 * Set response protocol string.
 *
 * @param[in] tx
 * @param[in] protocol
 */
void htp_txh_res_set_status_protocol(htp_tx_t *tx, const char *protocol);

/**
 * Set response status code, as seen by the container.
 * 
 * @param[in] tx
 * @param[in] status
 */
void htp_txh_res_set_status_code(htp_tx_t *tx, int status);

/**
 * Set response status message, which is the part of the response
 * line that comes after the status code.
 *
 * @param[in] tx
 * @param[in] message
 */
void htp_txh_res_set_status_message(htp_tx_t *tx, const char *message);

/**
 * Change transaction state to RESPONSE_LINE and invoke all
 * registered callbacks.
 */
void htp_txh_state_response_line(htp_tx_t *tx);

/**
 * Set one response header. This function should be invoked once for
 * each available header, and in the order in which headers were
 * seen in the response.
 * 
 * @param[in] tx
 * @param[in] name
 * @param[in] value
 * @param[in] alloc
 */     
void htp_txh_res_set_header_c(htp_tx_t *tx, const char *name, const char *value, enum alloc_strategy alloc);

/**
 * Removes all response headers associated with this transaction. This
 * function is needed because in some cases the container does not
 * differentiate between standard and trailing headers. In that case,
 * you set response headers once at the beginning of the transaction,
 * read the body, clear all headers, and then set them all again. After
 * the headers are set for the second time, they will potentially contain
 * a mixture of standard and trailing headers.
 * 
 * @param[in] tx
 */
void htp_txh_res_headers_clear(htp_tx_t *tx);

/**
 * Change transaction state to RESPONSE_HEADERS and invoke all
 * registered callbacks.
 */
void htp_txh_state_response_headers(htp_tx_t *tx);

/**
 * Sets desired (de)compression method for the response body. The
 * default for hybrid parsing is COMPRESSION_DISABLED, which assumes that
 * the container will decompress if necessary. Set to COMPRESSION_AUTO to
 * instruct LibHTP to attempt decompress if the headers indicate that
 * compression was used by the client.
 * 
 * @param[in] tx
 * @param[in] compression
 */
void htp_txh_res_set_compression(htp_tx_t *tx, int compression);

/**
 * Process a chunk of response body data. This function assumes that
 * handling of chunked encoding is implemented by the container. The
 * supplied body will be decompressed if instructed by a previous
 * invocation of htp_txh_res_set_compression(). When you're done
 * submitting body data, invoking a state change (to RESPONSE) will
 * finalize any processing that might be pending.
 * 
 * @param[in] tx
 * @param[in] data
 * @param[in] len
 */
void htp_txh_res_process_body_data(htp_tx_t *tx, const char *data, size_t len);

/**
 * Change transaction state to RESPONSE and invoke all
 * registered callbacks.
 *
 * @param[in] tx
 */
void htp_txh_state_response(htp_tx_t *tx);


#ifdef	__cplusplus
}
#endif

#endif	/* HTP_HYBRID_H */

