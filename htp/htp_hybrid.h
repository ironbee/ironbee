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
 * Enumerate possible data handling strategies in hybrid parsing
 * mode. The two possibilities are to make copies of all data and
 * use bstr instances to wrap already available data.
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
 * Create a new transaction using the connection parser provided.
 *
 * @param[in] connp
 * @return Transaction instance on success, NULL on failure.
 */
htp_tx_t *htp_txh_create(htp_connp_t *connp);
    
/**
 * Initialize hybrid parsing mode, change state to TRANSACTION_START,
 * and invoke all registered callbacks.
 * 
 * @param[in] tx
 * @return HTP_OK on success; HTP_ERROR on error, HTP_STOP if one of the
 *         callbacks does not want to follow the transaction any more.
 */
int htp_txh_state_request_start(htp_tx_t *tx);
     
/**
 * Set transaction request method. This function will enable you to keep
 * track of the text representation of the method.
 * 
 * @param[in] tx
 * @param[in] method
 * @param[in] alloc
 * @return HTP_OK on success, HTP_ERROR on failure.
 */
int htp_txh_req_set_method_c(htp_tx_t *tx, const char *method, enum alloc_strategy alloc);

/**
 * Set transaction request method number. This function enables you to
 * keep track how a particular method string is interpreted. This function
 * is useful with web servers that ignore invalid methods; for example, some
 * web servers will treat them as a GET.
 * 
 * @param[in] tx
 * @param[in] method_number
 */
void htp_txh_req_set_method_number(htp_tx_t *tx, int method_number);
     
/**
 * Set transaction request URI. The value provided here must not include any
 * query string data. Use a separate call to htp_txh_req_set_query_string_c() for that.
 * 
 * @param[in] tx
 * @param[in] uri
 * @param[in] alloc
 * @return HTP_OK on success, HTP_ERROR on failure.
 */
int htp_txh_req_set_uri_c(htp_tx_t *tx, const char *uri, enum alloc_strategy alloc);

/**
 * Sets transaction query string. If there are any query string processors
 * configured, they will be called to parse the provided data (although that
 * may not happen until the transaction state is changed to REQUEST_LINE).
 * 
 * @param[in] tx
 * @param[in] query_string
 * @param[in] alloc
 * @return HTP_OK on success, HTP_ERROR on failure.
 */
int htp_txh_req_set_query_string_c(htp_tx_t *tx, const char *query_string, enum alloc_strategy alloc);

/**
 * Set request protocol string (e.g., "HTTP/1.0"), which will then be parsed
 * to extract protocol name and version. Do not invoke when HTTP/0.9 is used
 * (because this protocol version does not actually use the protocol string).
 * Must be invoked before htp_txh_set_req_protocol_number().
 * 
 * @param[in] tx
 * @param[in] protocol
 * @param[in] alloc
 * @return HTP_OK on success, HTP_ERROR on failure.
 */
int htp_txh_req_set_protocol_c(htp_tx_t *tx, const char *protocol, enum alloc_strategy alloc);

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
 * potential differences in how the protocol version is determined.
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
 * @return HTP_OK on success; HTP_ERROR on error, HTP_STOP if one of the
 *         callbacks does not want to follow the transaction any more.
 */
int htp_txh_state_request_line(htp_tx_t *tx);
     
/**
 * Set one request header. This function should be invoked once for
 * each available header, and in the order in which headers were
 * seen in the request.
 * 
 * @param[in] tx
 * @param[in] name
 * @param[in] value
 * @param[in] alloc
 * @return HTP_OK on success, HTP_ERROR on failure.
 */
int htp_txh_req_set_header_c(htp_tx_t *tx, const char *name, const char *value, enum alloc_strategy alloc);

/**
 * Removes all request headers associated with this transaction. This
 * function is needed because in some cases the container does not
 * differentiate between standard and trailing headers. In that case,
 * you set request headers once at the beginning of the transaction,
 * read the body (at this point the request headers should contain the
 * mix of regular and trailing headers), clear all headers, and then set
 * them all again.
 * 
 * @param[in] tx
 * @return HTP_OK on success, HTP_ERROR on failure.
 */
int htp_txh_req_headers_clear(htp_tx_t *tx);
     
/**
 * Change transaction state to REQUEST_HEADERS and invoke all
 * registered callbacks.
 * 
 * @param[in] tx
 * @return HTP_OK on success; HTP_ERROR on error, HTP_STOP if one of the
 *         callbacks does not want to follow the transaction any more.
 */
int htp_txh_state_request_headers(htp_tx_t *tx);     

/**
 * Process a chunk of request body data. This function assumes that
 * handling of chunked encoding is implemented by the container. When
 * you're done submitting body data, invoke a state change (to REQUEST)
 * to finalize any processing that might be pending. The supplied data is
 * fully consumed and there is no expectation that it will be available
 * afterwards. The protocol parsing code makes no copies of the data,
 * but some parsers might.
 * 
 * @param[in] tx
 * @param[in] data
 * @param[in] len
 * @return HTP_OK on success, HTP_ERROR on failure.
 */
int htp_txh_req_process_body_data(htp_tx_t *tx, const unsigned char *data, size_t len);

/**
 * Change transaction state to REQUEST and invoke registered callbacks.
 *
 * @param[in] tx
 * @return HTP_OK on success; HTP_ERROR on error, HTP_STOP if one of the
 *         callbacks does not want to follow the transaction any more.
 */
int htp_txh_state_request_complete(htp_tx_t *tx);

/**
 * Change transaction state to RESPONSE_START and invoke registered callbacks.
 *
 * @param[in] tx
 * @return HTP_OK on success; HTP_ERROR on error, HTP_STOP if one of the
 *         callbacks does not want to follow the transaction any more.
 */
int htp_txh_state_response_start(htp_tx_t *tx);
     
/**
 * Set response line. Use this function is you have a single buffer containing
 * the entire line. If you have individual request line pieces, use the other
 * available functions.
 * 
 * @param[in] tx
 * @param[in] line
 * @param[in] alloc
 * @return HTP_OK on success, HTP_ERROR on failure.
 */     
int htp_txh_res_set_status_line_c(htp_tx_t *tx, const char *line, enum alloc_strategy alloc);

/**
 * Set response protocol number.
 *
 * @param[in] tx
 * @param[in] protocol
 * @return HTP_OK on success, HTP_ERROR on failure.
 */
void htp_txh_res_set_protocol_number(htp_tx_t *tx, int protocol_number);

/**
 * Set response status code.
 * 
 * @param[in] tx
 * @param[in] status
 * @return HTP_OK on success, HTP_ERROR on failure.
 */
void htp_txh_res_set_status_code(htp_tx_t *tx, int status_code);

/**
 * Set response status message, which is the part of the response
 * line that comes after the status code.
 *
 * @param[in] tx
 * @param[in] message
 * @return HTP_OK on success, HTP_ERROR on failure.
 */
int htp_txh_res_set_status_message(htp_tx_t *tx, const char *message, enum alloc_strategy alloc);

/**
 * Change transaction state to RESPONSE_LINE and invoke registered callbacks.
 *
 * @param[in] tx
 * @return HTP_OK on success; HTP_ERROR on error, HTP_STOP if one of the
 *         callbacks does not want to follow the transaction any more.
 */
int htp_txh_state_response_line(htp_tx_t *tx);

/**
 * Set one response header. This function should be invoked once for
 * each available header, and in the order in which headers were
 * seen in the response.
 * 
 * @param[in] tx
 * @param[in] name
 * @param[in] value
 * @param[in] alloc
 * @return HTP_OK on success, HTP_ERROR on failure.
 */     
int htp_txh_res_set_header_c(htp_tx_t *tx, const char *name, const char *value, enum alloc_strategy alloc);

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
 * @return HTP_OK on success, HTP_ERROR on failure.
 */
int htp_txh_res_headers_clear(htp_tx_t *tx);

/**
 * Change transaction state to RESPONSE_HEADERS and invoke registered callbacks.
 *
 * @param[in] tx
 * @return HTP_OK on success; HTP_ERROR on error, HTP_STOP if one of the
 *         callbacks does not want to follow the transaction any more.
 */
int htp_txh_state_response_headers(htp_tx_t *tx);

/**
 * Process a chunk of response body data. This function assumes that
 * handling of chunked encoding is implemented by the container. When
 * you're done submitting body data, invoking a state change (to RESPONSE)
 * will finalize any processing that might be pending.
 * 
 * The response body data will be decompressed if two conditions are met: one,
 * decompression is enabled in configuration and two, if the response headers
 * indicate compression. Alternatively, you can control decompression from
 * a RESPONSE_HEADERS callback, by setting tx->response_content_encoding either
 * to COMPRESSION_NONE (to disable compression), or to one of the supported
 * decompression algorithms.
 *
 * @param[in] tx
 * @param[in] data
 * @param[in] len
 * @return HTP_OK on success, HTP_ERROR on failure.
 */
int htp_txh_res_process_body_data(htp_tx_t *tx, const char *data, size_t len);

/**
 * Change transaction state to RESPONSE and invoke registered callbacks.
 *
 * @param[in] tx
 * @return HTP_OK on success; HTP_ERROR on error, HTP_STOP if one of the
 *         callbacks does not want to follow the transaction any more.
 */
int htp_txh_state_response_complete(htp_tx_t *tx);


#ifdef	__cplusplus
}
#endif

#endif	/* HTP_HYBRID_H */
