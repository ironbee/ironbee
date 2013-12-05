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
 * @brief IronBee --- Development TxResponse Module
 *
 * This module can be used to add headers to the transaction response.  These
 * can be defined through the use of the TxRsp configuration directive.
 *
 * Below is an example configuration snippet that uses the FieldTx directive
 * to create number, unsigned number, NUL-terminated string, Byte-string and
 * List.  The named fields "Num1", "Num2", ... will be created for every
 * transaction processed by the engine.
 *
 *   TxResp Num1      NUM      1
 *   TxResp Num2      NUM      5
 *   TxResp Float1    FLOAT    1
 *   TxResp Float2    FLOAT    5.5
 *   TxResp Str1      NULSTR   "abc"
 *   TxResp Str2      NULSTR   "ABC"
 *   TxResp BStr1     BYTESTR  "ABC"
 *   TxResp BStr2     BYTESTR  "DEF"
 *   TxResp List0     LIST
 *   TxResp List1     LIST:NUM 1 2 3 4 5
 *   TxResp List2     LIST:NULSTR a bc def foo
 *   TxResp List3     LIST
 *   TxResp List3:Lst LIST:NULSTR a bc def foo
 *
 * @note This module is enabled only for builds configured with
 * "--enable-devel".
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "moddevel_private.h"

#include <ironbee/bytestr.h>
#include <ironbee/cfgmap.h>
#include <ironbee/config.h>
#include <ironbee/core.h>
#include <ironbee/engine_state.h>
#include <ironbee/field.h>
#include <ironbee/list.h>
#include <ironbee/module.h>
#include <ironbee/mpool.h>
#include <ironbee/string.h>
#include <ironbee/util.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/**
 * TxResp configuration
 */
struct ib_moddevel_txresp_config_t {
    ib_mpool_t *mp;                        /**< Memory pool to use for allocations */
    bool        enabled;                   /**< Are we enabled? */
};

/**
 * Handle response_header events to add headers.
 *
 * @param[in] ib IronBee object
 * @param[in] event Event type
 * @param[in,out] tx Transaction object
 * @param[in] cbdata Callback data (module configuration)
 *
 * @returns Status code
 */
static ib_status_t tx_header_finished(
    ib_engine_t           *ib,
    ib_tx_t               *tx,
    ib_state_event_type_t  event,
    void                  *cbdata)
{
    assert(ib != NULL);
    assert(tx != NULL);
    assert(event == response_header_finished_event);
    assert(cbdata != NULL);

    const ib_moddevel_txresp_config_t *config =
        (const ib_moddevel_txresp_config_t *)cbdata;
    ib_status_t rc;

    if (!config->enabled) {
        return IB_OK;
    }

    /* Note: ib_server_header() ignores lengths for now */
    rc = ib_tx_server_header(tx, IB_SERVER_RESPONSE, IB_HDR_SET,
                             IB_S2SL("ENGINE-UUID"),
                             IB_S2SL(ib_engine_instance_uuid_str(ib)),
                             NULL);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tx_server_header(tx, IB_SERVER_RESPONSE, IB_HDR_SET,
                             IB_S2SL("TX-UUID"), IB_S2SL(tx->id), NULL);
    if (rc != IB_OK) {
        return rc;
    }


    return IB_OK;
}


/**
 * Handle on/off directives.
 *
 * @param[in] cp Config parser
 * @param[in] directive Directive name
 * @param[in] onoff on/off flag
 * @param[in] cbdata User data (module configuration)
 *
 * @returns Status code
 */
static ib_status_t moddevel_onoff_handler(
    ib_cfgparser_t *cp,
    const char     *directive,
    int             onoff,
    void           *cbdata)
{
    assert(cp != NULL);
    assert(directive != NULL);
    assert(cbdata != NULL);

    ib_moddevel_txresp_config_t *config =
        (ib_moddevel_txresp_config_t *)cbdata;
    config->enabled = (bool)onoff;

    /* Done */
    return IB_OK;
}

static ib_dirmap_init_t moddevel_txresp_directive_map[] = {
    IB_DIRMAP_INIT_ONOFF(
        "TxResp",
        moddevel_onoff_handler,
        NULL                        /* Filled in by the init function */
    ),

    /* signal the end of the list */
    IB_DIRMAP_INIT_LAST
};

ib_status_t ib_moddevel_txresp_init(
    ib_engine_t                  *ib,
    ib_module_t                  *mod,
    ib_mpool_t                   *mp,
    ib_moddevel_txresp_config_t **pconfig)
{
    assert(ib != NULL);
    assert(mod != NULL);
    assert(mp != NULL);
    assert(pconfig != NULL);

    ib_status_t rc;
    ib_moddevel_txresp_config_t *config;

    /* Create our configuration structure */
    config = ib_mpool_calloc(mp, sizeof(*config), 1);
    if (config == NULL) {
        return IB_EALLOC;
    }
    config->mp = mp;
    config->enabled = false;

    /* Set the directive callback data to be our configuration object */
    moddevel_txresp_directive_map[0].cbdata_cb = config;
    rc = ib_config_register_directives(ib, moddevel_txresp_directive_map);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the TX header_finished callback */
    rc = ib_hook_tx_register(ib,
                             response_header_finished_event,
                             tx_header_finished,
                             config);
    if (rc != IB_OK) {
        ib_log_error(ib, "Error registering hook: %d", rc);
    }

    *pconfig = config;
    return IB_OK;
}

ib_status_t ib_moddevel_txresp_cleanup(
    ib_engine_t                 *ib,
    ib_module_t                 *mod,
    ib_moddevel_txresp_config_t *config)
{
    /* Do nothing */
    return IB_OK;
}

ib_status_t ib_moddevel_txresp_fini(
    ib_engine_t                 *ib,
    ib_module_t                 *mod)
{
    return IB_OK;
}
