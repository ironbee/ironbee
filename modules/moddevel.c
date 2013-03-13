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
 * @brief IronBee --- Development module
 *
 * This is a module that provides several directives, operators and actions
 * useful for development purposes.
 *
 * @note This module is enabled only for builds configured with
 * "--enable-devel".
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "moddevel_private.h"

#include <ironbee/engine.h>
#include <ironbee/module.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>


/* Define the module name as well as a string version of it. */
#define MODULE_NAME        devel
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

/* Declare the public module symbol. */
IB_MODULE_DECLARE();

/**
 * Module configuration data
 */
typedef struct {
    ib_moddevel_txdata_config_t *txdata;  /**< TxData configuration */
    ib_moddevel_txdump_config_t *txdump;  /**< TxDump configuration */
    ib_moddevel_rules_config_t  *rules;   /**< Rules configuration */
} ib_moddevel_config_t;

static ib_moddevel_config_t moddevel_config =
{
    NULL,                   /**< TxData config structure */
    NULL,                   /**< TxDump config structure */
    NULL,                   /**< Rules config structure */
};

/**
 * Called to initialize the development module
 *
 * Initialize sub-modules
 *
 * @param[in,out] ib IronBee object
 * @param[in] module Module object
 * @param[in] cbdata (unused)
 *
 * @returns Status code
 */
static ib_status_t moddevel_init(
    ib_engine_t *ib,
    ib_module_t *module,
    void        *cbdata)
{
    ib_status_t rc;
    ib_mpool_t *mp = ib_engine_pool_main_get(ib);

    /* TxData */
    rc = ib_moddevel_txdata_init(ib, module, mp, &moddevel_config.txdata);
    if (rc != IB_OK) {
        return rc;
    }

    /* TxDump */
    rc = ib_moddevel_txdump_init(ib, module, mp, &moddevel_config.txdump);
    if (rc != IB_OK) {
        return rc;
    }

    /* Rule development */
    rc = ib_moddevel_rules_init(ib, module, mp, &moddevel_config.rules);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

/**
 * Un-Initialize the development module
 *
 * Un-initialize sub-modules
 *
 * @param[in,out] ib IronBee object
 * @param[in] module Module object
 * @param[in] cbdata (unused)
 *
 * @returns Status code
 */
static ib_status_t moddevel_finish(
    ib_engine_t *ib,
    ib_module_t *module,
    void        *cbdata)
{
    ib_status_t rc;

    rc = ib_moddevel_txdata_fini(ib, module, moddevel_config.txdata);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_moddevel_txdump_fini(ib, module, moddevel_config.txdump);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_moddevel_rules_fini(ib, module, moddevel_config.rules);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,          /* Default metadata */
    MODULE_NAME_STR,                    /* Module name */
    IB_MODULE_CONFIG(&moddevel_config), /* Global config data */
    NULL,                               /* Module config map */
    NULL,                               /* Module directive map */
    moddevel_init,                      /* Initialize function */
    NULL,                               /* Callback data */
    moddevel_finish,                    /* Finish function */
    NULL,                               /* Callback data */
    NULL,                               /* Context open function */
    NULL,                               /* Callback data */
    NULL,                               /* Context close function */
    NULL,                               /* Callback data */
    NULL,                               /* Context destroy function */
    NULL                                /* Callback data */
);
