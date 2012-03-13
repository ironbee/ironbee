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

#include <ironbee/types.h>
#include <ironbee/engine.h>
#include <ironbee/util.h>
#include <ironbee/debug.h>
#include <ironbee/plugin.h>
#include <ironbee/lock.h>
#include <ironbee/module.h>
#include <ironbee/provider.h>
#include <ironbee/array.h>
#include <ironbee/logformat.h>

/* Pull in FILE* for ib_auditlog_cfg_t. */
#include <stdio.h>

/**
 * @internal
 *
 * Internal hook structure
 */
typedef struct ib_hook_t ib_hook_t;
struct ib_hook_t {
    union {
        /*! Comparison only */
        ib_void_fn_t                as_void;

        ib_state_null_hook_fn_t     null;
        ib_state_conn_hook_fn_t     conn;
        ib_state_conndata_hook_fn_t conndata;
        ib_state_tx_hook_fn_t       tx;
        ib_state_txdata_hook_fn_t   txdata;
    } callback;
    void               *cdata;            /**< Data passed to the callback */
    ib_hook_t          *next;             /**< The next callback in the list */
};

/**
 * @internal
 *
 * Rule engine per-context data
 */
typedef struct ib_rule_engine_t ib_rule_engine_t;

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
    ib_hash_t          *operators;        /**< Hash tracking operators */
    ib_hash_t          *actions;          /**< Hash tracking rules */
    ib_rule_engine_t   *rules;            /**< Rule engine data */
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
 * Per-context audit log configuration.
 *
 * This struct is associated with an owning context by the ib_context_t*
 * member named "owner."
 * Only the owner context may destroy or edit the logging context.
 * Child contexts that copy from the parent context may have a copy of
 * the pointer to this struct, but may not edit its context.
 *
 * Child contexts may, though, lock the index_fp_lock field and write to
 * the index_fp.
 *
 * The owning context should lock index_fp_lock before updating lock_fp and
 * index.
 */
typedef struct ib_auditlog_cfg_t ib_auditlog_cfg_t;

//! See typedef for more details.
struct ib_auditlog_cfg_t {
    char *index;            /**< Index file. */
    FILE *index_fp;         /**< Index file pointer. */
    ib_lock_t index_fp_lock; /**< Lock to protect index_fp. */
    ib_context_t *owner;    /**< Owning context. Only owner should edit. */
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
    ib_auditlog_cfg_t       *auditlog;    /**< Per-context audit log cfgs. */

    /* Context Selection */
    ib_context_fn_t          fn_ctx;      /**< Context decision function */
    ib_context_site_fn_t     fn_ctx_site; /**< Context site function */
    void                    *fn_ctx_data; /**< Context function data */

    /* Filters */
    ib_list_t               *filters;     /**< Context enabled filters */

    /* Hooks */
    ib_hook_t   *hook[IB_STATE_EVENT_NUM + 1]; /**< Registered hook callbacks */

    /* Rules associated with this context */
    ib_rule_engine_t        *rules;       /**< Rule engine data */
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

/**
 * Parameters used for variable expansion in rules.
 */
#define IB_VARIABLE_EXPANSION_PREFIX  "%{"  /**< Variable prefix */
#define IB_VARIABLE_EXPANSION_POSTFIX "}"   /**< Variable postfix */

/**
 * @internal
 * Initialize the rule engine.
 *
 * Called when the rule engine is loaded, registers event handlers.
 *
 * @param[in,out] ib IronBee object
 * @param[in] mod Module object
 */
ib_status_t ib_rule_engine_init(ib_engine_t *ib,
                                ib_module_t *mod);

/**
 * @internal
 * Initialize a context the rule engine.
 *
 * Called when a context is initialized, performs rule engine initialization.
 *
 * @param[in,out] ib IronBee object
 * @param[in] mod Module object
 * @param[in,out] ctx IronBee context
 */
ib_status_t ib_rule_engine_ctx_init(ib_engine_t *ib,
                                    ib_module_t *mod,
                                    ib_context_t *ctx);

/**
 * @internal
 * Initialize the core operators.
 *
 * Called when the rule engine is loaded, registers the core operators.
 *
 * @param[in,out] ib IronBee object
 * @param[in] mod Module object
 */
ib_status_t ib_core_operators_init(ib_engine_t *ib,
                                   ib_module_t *mod);

/**
 * @internal
 * Initialize the core actions.
 *
 * Called when the rule engine is loaded, registers the core actions.
 *
 * @param[in,out] ib IronBee object
 * @param[in] mod Module object
 */
ib_status_t ib_core_actions_init(ib_engine_t *ib,
                                 ib_module_t *mod);


#endif /* IB_PRIVATE_H_ */
