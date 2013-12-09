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

#ifndef _IB_SERVER_H_
#define _IB_SERVER_H_

/**
 * @file
 * @brief IronBee --- Ironbee as a server plugin
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Nick Kew <nkew@qualys.com>
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbee/engine_types.h>
#include <ironbee/release.h>
#include <ironbee/types.h>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeServer Server Plugins
 * @ingroup IronBee
 *
 * A server plugin defines how data is given to IronBee.
 *
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
    IB_SERVER_REQUEST = 0x01,
    IB_SERVER_RESPONSE = 0x02
} ib_server_direction_t;

/* Functions to modify HTTP Request/Response Header
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
    IB_HDR_SET,    /**< Remove other values of the header and add this one. */
    IB_HDR_APPEND, /**< Append the header. */
    IB_HDR_MERGE,  /**< Append unless the header value already exists. */
    IB_HDR_ADD,    /**< Add the header. */
    IB_HDR_UNSET,  /**< Unset the header. */
} ib_server_header_action_t;

/**
 * Set server error status code.
 *
 * @param[in] tx The transaction.
 * @param[in] status The status code.
 * @param[in] cbdata Callback data.
 */
typedef ib_status_t (*ib_server_error_fn_t)(
    ib_tx_t *tx,
    int      status,
    void    *cbdata
);

/**
 * Set server error header.
 *
 * @param[in] tx The transaction.
 * @param[in] name Name of header.
 * @param[in] name_length Length of @a name.
 * @param[in] value value of header.
 * @param[in] value_length Length of @a value.
 * @param[in] cbdata Callback data.
 */
typedef ib_status_t (*ib_server_error_hdr_fn_t)(
    ib_tx_t    *tx,
    const char *name,
    size_t      name_length,
    const char *value,
    size_t      value_length,
    void       *cbdata
);

/**
 * Set server error data.
 *
 * @param[in] tx The transaction.
 * @param[in] data The data to set.
 * @param[in] dlen The data length to be copied, starting at index 0.
 * @param[in] cbdata Callback data.
 */
typedef ib_status_t (*ib_server_error_data_fn_t)(
    ib_tx_t    *tx,
    const char *data,
    size_t      dlen,
    void       *cbdata
);

/**
 * Set a server header.
 *
 * @param[in] tx The transaction.
 * @param[in] dir The direction.
 * @param[in] action The action determining how to add the header.
 * @param[in] name Name of header.
 * @param[in] name_length Length of @a name.
 * @param[in] value value of header.
 * @param[in] value_length Length of @a value.
 * @param[in] cbdata Callback data.
 */
typedef ib_status_t (*ib_server_header_fn_t)(
    ib_tx_t                   *tx,
    ib_server_direction_t      dir,
    ib_server_header_action_t  action,
    const char                *name,
    size_t                     name_length,
    const char                *value,
    size_t                     value_length,
    void                      *cbdata
);

/**
 * Close the given connection.
 *
 * @param[in] conn Connection to close.
 * @param[in] tx Transaction that the connection is in.
 *               There are situations where tx may be NULL.
 *               Implementers should expect to close a
 *               connection when there is no associated transaction.
 * @param[in] cbdata Callback data.
 *
 * @returns
 *   - IB_OK on success.
 *   - IB_DECLINED if the server cannot honor this request.
 */
typedef ib_status_t (*ib_server_close_fn_t)(
    ib_conn_t *conn,
    ib_tx_t   *tx,
    void      *cbdata
);

#ifdef HAVE_FILTER_DATA_API
typedef ib_status_t (*ib_server_filter_init_fn_t)(
    ib_tx_t               *tx,
    ib_server_direction_t  dir,
    void                  *cbdata
);
typedef ib_status_t (*ib_server_filter_data_fn_t)(
    ib_tx_t               *tx,
    ib_server_direction_t  dir,
    const char            *block,
    size_t                 len,
    void                  *cbdata
);
#endif /* HAVE_FILTER_DATA_API */

struct ib_server_t {
    /* Header */
    uint32_t                 vernum;   /**< Engine version number */
    uint32_t                 abinum;   /**< Engine ABI Number */
    const char              *version;  /**< Engine version string */
    const char              *filename; /**< Plugin code filename */
    const char              *name;     /**< Unique plugin name */

    /**
     * Function to tell host server to do something to an HTTP header.
     */
    ib_server_header_fn_t hdr_fn;

    /** Callback data for hdr_fn */
    void *hdr_data;

    /**
     * Function to communicate an error response/action to host server.
     */
    ib_server_error_fn_t err_fn;

    /** Callback data for err_fn */
    void *err_data;

    /**
     * Function to communicate an error response header to host server.
     */
    ib_server_error_hdr_fn_t err_hdr_fn;

    /** Callback data for err_hdr_fn */
    void *err_hdr_data;

    /**
     * Function to communicate an error response body to host server.
     */
    ib_server_error_data_fn_t err_body_fn;

    /** Callback data for err_body_fn */
    void *err_body_data;

    /** Close connection. */
    ib_server_close_fn_t close_fn;

    /** Callback data for close_fn. */
    void *close_data;

#ifdef HAVE_FILTER_DATA_API
    /** Initialize data filtering */
    ib_server_filter_init_fn_t init_fn;

    /** Callback data for init_fn */
    void *init_data;

    /** Pass filtered data chunk to caller */
    ib_server_filter_data_fn_t data_fn;

    /** Callback data for data_fn */
    void *data_data;
#endif
};

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
 * @param[in] svr The @ref ib_server_t
 * @param[in] tx Transaction
 * @param[in] status Status code
 * @return indication of whether the requested error action is supported
 */
ib_status_t DLL_PUBLIC ib_server_error_response(
    const ib_server_t *svr,
    ib_tx_t           *tx,
    int                status
);

/**
 * Function to set an HTTP header in an error response.
 * Any values set here will only take effect if an HTTP response
 * code is also set using ib_server_error_response.
 *
 * @param[in] svr The @ref ib_server_t
 * @param[in] tx Transaction object
 * @param[in] name Name of header.
 * @param[in] name_length Length of @a name.
 * @param[in] value value of header.
 * @param[in] value_length Length of @a value.
 * @return indication of whether the requested error action is supported
 */
ib_status_t DLL_PUBLIC ib_server_error_header(
    const ib_server_t *svr,
    ib_tx_t           *tx,
    const char        *name,
    size_t             name_length,
    const char        *value,
    size_t             value_length
);

/**
 * Function to set an error response body.
 * Any values set here will only take effect if an HTTP response
 * code is also set using ib_server_error_response.
 *
 * @param[in] svr The @ref ib_server_t
 * @param[in] ctx Application pointer from the server
 * @param[in] data Response to set
 * @param[in] dlen Length of @a data
 * @return indication of whether the requested error action is supported
 */
ib_status_t DLL_PUBLIC ib_server_error_body(
    const ib_server_t *svr,
    ib_tx_t           *tx,
    const char        *data,
    size_t             dlen
);

/**
 * Function to modify HTTP Request/Response Header
 *
 * We support header actions as in httpd's mod_headers, with semantics
 * as documented at
 * http://httpd.apache.org/docs/current/mod/mod_headers.html#requestheader
 *
 * We exclude the "edit" option on the premise that Ironbee will perform
 * any such operation internally and use set/append/merge/add/unset
 *
 * @param[in] svr The @ref ib_server_t
 * @param[in] tx Transaction
 * @param[in] dir Request or Response
 * @param[in] action Action requested
 * @param[in] name Name of header.
 * @param[in] name_length Length of @a name.
 * @param[in] value value of header.
 * @param[in] value_length Length of @a value.
 * @return Success or error.
 */
ib_status_t DLL_PUBLIC ib_server_header(
    const ib_server_t         *svr,
    ib_tx_t                   *tx,
    ib_server_direction_t      dir,
    ib_server_header_action_t  action,
    const char                *name,
    size_t                     name_length,
    const char                *value,
    size_t                     value_length
);

/**
 * Close the given connection.
 *
 * @param[in] svr The @ref ib_server_t
 * @param[in] conn Connection to close.
 * @param[in] tx Transaction that the connection is in.
 *               There are situations where tx may be NULL.
 *               Implementers should expect to close a
 *               connection when there is no associated transaction.
 *
 * @returns
 *   - IB_OK on success.
 *   - IB_DECLINED if the server cannot honor this request.
 */
ib_status_t DLL_PUBLIC ib_server_close(
    const ib_server_t *svr,
    ib_conn_t         *conn,
    ib_tx_t           *tx
);

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
 * @param[in] svr The @ref ib_server_t
 * @param[in] ctx Application pointer from the server
 * @param[in] dir Request or Response
 * @return Indication of whether the action is supported and will happen.
 */
ib_status_t DLL_PUBLIC ib_server_filter_init(
    const ib_server_t *svr,
    void              *ctx,
    ib_direction_t     dir
);

/**
 * Filtered data should only be passed if ib_server_filter_init returned IB_OK.
 *
 * @param[in] svr The @ref ib_server_t
 * @param[in] ctx Application pointer from the server
 * @param[in] dir Request or Response
 * @param[in] data Data chunk
 * @param[in] len Length of chunk
 * @return Success or error
 */
ib_status_t DLL_PUBLIC ib_server_filter_data(
    const ib_server_t *svr,
    void              *ctx,
    ib_direction_t     dir,
    const char        *block,
    size_t             len
);
#endif /* HAVE_FILTER_DATA_API */

/**
 * @} IronBeeServer
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_SERVER_H_ */
