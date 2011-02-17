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

#ifndef _IB_PRIVATE_H_
#define _IB_PRIVATE_H_

/**
 * @file
 * @brief IronBee - Private Declarations
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <ironbee/engine.h>
#include <ironbee/util.h>
#include <ironbee/plugin.h>
#include <ironbee/module.h>
#include <ironbee/provider.h>

/**
 * @internal
 *
 * Internal hook structure
 */
struct ib_hook_t {
    ib_void_fn_t        callback;         /**< Callback function */
    void               *cdata;            /**< Data passed to the callback */
    ib_hook_t          *next;             /**< The next callback in the list */
};

/**
 * @internal
 *
 * Engine handle.
 */
struct ib_engine_t {
    ib_mpool_t         *mp;               /**< Primary memory pool */
    ib_plugin_t        *plugin;           /**< Info about the server plugin */

    ib_provider_inst_t *dpi;              /**< Data provider instance */

    /* Configuration */
    ib_mpool_t         *temp_mp;          /**< Temp memory pool for config */
    ib_mpool_t         *config_mp;        /**< Config memory pool */
    ib_array_t         *modules;          /**< Array tracking modules */
    ib_array_t         *contexts;         /**< Configuration contexts */
    ib_context_t       *ectx;             /**< Engine configuration context */
    ib_context_t       *ctx;              /**< Main configuration context */
    ib_hash_t          *dirmap;           /**< Hash tracking directive map */
    ib_hash_t          *apis;             /**< Hash tracking provider APIs */
    ib_hash_t          *providers;        /**< Hash tracking providers */
    ib_hash_t          *tfns;             /**< Hash tracking transformations */
};

/**
 * @internal
 *
 * Transformation.
 */
struct ib_tfn_t {
    const char         *name;              /**< Tfn name */  
    ib_tfn_fn_t         transform;         /**< Tfn function */
    void               *fndata;            /**< Tfn function data */
};

/**
 * @internal
 *
 * Configuration context data.
 */
typedef struct ib_context_data_t ib_context_data_t;
struct ib_context_data_t {
    ib_module_t        *module;           /**< Module handle */
    void               *data;             /**< Module config structure */
};

/**
 * @internal
 *
 * Site.
 */
struct ib_site_t {
    ib_engine_t             *ib;          /**< Engine */
    ib_mpool_t              *mp;          /**< Memory pool */
    const char              *name;        /**< Site name */
    /// @todo IPs needs to be IP:Port and be associated with a host
    ib_list_t               *ips;         /**< IP addresses */
    ib_list_t               *hosts;       /**< Hostnames */
    ib_list_t               *locations;   /**< List of locations */
    ib_loc_t                *default_loc; /**< Default location */
};

/**
 * @internal
 *
 * Location.
 */
struct ib_loc_t {
    ib_site_t               *site;        /**< Site */
    /// @todo: use regex
    const char              *path;        /**< Location path */
};

/**
 * @internal
 *
 * Configuration context.
 */
struct ib_context_t {
    ib_engine_t             *ib;          /**< Engine */
    ib_mpool_t              *mp;          /**< Memory pool */
    ib_cfgmap_t             *cfg;         /**< Config map */
    ib_array_t              *cfgdata;     /**< Config data */

    ib_context_fn_t          fn_ctx;      /**< Context decision function */
    void                    *fn_ctx_data; /**< Context function data */

    ib_core_cfg_t           *core_cfg;    /**< Core config */
    ib_provider_inst_t      *logger;      /**< Log provider instance */
    ib_provider_inst_t      *parser;      /**< Parser provider instance */

    ib_hook_t   *hook[IB_STATE_EVENT_NUM + 1]; /**< Registered hook callbacks */
};

#endif /* IB_PRIVATE_H_ */
