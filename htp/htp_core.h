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

#ifndef HTP_DEFINITIONS_H
#define	HTP_DEFINITIONS_H

#ifdef	__cplusplus
extern "C" {
#endif

typedef int htp_status_t;

typedef struct htp_cfg_t htp_cfg_t;
typedef struct htp_conn_t htp_conn_t;
typedef struct htp_connp_t htp_connp_t;
typedef struct htp_file_t htp_file_t;
typedef struct htp_file_data_t htp_file_data_t;
typedef struct htp_header_t htp_header_t;
typedef struct htp_header_line_t htp_header_line_t;
typedef struct htp_log_t htp_log_t;
typedef struct htp_tx_data_t htp_tx_data_t;
typedef struct htp_tx_t htp_tx_t;
typedef struct htp_uri_t htp_uri_t;
typedef struct timeval htp_time_t;

// Free-form version string
#define HTP_VERSION_STRING          "master"

// In the form of x.y.z, with two positions for each component; for example, 400 means 0.4.0
#define HTP_VERSION_NUMBER          400

// htp_status_t return codes
#define HTP_ERROR                   -1
#define HTP_DECLINED                0
#define HTP_OK                      1
#define HTP_DATA                    2
#define HTP_DATA_OTHER              3
#define HTP_STOP                    4

// Protocol version constants
#define HTP_PROTOCOL_UNKNOWN        -1
#define HTP_PROTOCOL_0_9             9
#define HTP_PROTOCOL_1_0             100
#define HTP_PROTOCOL_1_1             101

// HTP method constants
#define HTP_M_UNKNOWN               -1
#define HTP_M_GET                    0
#define HTP_M_PUT                    1
#define HTP_M_POST                   2
#define HTP_M_DELETE                 3
#define HTP_M_CONNECT                4
#define HTP_M_OPTIONS                5
#define HTP_M_TRACE                  6
#define HTP_M_PATCH                  7
#define HTP_M_PROPFIND               8
#define HTP_M_PROPPATCH              9
#define HTP_M_MKCOL                  10
#define HTP_M_COPY                   11
#define HTP_M_MOVE                   12
#define HTP_M_LOCK                   13
#define HTP_M_UNLOCK                 14
#define HTP_M_VERSION_CONTROL        15
#define HTP_M_CHECKOUT               16
#define HTP_M_UNCHECKOUT             17
#define HTP_M_CHECKIN                18
#define HTP_M_UPDATE                 19
#define HTP_M_LABEL                  20
#define HTP_M_REPORT                 21
#define HTP_M_MKWORKSPACE            22
#define HTP_M_MKACTIVITY             23
#define HTP_M_BASELINE_CONTROL       24
#define HTP_M_MERGE                  25
#define HTP_M_INVALID                26
#define HTP_M_HEAD                   1000

// Logging-related constants
#define HTP_LOG_MARK                 __FILE__,__LINE__

/**
 * Enumerates all log levels.
 */
enum htp_log_level_t {
    HTP_LOG_ERROR = 1,
    HTP_LOG_WARNING = 2,
    HTP_LOG_NOTICE = 3,
    HTP_LOG_INFO = 4,
    HTP_LOG_DEBUG = 5,
    HTP_LOG_DEBUG2 = 6
};

/**
 * Enumerates all stream states. Each connection has two streams, one
 * inbound and one outbound. Their states are tracked separately.
 */
enum htp_stream_state_t {
    HTP_STREAM_NEW = 0,
    HTP_STREAM_OPEN = 1,
    HTP_STREAM_CLOSED = 2,
    HTP_STREAM_ERROR = 3,
    HTP_STREAM_TUNNEL = 4,
    HTP_STREAM_DATA_OTHER = 5,
    HTP_STREAM_STOP = 6,
    HTP_STREAM_DATA = 9
};

#ifdef	__cplusplus
}
#endif

#endif	/* HTP_DEFINITIONS_H */

