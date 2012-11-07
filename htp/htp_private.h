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

#ifndef _HTP_PRIVATE_H
#define	_HTP_PRIVATE_H

#ifdef	__cplusplus
extern "C" {
#endif

#define IN_TEST_NEXT_BYTE_OR_RETURN(X) \
if ((X)->in_current_offset >= (X)->in_current_len) { \
    return HTP_DATA; \
}

#define IN_NEXT_BYTE(X) \
if ((X)->in_current_offset < (X)->in_current_len) { \
    (X)->in_next_byte = (X)->in_current_data[(X)->in_current_offset]; \
    (X)->in_current_offset++; \
    (X)->in_stream_offset++; \
} else { \
    (X)->in_next_byte = -1; \
}

#define IN_NEXT_BYTE_OR_RETURN(X) \
if ((X)->in_current_offset < (X)->in_current_len) { \
    (X)->in_next_byte = (X)->in_current_data[(X)->in_current_offset]; \
    (X)->in_current_offset++; \
    (X)->in_stream_offset++; \
} else { \
    return HTP_DATA; \
}

#define IN_COPY_BYTE_OR_RETURN(X) \
if ((X)->in_current_offset < (X)->in_current_len) { \
    (X)->in_next_byte = (X)->in_current_data[(X)->in_current_offset]; \
    (X)->in_current_offset++; \
    (X)->in_stream_offset++; \
} else { \
    return HTP_DATA; \
} \
\
if ((X)->in_line_len < (X)->in_line_size) { \
    (X)->in_line[(X)->in_line_len] = (X)->in_next_byte; \
    (X)->in_line_len++; \
    if (((X)->in_line_len == HTP_HEADER_LIMIT_SOFT)&&(!((X)->in_tx->flags & HTP_FIELD_LONG))) { \
        (X)->in_tx->flags |= HTP_FIELD_LONG; \
        htp_log((X), HTP_LOG_MARK, HTP_LOG_ERROR, HTP_LINE_TOO_LONG_SOFT, "Request field over soft limit"); \
    } \
} else { \
    htp_log((X), HTP_LOG_MARK, HTP_LOG_ERROR, HTP_LINE_TOO_LONG_HARD, "Request field over hard limit"); \
    return HTP_ERROR; \
}

#define OUT_TEST_NEXT_BYTE_OR_RETURN(X) \
if ((X)->out_current_offset >= (X)->out_current_len) { \
    return HTP_DATA; \
}

#define OUT_NEXT_BYTE(X) \
if ((X)->out_current_offset < (X)->out_current_len) { \
    (X)->out_next_byte = (X)->out_current_data[(X)->out_current_offset]; \
    (X)->out_current_offset++; \
    (X)->out_stream_offset++; \
} else { \
    (X)->out_next_byte = -1; \
}

#define OUT_NEXT_BYTE_OR_RETURN(X) \
if ((X)->out_current_offset < (X)->out_current_len) { \
    (X)->out_next_byte = (X)->out_current_data[(X)->out_current_offset]; \
    (X)->out_current_offset++; \
    (X)->out_stream_offset++; \
} else { \
    return HTP_DATA; \
}

#define OUT_COPY_BYTE_OR_RETURN(X) \
if ((X)->out_current_offset < (X)->out_current_len) { \
    (X)->out_next_byte = (X)->out_current_data[(X)->out_current_offset]; \
    (X)->out_current_offset++; \
    (X)->out_stream_offset++; \
} else { \
    return HTP_DATA; \
} \
\
if ((X)->out_line_len < (X)->out_line_size) { \
    (X)->out_line[(X)->out_line_len] = (X)->out_next_byte; \
    (X)->out_line_len++; \
    if (((X)->out_line_len == HTP_HEADER_LIMIT_SOFT)&&(!((X)->out_tx->flags & HTP_FIELD_LONG))) { \
        (X)->out_tx->flags |= HTP_FIELD_LONG; \
        htp_log((X), HTP_LOG_MARK, HTP_LOG_ERROR, HTP_LINE_TOO_LONG_SOFT, "Response field over soft limit"); \
    } \
} else { \
    htp_log((X), HTP_LOG_MARK, HTP_LOG_ERROR, HTP_LINE_TOO_LONG_HARD, "Response field over hard limit"); \
    return HTP_ERROR; \
}

// Parser states

int htp_connp_REQ_IDLE(htp_connp_t *connp);
int htp_connp_REQ_LINE(htp_connp_t *connp);
int htp_connp_REQ_PROTOCOL(htp_connp_t *connp);
int htp_connp_REQ_HEADERS(htp_connp_t *connp);
int htp_connp_REQ_BODY_DETERMINE(htp_connp_t *connp);
int htp_connp_REQ_BODY_IDENTITY(htp_connp_t *connp);
int htp_connp_REQ_BODY_CHUNKED_LENGTH(htp_connp_t *connp);
int htp_connp_REQ_BODY_CHUNKED_DATA(htp_connp_t *connp);
int htp_connp_REQ_BODY_CHUNKED_DATA_END(htp_connp_t *connp);
int htp_connp_REQ_FINALIZE(htp_connp_t *connp);

int htp_connp_REQ_CONNECT_CHECK(htp_connp_t *connp);
int htp_connp_REQ_CONNECT_WAIT_RESPONSE(htp_connp_t *connp);

int htp_connp_RES_IDLE(htp_connp_t *connp);
int htp_connp_RES_LINE(htp_connp_t *connp);
int htp_connp_RES_HEADERS(htp_connp_t *connp);
int htp_connp_RES_BODY_DETERMINE(htp_connp_t *connp);
int htp_connp_RES_BODY_IDENTITY(htp_connp_t *connp);
int htp_connp_RES_BODY_CHUNKED_LENGTH(htp_connp_t *connp);
int htp_connp_RES_BODY_CHUNKED_DATA(htp_connp_t *connp);
int htp_connp_RES_BODY_CHUNKED_DATA_END(htp_connp_t *connp);
int htp_connp_RES_FINALIZE(htp_connp_t *connp);


#ifdef	__cplusplus
}
#endif

#endif	/* _HTP_PRIVATE_H */

