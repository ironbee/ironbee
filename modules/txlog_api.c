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
 * @brief IronBee Modules --- Transaction Logs Public API
 *
 * The TxLog module, if enabled for a site, writes transaction logs.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include "ironbee_config_auto.h"

#include "txlog_private.h"

/* Include our own public header file. */
#include "txlog.h"

#include <assert.h>
#include <ironbee/module.h>
#include <ironbee/context.h>

ib_status_t ib_txlog_get_config(
    const ib_engine_t            *ib,
    const ib_context_t           *ctx,
    const ib_txlog_module_cfg_t **cfg
)
{
    assert(ib != NULL);
    assert(ctx != NULL);
    assert(cfg != NULL);

    ib_status_t     rc;
    txlog_config_t *module_cfg = NULL;
    ib_module_t    *module     = NULL;

    rc = ib_engine_module_get(ib, TXLOG_MODULE_NAME, &module);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to retrieve module "TXLOG_MODULE_NAME);
        return rc;
    }

    rc = ib_context_module_config(ctx, module, &module_cfg);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to retrieve " TXLOG_MODULE_NAME " config.");
        return rc;
    }

    *cfg = (const ib_txlog_module_cfg_t *)&(module_cfg->pub_cfg);

    return IB_OK;
}
