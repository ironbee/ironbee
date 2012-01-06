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

/**
 * @file
 * @brief IronBee - Effective Remote Address Extraction Module
 *
 * This module extracts the effective remote address
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */
#include <stdio.h>
#include <string.h>

#include <ironbee/engine.h>
#include <ironbee/util.h>
#include <ironbee/module.h>
#include <ironbee/debug.h>
#include <ironbee/hash.h>
#include <ironbee/bytestr.h>
#include <ironbee/mpool.h>

/* Define the module name as well as a string version of it. */
#define MODULE_NAME        remote_address
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

/* Declare the public module symbol. */
IB_MODULE_DECLARE();

/**
 * @internal
 * Handle request_header events.
 *
 * Extract the "request_headers" field (a list) from the transactions's
 * data provider instance, then loop through the list, looking for the
 * "X-Forwarded-For"  field.  If found, the first value in the (comma
 * separated) list replaces the local ip address string in the connection
 * object.
 *
 * @param[in] ib IronBee object
 * @param[in,out] tx Transaction object
 * @param[in] data Callback data (not used)
 *
 * @returns Status code
 */
static ib_status_t modra_handle_req_headers(ib_engine_t *ib,
                                            ib_tx_t *tx,
                                            void *data)
{
    IB_FTRACE_INIT(modra_handle_tx);
    ib_field_t    *req_fwd = NULL;
    ib_status_t    rc = IB_OK;
    ib_bytestr_t  *bs;
    unsigned       len;
    char          *buf;
    uint8_t       *comma;

    /* Extract the request headers field from the provider instance */
    rc = ib_data_get(tx->dpi, "request_headers.X-Forwarded-For", &req_fwd);
    if ( (req_fwd == NULL) || (rc != IB_OK) ) {
        ib_log_debug(ib, 4,
                     "request_headers_event: No forward header" );
        IB_FTRACE_RET_STATUS(IB_OK);
    }


    /* Found it: copy the data into a newly allocated string buffer */
    bs = ib_field_value_bytestr(req_fwd);
    len = ib_bytestr_length(bs);

    /* Search for a comma in the buffer */
    comma = memchr(ib_bytestr_ptr(bs), ',', len);
    if (comma != NULL) {
        len = comma - ib_bytestr_ptr(bs);
    }

    /* Allocate the memory */
    buf = (char *)ib_mpool_calloc(tx->mp, 1, len+1);
    if (buf == NULL) {
        ib_log_error( ib, 4,
                      "Failed to allocate %d bytes for local address",
                      len+1 );
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Copy the string out */
    memcpy(buf, ib_bytestr_ptr(bs), len);
    buf[len] = '\0';

    ib_log_debug(ib, 4, "Remote address => '%s'", buf);

    /* This will lose the pointer to the original address
     * buffer, but it should be cleaned up with the rest
     * of the memory pool. */
    tx->er_ipstr = buf;
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Called to initialize the remote address module (when the module is loaded).
 *
 * Registers a handler for the request_headers_event event.
 *
 * @param[in,out] ib IronBee object
 * @param[in] m Module object
 *
 * @returns Status code
 */
static ib_status_t modra_init(ib_engine_t *ib, ib_module_t *m)
{
   IB_FTRACE_INIT(modra_context_init);
   ib_status_t rc;

   /* Register our callback */
   rc = ib_hook_register(ib, request_headers_event,
                         (ib_void_fn_t)modra_handle_req_headers,
                         (void*)request_headers_event);
   if (rc != IB_OK) {
       ib_log_error(ib, 4, "Hook register returned %d", rc);
   }

   IB_FTRACE_RET_STATUS(IB_OK);
}

IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,      /* Default metadata */
    MODULE_NAME_STR,                /* Module name */
    IB_MODULE_CONFIG_NULL,          /* Global config data */
    NULL,                           /* Module config map */
    NULL,                           /* Module directive map */
    modra_init,                     /* Initialize function */
    NULL,                           /* Finish function */
    NULL,                           /* Context init function */
    NULL                            /* Context fini function */
);
