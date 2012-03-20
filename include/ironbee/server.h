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
 *****************************************************************************/

#ifndef _IB_SERVER_H_
#define _IB_SERVER_H_

/**
 * @file
 * @brief IronBee - Ironbee as a server plugin
 *
 * @author Brian Rectanus <brectanus@qualys.com>, Nick Kew <nkew@qualys.com>
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeServer Server Plugins
 * @ingroup IronBee
 * @{
 */

#define IB_SERVER_HEADER_DEFAULTS     IB_VERNUM, \
                                      IB_ABINUM, \
                                      IB_VERSION, \
                                      __FILE__

/** Server plugin Structure */
typedef struct ib_server_t ib_server_t;

/* Request vs Response, for functions likely to share code */
typedef enum {
    IB_SERVER_REQUEST,
    IB_SERVER_RESPONSE
} ib_server_direction_t;

/* Functions to modify HTTP Request/Response Headers
 *
 * We support header actions as in httpd's mod_headers, with semantics
 * as documented at
 * http://httpd.apache.org/docs/current/mod/mod_headers.html#requestheader
 *
 * We exclude the "edit" option on the premise that Ironbee will perform
 * any such operation internally and use set/append/merge/add/unset
 *
 * We add options for the webserver plugin can signal to Ironbee
 * that it has failed to process an instruction.
 */
typedef enum {
    IB_HDR_SET,
    IB_HDR_APPEND,
    IB_HDR_MERGE,
    IB_HDR_ADD,
    IB_HDR_UNSET,
    IB_HDR_ERROR,
    IB_HDR_NOTIMPL
} ib_server_header_action_t;

typedef ib_status_t (*ib_server_error_fn)(void *ctx, int status);
typedef ib_status_t (*ib_server_error_hdr)(void *ctx, const char *name,
                                           const char *value);
typedef ib_status_t (*ib_server_error_data)(void *ctx, const char *data);
typedef ib_server_header_action_t (*ib_server_header_fn)
                  (void *ctx, ib_server_direction_t dir,
                   ib_server_header_action_t action,
                   const char *hdr, const char *value);
typedef ib_status_t (*ib_server_filter_init_fn)(void *ctx,
                                                ib_server_direction_t dir);
typedef ib_status_t (*ib_server_filter_data_fn)(void *ctx,
                                                ib_server_direction_t dir,
                                                const char *block, size_t len);

struct ib_server_t {
    /* Header */
    uint32_t                 vernum;   /**< Engine version number */
    uint32_t                 abinum;   /**< Engine ABI Number */
    const char              *version;  /**< Engine version string */
    const char              *filename; /**< Plugin code filename */

    const char              *name;     /**< Unique plugin name */

    ib_server_header_fn     hdr_fn;    /** 
                                        * Function to tell host server
                                        * to do something to an HTTP header.
                                        */
    ib_server_error_fn      err_fn;    /**
                                        * Function to communicate an error
                                        * response/action to host server.
                                        */
    ib_server_error_hdr     err_hdr;   /**
                                        * Function to communicate an error
                                        * response header to host server.
                                        */
    ib_server_error_data    err_data;  /**
                                        * Function to communicate an error
                                        * response body to host server.
                                        */
#ifdef HAVE_FILTER_DATA_API
    ib_server_filter_init_fn init_fn;  /** Initialise data filtering */
    ib_server_filter_data_fn data_fn;  /** Pass filtered data chunk to caller */
#endif
};

#ifdef DOXYGEN
/**
 * Function to indicate an error.
 * Status argument is an HTTP response code, or a special value
 *
 * In the first instance, the server takes responsibility for the
 * error document, and the data (if non-null) gives the errordoc.
 *
 * In the second instance, the server takes an enumerated special
 * action, or returns NOTIMPL if that's not supported.
 *
 * @param svr[in] The ib_server_t
 * @param ctx[in] Application pointer from the server
 * @param status[in] Action requested
 * @param data[in] Data field associated with the action
 * @return indication of whether the requested error action is supported
 */
ib_status_t ib_server_error_response(ib_server_t *svr, void *ctx,
                                     int status, const void *data);

/**
 * Function to modify HTTP Request/Response Headers
 *
 * We support header actions as in httpd's mod_headers, with semantics
 * as documented at
 * http://httpd.apache.org/docs/current/mod/mod_headers.html#requestheader
 *
 * We exclude the "edit" option on the premise that Ironbee will perform
 * any such operation internally and use set/append/merge/add/unset
 *
 * @param svr[in] The ib_server_t
 * @param ctx[in] Application pointer from the server
 * @param dir[in] Request or Response
 * @param action[in] Action requested
 * @param hdr[in] Header to act on
 * @param value[in] Value to act with
 * @return The action actually performed, or an error
 */
ib_server_header_action_t ib_server_header(ib_server_t *svr, void *ctx,
                                           ib_server_direction_t dir,
                                           ib_server_header_action_t action,
                                           const char *hdr, const char *value);

#ifdef HAVE_FILTER_DATA_API
/**
 * Ironbee should signal in advance to the server if it may modify
 * a request, so the server can avoid filtering complexity/overheads if
 * it knows nothing will change.  Server will indicate whether it supports
 * modifying the payload (and may differ between Requests and Responses).
 *
 * If Ironbee is filtering a payload, the server will regard Ironbee as
 * consuming its entire input, and generating the entire payload as
 * output in blocks.
 *
 * @param svr[in] The ib_server_t
 * @param ctx[in] Application pointer from the server
 * @param dir[in] Request or Response
 * @return Indication of whether the action is supported and will happen.
 */
ib_status_t ib_server_filter_init(ib_server_t *svr, void *ctx,
                                  ib_direction_t dir);

/**
 * Filtered data should only be passed if ib_server_filter_init returned IB_OK.
 *
 * @param svr[in] The ib_server_t
 * @param ctx[in] Application pointer from the server
 * @param dir[in] Request or Response
 * @param data[in] Data chunk
 * @param len[in] Length of chunk
 * @return Success or error
 */
ib_status_t ib_server_filter_data(ib_server_t *svr, void *ctx,
                                  ib_direction_t dir,
                                  const char *block, size_t len);
#endif

#else
#define ib_server_error_response(svr,ctx,status) \
             (svr)->err_fn ? (svr)->err_fn(ctx, status) : IB_ENOTIMPL;
#define ib_server_error_header(svr,ctx,name,val) \
             (svr)->err_hdr ? (svr)->err_hdr(ctx, name, val) : IB_ENOTIMPL;
#define ib_server_error_body(svr,ctx,data) \
             (svr)->err_data ? (svr)->err_data(ctx, data) : IB_ENOTIMPL;
#define ib_server_header(svr,ctx,dir,action,hdr,value) \
             (svr)->hdr_fn ? (svr)->hdr_fn(ctx, dir, action, hdr, value) \
                           : IB_HDR_NOTIMPL;
#define ib_server_filter_init(svr,ctx,dir) \
             (svr)->init_fn ? (svr)->init_fn(ctx, dir) : IB_ENOTIMPL;
#define ib_server_filter_data(svr,ctx,dir,data,len) \
             (svr)->data_fn ? (svr)->data_fn(ctx, dir, data, len) : IB_ENOTIMPL;
#endif

/**
 * @} IronBeePlugins
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_PLUGINS_H_ */
