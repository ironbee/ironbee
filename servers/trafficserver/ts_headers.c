/*****************************************************************************
 * Licensed to Qualys, Inc. (QUALYS) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * QUALYS licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ****************************************************************************/

/**
 * @file
 * @brief IronBee --- Apache Traffic Server Plugin
 *
 * @author Nick Kew <nkew@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ctype.h>
#include <assert.h>
#include <ts/ts.h>

#if defined(__cplusplus) && !defined(__STDC_FORMAT_MACROS)
/* C99 requires that inttypes.h only exposes PRI* macros
 * for C++ implementations if this is defined: */
#define __STDC_FORMAT_MACROS
#endif

#include <ironbee/context.h>
#include <ironbee/core.h>
#include <ironbee/site.h>
#include <ironbee/state_notify.h>
#include <ironbee/string.h>

#include "ts_ib.h"

tsib_direction_data_t tsib_direction_client_req = {
    .dir                       = IBD_REQ,
    .type_label                = "client request",
    .dir_label                 = "request",
    .hdr_get                   = TSHttpTxnClientReqGet,
    .ib_notify_header          = ib_state_notify_request_header_data,
    .ib_notify_header_finished = ib_state_notify_request_header_finished,
    .ib_notify_body            = ib_state_notify_request_body_data,
    .ib_notify_end             = ib_state_notify_request_finished,
    .ib_notify_post            = NULL,
    .ib_notify_log             = NULL
};
tsib_direction_data_t tsib_direction_server_resp = {
    .dir                       = IBD_RESP,
    .type_label                = "server response",
    .dir_label                 = "response",
    .hdr_get                   = TSHttpTxnServerRespGet,
    .ib_notify_header          = ib_state_notify_response_header_data,
    .ib_notify_header_finished = ib_state_notify_response_header_finished,
    .ib_notify_body            = ib_state_notify_response_body_data,
    .ib_notify_end             = ib_state_notify_response_finished,
    .ib_notify_post            = ib_state_notify_postprocess,
    .ib_notify_log             = ib_state_notify_logging
};
tsib_direction_data_t tsib_direction_client_resp = {
    .dir                       = IBD_RESP,
    .type_label                = "client response",
    .dir_label                 = "response",
    .hdr_get                   = TSHttpTxnClientRespGet,
    .ib_notify_header          = ib_state_notify_response_header_data,
    .ib_notify_header_finished = ib_state_notify_response_header_finished,
    .ib_notify_body            = ib_state_notify_response_body_data,
    .ib_notify_end             = ib_state_notify_response_finished,
    .ib_notify_post            = ib_state_notify_postprocess,
    .ib_notify_log             = ib_state_notify_logging
};

/**
 * Parse lines in an HTTP header buffer
 *
 * Given a buffer including "\r\n" linends, this finds the next line and its
 * length.  Where a line is wrapped, continuation lines are included in
 * in the (multi-)line parsed.
 *
 * Can now also error-correct for "\r" or "\n" line ends.
 *
 * @param[in,out] linep Buffer to parse.  On output, moved on one line.
 * @param[out] lenp Line length (excluding line end)
 * @param[in] letype What to treat as line ends
 * @return 1 if a line was parsed, 2 if parsed but with error correction,
 *         0 for a blank line (no more headers), -1 for irrecoverable error
 */
static int next_line(const char **linep, size_t *lenp, http_lineend_t letype)
{
    int rv = 1;

    size_t len = 0;
    size_t lelen = 2;
    const char *end;
    const char *line = *linep;

    switch (letype) {
      case LE_RN: /* Enforces the HTTP spec */
        if ( (line[0] == '\r') && (line[1] == '\n') ) {
            return 0; /* blank line = no more hdr lines */
        }
        /* skip to next start-of-line from where we are */
        line = strstr(line, "\r\n");
        if (!line) {
            return -1;
        }
        line += 2;
        if ( (line[0] == '\r') && (line[1] == '\n') ) {
            return 0; /* blank line = no more hdr lines */
        }
        /* Use a loop here to catch theoretically-unlimited numbers
         * of continuation lines in a folded header.  The isspace
         * tests for a continuation line
         */
        do {
            end = strstr(line, "\r\n");
            if (!end) {
                return -1;
            }
            lelen = 2;
            len = end - line;
        } while ( (isspace(end[lelen]) != 0) &&
                  (end[lelen] != '\r') &&
                  (end[lelen] != '\n') );
        break;
      case LE_ANY: /* Original code: take either \r or \n as lineend */

        if ( (line[0] == '\r') && (line[1] == '\n') ) {
            return 0; /* blank line = no more hdr lines */
        }
        else if ( ( (line[0] == '\r') || (line[0] == '\n') ) ) {
            return 0; /* blank line which is also malformed HTTP */
        }

        /* skip to next start-of-line from where we are */
        line += strcspn(line, "\r\n");
        if ( (line[0] == '\r') && (line[1] == '\n') ) {
            /* valid line end.  Set pointer to start of next line */
            line += 2;
        }
        else {   /* bogus lineend!
                  * Treat a single '\r' or '\n' as a lineend
                  */
            line += 1;
            rv = 2; /* bogus linend */
        }
        if ( (line[0] == '\r') && (line[1] == '\n') ) {
            return 0; /* blank line = no more hdr lines */
        }
        else if ( (line[0] == '\r') || (line[0] == '\n') ) {
            return 0; /* blank line which is also malformed HTTP */
        }

        /* Use a loop here to catch theoretically-unlimited numbers
         * of continuation lines in a folded header.  The isspace
         * tests for a continuation line
         */
        do {
            if (len > 0) {
                /* we have a continuation line.  Add the lineend. */
                len += lelen;
            }
            end = line + strcspn(line + len, "\r\n");
            if ( (end[0] == '\r') && (end[1] == '\n') ) {
                lelen = 2;             /* All's well, this is a good line */
            }
            else {
                /* Malformed header.  Check for a bogus single-char lineend */
                if (end > line) {
                    lelen = 1;
                    rv = 2;
                }
                else { /* nothing at all we can interpret as lineend */
                    return -1;
                }
            }
            len = end - line;
        } while ( (isspace(end[lelen]) != 0) &&
                  (end[lelen] != '\r') &&
                  (end[lelen] != '\n') );
        break;
      case LE_N: /* \n is lineend, but either \n or \r\n is blank line */

        if ( (line[0] == '\r') && (line[1] == '\n') ) {
            return 0; /* blank line = no more hdr lines */
        }
        else if ( line[0] == '\n' ) {
            return 0; /* blank line which is also malformed HTTP */
        }

        /* skip to next start-of-line from where we are */
        line = strchr(line, '\n');
        if (line == NULL) {
            return -1;
        }
        ++line;
        if ( (line[0] == '\r') && (line[1] == '\n') ) {
            return 0; /* blank line = no more hdr lines */
        }
        else if ( line[0] == '\n' ) {
            return 0; /* blank line which is also malformed HTTP */
        }

        /* Use a loop here to catch theoretically-unlimited numbers
         * of continuation lines in a folded header.  The isspace
         * tests for a continuation line
         */
        do {
            end = strchr(line, '\n');
            if (end == NULL) {
                return -1; /* there's no lineend */
            }
            /* point to the last non-lineend char and set length of lineend */
            if (end[-1] == '\r') {
                end--;
                lelen = 2;
            }
            else {
                lelen = 1;
                rv = 2;    /* we're into error-correcting */
            }
            len = end - line;
        } while ( (isspace(end[lelen]) != 0) &&
                  (end[lelen] != '\r') &&
                  (end[lelen] != '\n') );
        break;
    }

    *lenp = len;
    *linep = line;
    return rv;
}

static void header_action(TSMBuffer bufp, TSMLoc hdr_loc,
                          const hdr_action_t *act, ib_tx_t *tx)
{
    TSMLoc field_loc;
    int rv;
    assert(tx != NULL);

    switch (act->action) {

    case IB_HDR_SET:  /* replace any previous instance == unset + add */
    case IB_HDR_UNSET:  /* unset it */
        ib_log_debug_tx(tx, "Remove HTTP Header \"%s\"", act->hdr);
        /* Use a while loop in case there are multiple instances */
        while (field_loc = TSMimeHdrFieldFind(bufp, hdr_loc, act->hdr,
                                              strlen(act->hdr)),
               field_loc != TS_NULL_MLOC) {
            TSMimeHdrFieldDestroy(bufp, hdr_loc, field_loc);
            TSHandleMLocRelease(bufp, hdr_loc, field_loc);
        }
        if (act->action == IB_HDR_UNSET)
            break;
        /* else fallthrough to ADD */

    case IB_HDR_ADD:  /* add it in, regardless of whether it exists */
add_hdr:
        ib_log_debug_tx(tx, "Add HTTP Header \"%s\"=\"%s\"",
                        act->hdr, act->value);
        rv = TSMimeHdrFieldCreate(bufp, hdr_loc, &field_loc);
        if (rv != TS_SUCCESS) {
            ib_log_error_tx(tx, "Failed to add MIME header field \"%s\".", act->hdr);
        }
        rv = TSMimeHdrFieldNameSet(bufp, hdr_loc, field_loc,
                                   act->hdr, strlen(act->hdr));
        if (rv != TS_SUCCESS) {
            ib_log_error_tx(tx, "Failed to set name of MIME header field \"%s\".",
                            act->hdr);
        }
        rv = TSMimeHdrFieldValueStringSet(bufp, hdr_loc, field_loc, -1,
                                          act->value, strlen(act->value));
        if (rv != TS_SUCCESS) {
            ib_log_error_tx(tx, "Failed to set value of MIME header field \"%s\".",
                            act->hdr);
        }
        rv = TSMimeHdrFieldAppend(bufp, hdr_loc, field_loc);
        if (rv != TS_SUCCESS) {
            ib_log_error_tx(tx, "Failed to append MIME header field \"%s\".", act->hdr);
        }
        TSHandleMLocRelease(bufp, hdr_loc, field_loc);
        break;

    case IB_HDR_MERGE:  /* append UNLESS value already appears */
        /* FIXME: implement this in full */
        /* treat this as APPEND */

    case IB_HDR_APPEND: /* append it to any existing instance */
        ib_log_debug_tx(tx, "Merge/Append HTTP Header \"%s\"=\"%s\"",
                        act->hdr, act->value);
        field_loc = TSMimeHdrFieldFind(bufp, hdr_loc, act->hdr,
                                       strlen(act->hdr));
        if (field_loc == TS_NULL_MLOC) {
            /* this is identical to IB_HDR_ADD */
            goto add_hdr;
        }
        /* This header exists, so append to it
         * (the function is called Insert but actually appends
         */
        rv = TSMimeHdrFieldValueStringInsert(bufp, hdr_loc, field_loc, -1,
                                             act->value, strlen(act->value));
        if (rv != TS_SUCCESS) {
            ib_log_error_tx(tx, "Failed to insert MIME header field \"%s\".", act->hdr);
        }
        TSHandleMLocRelease(bufp, hdr_loc, field_loc);
        break;

    default:  /* bug !! */
        ib_log_debug_tx(tx, "Bogus header action %d", act->action);
        break;
    }
}

/**
 * Get the HTTP request/response buffer & line from ATS
 *
 * @param[in]  hdr_bufp Header marshal buffer
 * @param[in]  hdr_loc Header location object
 * @param[in]  mm IronBee memory manager to use for allocations
 * @param[out] phdr_buf Pointer to header buffer
 * @param[out] phdr_len Pointer to header length
 * @param[out] pline_buf Pointer to line buffer
 * @param[out] pline_len Pointer to line length
 *
 * @returns IronBee Status Code
 */
static ib_status_t get_http_header(
    TSMBuffer         hdr_bufp,
    TSMLoc            hdr_loc,
    ib_tx_t           *tx,
    const char      **phdr_buf,
    size_t           *phdr_len,
    const char      **pline_buf,
    size_t           *pline_len)
{
    assert(hdr_bufp != NULL);
    assert(hdr_loc != NULL);
    assert(phdr_buf != NULL);
    assert(phdr_len != NULL);
    assert(pline_buf != NULL);
    assert(pline_len != NULL);
    assert(tx != NULL);

    ib_mm_t           mm = tx->mm;
    ib_status_t       rc = IB_OK;
    TSIOBuffer        iobuf;
    TSIOBufferReader  reader;
    TSIOBufferBlock   block;
    char             *hdr_buf;
    size_t            hdr_len;
    size_t            hdr_off = 0;
    size_t            line_len;
    int64_t           bytes;

    iobuf = TSIOBufferCreate();
    /* reader has to be allocated *before* TSHttpHdrPrint, because
     * the latter loses all reference to blocks before the last 4K
     * in iobuf.
     */
    reader = TSIOBufferReaderAlloc(iobuf);
    TSHttpHdrPrint(hdr_bufp, hdr_loc, iobuf);

    bytes = TSIOBufferReaderAvail(reader);
    hdr_buf = ib_mm_alloc(mm, bytes + 1);
    if (hdr_buf == NULL) {
        rc = IB_EALLOC;
        goto cleanup;
    }
    hdr_len = bytes;

    for (block = TSIOBufferReaderStart(reader);
         block != NULL;
         block = TSIOBufferBlockNext(block)) {
        const char *data;
        data = TSIOBufferBlockReadStart(block, reader, &bytes);
        if (bytes == 0) {
            break;
        }
        memcpy(hdr_buf + hdr_off, data, bytes);
        hdr_off += bytes;
    }
    *(hdr_buf + hdr_len) = '\0';
    ++hdr_len;

    /* Find the line end. */
    /* hack fixes RNS506, but could potentially get in trouble if
     * a malformed request contains a \r or \n lineend but no \r\n
     */
    char *line_end = strstr(hdr_buf, "\r\n");
    while (line_end == NULL && hdr_len > 2) {
        char *null = memchr(hdr_buf, 0, hdr_len);
        if (null != NULL) {
            /* remove embedded NULL and retry */
            int bytes = hdr_len - (null+1 - hdr_buf);
            memmove(null, null+1, bytes);
            hdr_len--;
            line_end = strstr(null, "\r\n");
        }
        else {
            /* There are no NULLs, and we still don't have termination */
            ib_log_error_tx(tx, "Invalid HTTP request line.");
            rc = IB_EINVAL;
            goto cleanup;
        }
    }
    line_len = (line_end - hdr_buf);

    *phdr_buf  = hdr_buf;
    *phdr_len  = strlen(hdr_buf);
    *pline_buf = hdr_buf;
    *pline_len = line_len;

cleanup:
    TSIOBufferReaderFree(reader);
    TSIOBufferDestroy(iobuf);

    return rc;
}

/**
 * Get the HTTP request URL from ATS
 *
 * @param[in]  hdr_bufp Header marshal buffer
 * @param[in]  hdr_loc Header location object
 * @param[in]  mm Memory manager
 * @param[out] purl_buf Pointer to URL buffer
 * @param[out] purl_len Pointer to URL length
 *
 * @returns IronBee Status Code
 */

static ib_status_t get_request_url(
    TSMBuffer         hdr_bufp,
    TSMLoc            hdr_loc,
    ib_tx_t           *tx,
    const char      **purl_buf,
    size_t           *purl_len)
{
    ib_mm_t           mm = tx->mm;
    ib_status_t       rc = IB_OK;
    int               rv;
    TSIOBuffer        iobuf;
    TSIOBufferReader  reader;
    TSIOBufferBlock   block;
    TSMLoc            url_loc;
    const char       *url_buf;
    int64_t           url_len;
    const char       *url_copy;

    assert (tx != NULL);
    rv = TSHttpHdrUrlGet(hdr_bufp, hdr_loc, &url_loc);
    assert(rv == TS_SUCCESS);

    iobuf = TSIOBufferCreate();
    TSUrlPrint(hdr_bufp, url_loc, iobuf);

    reader = TSIOBufferReaderAlloc(iobuf);
    block = TSIOBufferReaderStart(reader);

    TSIOBufferBlockReadAvail(block, reader);
    url_buf = TSIOBufferBlockReadStart(block, reader, &url_len);
    if (url_buf == NULL) {
        ib_log_error_tx(tx, "TSIOBufferBlockReadStart() returned NULL.");
        rc = IB_EUNKNOWN;
        goto cleanup;
    }

    /* hack fixes RNS506, but could potentially get in trouble if
     * a malformed request contains a \r or \n lineend but no \r\n
     */
#if STRICT_LINEEND
    char *line_end = strstr(url_buf, "\r\n");
#else
    char *line_end = strchr(url_buf, '\n');
#endif
    while (line_end == NULL && url_len > 1) {
        char *null = memchr(url_buf, 0, url_len);
        if (null != NULL) {
            /* remove embedded NULL and retry */
            int bytes = url_len - (null+1 - url_buf);
            memmove(null, null+1, bytes);
            url_len--;
            line_end = strstr(null, "\r\n");
        }
        else {
            /* There are no NULLs, and we still don't have termination */
            ib_log_error_tx(tx, "Invalid HTTP request line.");
            rc = IB_EINVAL;
            goto cleanup;
        }
    }

    url_copy = ib_mm_memdup(mm, url_buf, url_len);
    if (url_copy == NULL) {
        rc = IB_EALLOC;
        goto cleanup;
    }
    *purl_buf  = url_copy;
    *purl_len  = url_len;

cleanup:
    TSIOBufferReaderFree(reader);
    TSIOBufferDestroy(iobuf);

    return rc;
}
/**
 * Fixup the HTTP request line from ATS if required
 *
 * @param[in]  hdr_bufp Header marshal buffer
 * @param[in]  hdr_loc Header location object
 * @param[in]  tx IronBee transaction
 * @param[in]  line_buf Line buffer
 * @param[in]  line_len Length of @a line_buf
 * @param[out] pline_buf Pointer to line buffer
 * @param[out] pline_len Pointer to line length
 *
 * @returns IronBee Status Code
 */
static ib_status_t fixup_request_line(
    TSMBuffer         hdr_bufp,
    TSMLoc            hdr_loc,
    ib_tx_t          *tx,
    const char       *line_buf,
    size_t            line_len,
    const char      **pline_buf,
    size_t           *pline_len)
{
    assert(tx != NULL);
    assert(line_buf != NULL);
    assert(pline_buf != NULL);
    assert(pline_len != NULL);

    ib_status_t          rc = IB_OK;
    static const char   *bad1_str = "http:///";
    static const size_t  bad1_len = 8;
    static const char   *bad2_str = "https:///";
    static const size_t  bad2_len = 9;
    const char          *bad_url;
    size_t               bad_len;
    const char          *url_buf;
    size_t               url_len;
    const char          *bad_line_url = NULL;
    size_t               bad_line_len;
    size_t               line_method_len; /* Includes trailing space(s) */
    size_t               line_proto_off;  /* Includes leading space(s) */
    size_t               line_proto_len;  /* Includes leading space(s) */
    char                *new_line_buf;
    char                *new_line_cur;
    size_t               new_line_len;

    /* Search for "http:///" or "https:///" in the line */
    if (line_len < bad2_len + 2) {
        goto line_ok;
    }

    /* Look for http:/// first */
    bad_line_url = ib_strstr(line_buf, line_len, bad1_str, bad1_len);
    if (bad_line_url != NULL) {
        bad_url = bad1_str;
        bad_len = bad1_len;
    }
    else {
        /* Look for https:/// next */
        bad_line_url = ib_strstr(line_buf, line_len, bad2_str, bad2_len);
        if (bad_line_url != NULL) {
            bad_url = bad2_str;
            bad_len = bad2_len;
        }
    }
    if (bad_line_url == NULL) {
        goto line_ok;
    }

    /* Next, check for the pattern in the URL.  We need the URL to do that. */
    rc = get_request_url(hdr_bufp, hdr_loc, tx, &url_buf, &url_len);
    if (rc != IB_OK) {
        ib_log_error_tx(tx, "Error getting request URL: %s", ib_status_to_string(rc));
        return rc;
    }
    /* If the URL doesn't start with the above pattern, we're done. */
    if ( (url_len < bad_len) || (memcmp(url_buf, bad_url, bad_len) != 0) ) {
        goto line_ok;
    }

    bad_line_len = url_len;

    /*
     * Calculate the offset of the offending URL,
     * the start & length of the protocol
     */
    line_method_len = (bad_line_url - line_buf);
    line_proto_off = line_method_len + url_len;
    if (line_len < line_proto_off) {
        /* line_len was computed using our parser, which forgivingly
         * treats a lone \r or \n as line end.
         * url_len and hence line_proto_off was computed by TS, which is
         * less forgiving.  Hence a malformed line may trigger this.
         */
        ib_log_error_tx(tx, "Malformed request line.");
        return IB_EOTHER;
    }
    line_proto_len = line_len - line_proto_off;

    /* Advance the pointer into the URL buffer, shorten it... */
    url_buf += (bad_len - 1);
    url_len -= (bad_len - 1);

    /* Determine the size of the new line buffer, allocate it */
    new_line_len = line_method_len + url_len + line_proto_len;
    new_line_buf = ib_mm_alloc(tx->mm, new_line_len+1);
    if (new_line_buf == NULL) {
        ib_log_error_tx(tx, "Failed to allocate buffer for fixed request line.");
        *pline_buf = line_buf;
        *pline_len = line_len;
        return IB_EINVAL;
    }

    /* Copy into the new buffer */
    new_line_cur = new_line_buf;
    memcpy(new_line_cur, line_buf, line_method_len);
    new_line_cur += line_method_len;
    memcpy(new_line_cur, url_buf, url_len);
    new_line_cur += url_len;
    memcpy(new_line_cur, line_buf + line_proto_off, line_proto_len);

    /* Store new pointers */
    *pline_buf = new_line_buf;
    *pline_len = new_line_len;

    /* Log a message */
    if (ib_logger_level_get(ib_engine_logger_get(tx->ib)) >= IB_LOG_DEBUG) {
        ib_log_debug_tx(tx, "Rewrote request URL from \"%.*s\" to \"%.*s\"",
                        (int)bad_line_len, bad_line_url,
                        (int)url_len, url_buf);
    }

    /* Done */
    return IB_OK;

line_ok:
    *pline_buf = line_buf;
    *pline_len = line_len;
    return IB_OK;

}

/**
 * Start the IronBee request
 *
 * @param[in] tx The IronBee transaction
 * @param[in] line_buf Pointer to line buffer
 * @param[in] line_len Pointer to line length
 *
 * @returns IronBee Status Code
 */
static ib_status_t start_ib_request(
    ib_tx_t     *tx,
    const char  *line_buf,
    size_t       line_len)
{
    ib_status_t           rc;
    ib_parsed_req_line_t *rline;
    assert (tx != NULL);

    rc = ib_parsed_req_line_create(&rline, tx->mm,
                                   line_buf, line_len,
                                   NULL, 0,
                                   NULL, 0,
                                   NULL, 0);

    if (rc != IB_OK) {
        ib_log_error_tx(tx, "Error creating IronBee request line: %s",
                        ib_status_to_string(rc));
        return rc;
    }

    ib_log_debug_tx(tx, "calling ib_state_notify_request_started()");
    rc = ib_state_notify_request_started(tx->ib, tx, rline);
    if (rc != IB_OK) {
        ib_log_error_tx(tx, "Error notifying IronBee request start: %s",
                        ib_status_to_string(rc));
    }

    return IB_OK;
}

/**
 * Start the IronBee response
 *
 * @param[in] tx The IronBee transaction
 * @param[in] line_buf Pointer to line buffer
 * @param[in] line_len Pointer to line length
 *
 * @returns IronBee Status Code
 */
static ib_status_t start_ib_response(
    ib_tx_t     *tx,
    const char  *line_buf,
    size_t       line_len)
{
    ib_status_t            rc;
    ib_parsed_resp_line_t *rline;
    assert(tx != NULL);

    rc = ib_parsed_resp_line_create(&rline, tx->mm,
                                    line_buf, line_len,
                                    NULL, 0,
                                    NULL, 0,
                                    NULL, 0);

    if (rc != IB_OK) {
        ib_log_error_tx(tx, "Error creating IronBee response line: %s",
                        ib_status_to_string(rc));
        return rc;
    }

    ib_log_debug_tx(tx, "calling ib_state_notify_response_started()");
    rc = ib_state_notify_response_started(tx->ib, tx, rline);
    if (rc != IB_OK) {
        ib_log_error_tx(tx, "Error notifying IronBee response start: %s",
                        ib_status_to_string(rc));
    }

    return IB_OK;
}

/**
 * Process an HTTP header from ATS.
 *
 * Handles an HTTP header, called from ironbee_plugin.
 *
 * @param[in,out] data Transaction context
 * @param[in,out] txnp ATS transaction pointer
 * @param[in,out] ibd unknown
 * @return OK (nothing to tell), Error (something bad happened),
 *         HTTP_STATUS (check data->status).
 */
tsib_hdr_outcome process_hdr(tsib_txn_ctx *txndata,
                             TSHttpTxn txnp,
                             tsib_direction_data_t *ibd)
{
    int rv;
    tsib_hdr_outcome ret = HDR_OK;
    TSMBuffer bufp;
    TSMLoc hdr_loc;
    hdr_action_t *act;
    hdr_action_t setact;
    const char *line, *lptr;
    size_t line_len = 0;
    const ib_site_t *site;
    ib_status_t ib_rc;
    int nhdrs = 0;
    int body_len = 0;
    ib_parsed_headers_t *ibhdrs;
    int status_in = txndata->status;

    if (txndata->tx == NULL) {
        return HDR_OK;
    }
    ib_log_debug_tx(txndata->tx, "process %s headers", ibd->type_label);

    /* Use alternative simpler path to get the un-doctored request
     * if we have the fix for TS-998
     *
     * This check will want expanding/fine-tuning according to what released
     * versions incorporate the fix
     */
    /* We'll get a bogus URL from TS-998 */

    rv = (*ibd->hdr_get)(txnp, &bufp, &hdr_loc);
    if (rv != 0) {
        ib_log_error_tx(txndata->tx, " get %s header: %d", ibd->type_label, rv);
        ibplugin.err_fn(txndata->tx, 500, NULL);
        return HDR_ERROR;
    }

    const char           *hdr_buf;
    size_t                hdr_len;
    const char           *rline_buf;
    size_t                rline_len;

    ib_rc = get_http_header(bufp, hdr_loc, txndata->tx,
                            &hdr_buf, &hdr_len,
                            &rline_buf, &rline_len);
    if (ib_rc != IB_OK) {
        ib_log_error_tx(txndata->tx, "Failed to get %s header: %s", ibd->type_label,
                        ib_status_to_string(ib_rc));
        ibplugin.err_fn(txndata->tx, 500, NULL);
        ret = HDR_ERROR;
        goto process_hdr_cleanup;
    }

    /* Handle the request / response line */
    switch(ibd->dir) {
    case IBD_REQ: {
        ib_rc = fixup_request_line(bufp, hdr_loc, txndata->tx,
                                   rline_buf, rline_len,
                                   &rline_buf, &rline_len);
        if (ib_rc != 0) {
            ib_log_error_tx(txndata->tx, "Failed to fixup request line.");
            ibplugin.err_fn(txndata->tx, 400, NULL);
            ret = HDR_ERROR;
            goto process_hdr_cleanup;
        }

        ib_rc = start_ib_request(txndata->tx, rline_buf, rline_len);
        if (ib_rc != IB_OK) {
            ib_log_error_tx(txndata->tx, "Error starting IronBee request: %s",
                            ib_status_to_string(ib_rc));
            ibplugin.err_fn(txndata->tx, 500, NULL);
            ret = HDR_ERROR;
            goto process_hdr_cleanup;
        }
        break;
    }

    case IBD_RESP: {
        TSHttpStatus  http_status;

        ib_rc = start_ib_response(txndata->tx, rline_buf, rline_len);
        if (ib_rc != IB_OK) {
            ib_log_error_tx(txndata->tx, "Error starting IronBee response: %s",
                            ib_status_to_string(ib_rc));
        }

        /* A transitional response doesn't have most of what a real response
         * does, so we need to wait for the real response to go further
         * Cleanup is N/A - we haven't yet allocated anything locally!
         */
        http_status = TSHttpHdrStatusGet(bufp, hdr_loc);
        if (http_status == TS_HTTP_STATUS_CONTINUE) {
            return HDR_HTTP_100;
        }

        break;
    }

    default:
        ib_log_error_tx(txndata->tx, "Invalid direction: %d", ibd->dir);
    }


    /*
     * Parse the header into lines and feed to IronBee as parsed data
     */

    /* The buffer contains the Request line / Status line, together with
     * the actual headers.  So we'll skip the first line, which we already
     * dealt with.
     */
    rv = ib_parsed_headers_create(&ibhdrs, txndata->tx->mm);
    if (rv != IB_OK) {
        ibplugin.err_fn(txndata->tx, 500, NULL);
        ib_log_error_tx(txndata->tx, "Failed to create ironbee header wrapper.  Disabling.");
        ret = HDR_ERROR;
        goto process_hdr_cleanup;
    }

    // get_line ensures CRLF (line_len + 2)?
    line = hdr_buf;
    /* RNS506 fix: enforce strict lineend first time round (jumping over the request/response line) */
    int l_status;

    for (l_status = next_line(&line, &line_len, LE_N);
         l_status > 0;
         l_status = next_line(&line, &line_len, LE_N)) {
        size_t n_len;
        size_t v_len;

        n_len = strcspn(line, ":");
        lptr = line + n_len + 1;
        while (isspace(*lptr) && lptr < line + line_len)
            ++lptr;
        v_len = line_len - (lptr - line);

        /* IronBee presumably wants to know of anything zero-length
         * so don't reject on those grounds!
         */
        rv = ib_parsed_headers_add(ibhdrs,
                                                line, n_len,
                                                lptr, v_len);
        if (!body_len && (ibd->dir == IBD_REQ)) {
            /* Check for expectation of a request body */
            if ((n_len == 14) && !strncasecmp(line, "Content-Length", n_len)) {
                /* lptr should contain a number.
                 * If it's positive we get normal processing, including body.
                 * If it's zero, we have a special case.
                 * If it's blank or malformed, log an error.
                 *
                 * FIXME: should we be more aggressive in malformed case?
                 * Shouldn't really be the plugin's problem.
                 */
                size_t i;
                for (i=0; i < v_len; ++i) {
                    if (isdigit(lptr[i])) {
                        body_len = 10*body_len + lptr[i] - '0';
                    }
                    else if (!isspace(lptr[i])) {
                        ib_log_error_tx(txndata->tx, "Malformed Content-Length: %.*s",
                                        (int)v_len, lptr);
                        break;
                    }
                }
            }
            else if (((n_len == 17) && (v_len == 7)
                   && !strncasecmp(line, "Transfer-Encoding", n_len)
                   && !strncasecmp(lptr, "chunked", v_len))) {
                body_len = -1;  /* nonzero - body and length yet to come */
            }
        }
        if (rv != IB_OK)
            ib_log_error_tx(txndata->tx, "Failed to add header '%.*s: %.*s' to IronBee list",
                            (int)n_len, line, (int)v_len, lptr);
        ++nhdrs;
    }

    /* Notify headers if present */
    if (nhdrs > 0) {
        ib_log_debug_tx(txndata->tx, "process_hdr: notifying header data");
        rv = (*ibd->ib_notify_header)(txndata->tx->ib, txndata->tx, ibhdrs);
        if (rv != IB_OK)
            ib_log_error_tx(txndata->tx, "Failed to notify IronBee header data event.");
        ib_log_debug_tx(txndata->tx, "process_hdr: notifying header finished");
        rv = (*ibd->ib_notify_header_finished)(txndata->tx->ib, txndata->tx);
        if (rv != IB_OK)
            ib_log_error_tx(txndata->tx, "Failed to notify IronBee header finished event.");
    }

    /* If there are no headers, treat as a transitional response */
    else {
        ib_log_debug_tx(txndata->tx,
                        "Response has no headers!  Treating as transitional!");
        ret = HDR_HTTP_100;
        goto process_hdr_cleanup;
    }

    /* If there's no or zero-length body in a Request, notify end-of-request */
    if ((ibd->dir == IBD_REQ) && !body_len) {
        rv = (*ibd->ib_notify_end)(txndata->tx->ib, txndata->tx);
        if (rv != IB_OK)
            ib_log_error_tx(txndata->tx, "Failed to notify IronBee end of request.");
    }

    /* Initialize the header action */
    setact.action = IB_HDR_SET;
    setact.dir = ibd->dir;

    /* Add the ironbee site id to an internal header. */
    ib_rc = ib_context_site_get(txndata->tx->ctx, &site);
    if (ib_rc != IB_OK) {
        ib_log_debug_tx(txndata->tx, "Error getting site for context: %s",
                        ib_status_to_string(ib_rc));
        site = NULL;
    }
    if (site != NULL) {
        setact.hdr = "@IB-SITE-ID";
        setact.value = site->id;
        header_action(bufp, hdr_loc, &setact, txndata->tx);
    }
    else {
        ib_log_debug_tx(txndata->tx, "No site available for @IB-SITE-ID");
    }

    /* Add internal header for effective IP address */
    setact.hdr = "@IB-EFFECTIVE-IP";
    setact.value = txndata->tx->remote_ipstr;
    header_action(bufp, hdr_loc, &setact, txndata->tx);

    /* Now manipulate header as requested by ironbee */
    for (act = txndata->hdr_actions; act != NULL; act = act->next) {
        if (act->dir != ibd->dir)
            continue;    /* it's not for us */

        ib_log_debug_tx(txndata->tx, "Manipulating HTTP headers");
        header_action(bufp, hdr_loc, act, txndata->tx);
    }

    /* Add internal header if we blocked the transaction */
    setact.hdr = "@IB-BLOCK-FLAG";
    if ((txndata->tx->flags & (IB_TX_FBLOCK_PHASE|IB_TX_FBLOCK_IMMEDIATE)) != 0) {
        setact.value = "blocked";
        header_action(bufp, hdr_loc, &setact, txndata->tx);
    }
    else if (txndata->tx->flags & IB_TX_FBLOCK_ADVISORY) {
        setact.value = "advisory";
        header_action(bufp, hdr_loc, &setact, txndata->tx);
    }

process_hdr_cleanup:
    TSHandleMLocRelease(bufp, TS_NULL_MLOC, hdr_loc);

    /* If an error sent us to cleanup then it's in ret.  Else just
     * return whether or not IronBee has signalled an HTTP status.
     */
    return  (ret != HDR_OK)
               ? ret
               : ((status_in != 0) || (txndata->status == 0))
                   ? HDR_OK : HDR_HTTP_STATUS;
}
