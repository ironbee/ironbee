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
#include <ironbee/debug.h>
#include <ironbee/plugin.h>
#include <ironbee/module.h>
#include <ironbee/provider.h>
#include <rule_engine_private.h>

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
    ib_mpool_t         *config_mp;        /**< Config memory pool */
    ib_mpool_t         *temp_mp;          /**< Temp memory pool for config */
    ib_provider_inst_t *dpi;              /**< Data provider instance */
    ib_context_t       *ectx;             /**< Engine configuration context */
    ib_context_t       *ctx;              /**< Main configuration context */
    ib_uuid_t           sensor_id;        /**< Sensor UUID */
    uint32_t            sensor_id_hash;   /**< Sensor UUID hash (4 bytes) */
    const char         *sensor_id_str;    /**< ascii format, for logging */
    const char         *sensor_name;      /**< Sensor name */
    const char         *sensor_version;   /**< Sensor version string */
    const char         *sensor_hostname;  /**< Sensor hostname */

    /// @todo Only these should be private
    ib_plugin_t        *plugin;           /**< Info about the server plugin */
    ib_array_t         *modules;          /**< Array tracking modules */
    ib_array_t         *filters;          /**< Array tracking filters */
    ib_array_t         *contexts;         /**< Configuration contexts */
    ib_hash_t          *dirmap;           /**< Hash tracking directive map */
    ib_hash_t          *apis;             /**< Hash tracking provider APIs */
    ib_hash_t          *providers;        /**< Hash tracking providers */
    ib_hash_t          *tfns;             /**< Hash tracking transformations */
    ib_rulelist_t      *rules;            /**< Global rules */
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
 * Configuration context.
 */
struct ib_context_t {
    ib_engine_t             *ib;          /**< Engine */
    ib_mpool_t              *mp;          /**< Memory pool */
    ib_cfgmap_t             *cfg;         /**< Config map */
    ib_array_t              *cfgdata;     /**< Config data */
    ib_context_t            *parent;      /**< Parent context */
    ib_logformat_t          *index_fmt;   /**< Used to specify the logformat */

    /* Context Selection */
    ib_context_fn_t          fn_ctx;      /**< Context decision function */
    ib_context_site_fn_t     fn_ctx_site; /**< Context site function */
    void                    *fn_ctx_data; /**< Context function data */

    /* Filters */
    ib_list_t               *filters;     /**< Context enabled filters */

    /* Hooks */
    ib_hook_t   *hook[IB_STATE_EVENT_NUM + 1]; /**< Registered hook callbacks */

    /* Rules to execute / phase: One rule set per "phase" */
    ib_ruleset_t            *ruleset;     /**< Rules to exec */

    /* All rules defined in this context */
    ib_rulelist_t           *ctx_rules;   /**< Context specific rules */
};

/**
 * @internal
 *
 * Matcher.
 */
struct ib_matcher_t {
    ib_engine_t             *ib;          /**< Engine */
    ib_mpool_t              *mp;          /**< Memory pool */
    ib_provider_t           *mpr;         /**< Matcher provider */
    ib_provider_inst_t      *mpi;         /**< Matcher provider instance */
    const char              *key;         /**< Matcher key */
};

#endif /* IB_PRIVATE_H_ */
