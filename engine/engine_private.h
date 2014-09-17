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

#ifndef _IB_ENGINE_PRIVATE_H_
#define _IB_ENGINE_PRIVATE_H_

/**
 * @file
 * @brief IronBee --- Engine Private Declarations
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "state_notify_private.h"

#include <ironbee/array.h>
#include <ironbee/context_selection.h>
#include <ironbee/lock.h>
#include <ironbee/logger.h>

#include <stdio.h>

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
struct ib_auditlog_cfg_t {
    bool          index_enabled; /**< Index file enabled? */
    bool          index_default; /**< Index file is default? */
    char         *index;         /**< Index file name. */
    FILE         *index_fp;      /**< Index file pointer. */
    ib_lock_t    *index_fp_lock; /**< Lock to protect index_fp. */
    ib_context_t *owner;         /**< Owning context. Only owner should edit. */
};

/**
 * Rule engine data
 */
typedef struct ib_rule_engine_t ib_rule_engine_t;

/**
 * Rule engine per-context data
 */
typedef struct ib_rule_context_t ib_rule_context_t;

/**
 * Engine configuration state
 */
typedef enum {
    CFG_NOT_STARTED,
    CFG_STARTED,
    CFG_FINISHED
} ib_engine_cfg_state_t;

/**
 * Configuration Context Selection Registration Data
 */
struct ib_ctxsel_registration_t {
    ib_mm_t                        mm;                 /**< Memory manager */
    const ib_module_t             *module;             /**< Associated Module */
    void                          *common_cb_data;     /**< Common cb data */
    ib_ctxsel_select_fn_t          select_fn;          /**< Selection fn */
    void                          *select_cb_data;     /**< callback data */
    ib_ctxsel_site_create_fn_t     site_create_fn;     /**< Site create fn */
    void                          *site_create_cb_data; /**< Callback data */
    ib_ctxsel_location_create_fn_t location_create_fn;  /**< loc create fn */
    void                          *location_create_cb_data; /**< Callback data*/
    ib_ctxsel_host_create_fn_t     host_create_fn;     /**< host-create fn */
    void                          *host_create_cb_data; /**< Callback data*/
    ib_ctxsel_service_create_fn_t  service_create_fn;     /**< serv create fn */
    void                          *service_create_cb_data; /**< Callback data */
    ib_ctxsel_site_open_fn_t       site_open_fn;       /**< Site-open fn */
    void                          *site_open_cb_data;  /**< Callback cb data */
    ib_ctxsel_location_open_fn_t   location_open_fn;   /**< Location-open fn */
    void                          *location_open_cb_data; /**< Callback data */
    ib_ctxsel_site_close_fn_t      site_close_fn;      /**< Site-close fn */
    void                          *site_close_cb_data; /**< Callback data*/
    ib_ctxsel_location_close_fn_t  location_close_fn;  /**< Location-close fn */
    void                          *location_close_cb_data; /**< Callback data*/
    ib_ctxsel_finalize_fn_t        finalize_fn;        /**< Finalize fn */
    void                          *finalize_cb_data;   /**< Callback data*/
};

/**
 * Block handler function and callback data.
 **/
struct ib_block_handler_t
{
    const char *name;
    ib_block_handler_fn_t handler;
    void *cbdata;
};
typedef struct ib_block_handler_t ib_block_handler_t;

/**
 * Block pre-callback function and callback data.
 **/
struct ib_block_pre_hook_t
{
    const char *name;
    ib_block_pre_hook_fn_t hook;
    void *cbdata;
};
typedef struct ib_block_pre_hook_t ib_block_pre_hook_t;

/**
 * Block handler function and callback data.
 **/
struct ib_block_post_hook_t
{
    const char *name;
    ib_block_post_hook_fn_t hook;
    void *cbdata;
};
typedef struct ib_block_post_hook_t ib_block_post_hook_t;

/**
 * Engine handle.
 */
struct ib_engine_t {
    ib_mpool_t            *mp;              /**< Primary memory pool */
    ib_mpool_t            *config_mp;       /**< Config memory pool */
    ib_mpool_t            *temp_mp;         /**< Temp memory pool for config */
    ib_var_store_t        *var_store;       /**< Var store */
    ib_context_t          *ectx;            /**< Engine configuration context */
    ib_context_t          *ctx;             /**< Main configuration context */
    ib_engine_cfg_state_t  cfg_state;       /**< Engine configuration state */
    char                   sensor_id[IB_UUID_LENGTH]; /**< Sensor UUID */
    const char            *sensor_name;     /**< Sensor name */
    const char            *sensor_version;  /**< Sensor version string */
    const char            *sensor_hostname; /**< Sensor hostname */
    char                   instance_id[IB_UUID_LENGTH]; /**< Engine instance UUID */
    ib_cfgparser_t        *cfgparser;       /**< Our configuration parser */

    /// @todo Only these should be private
    const ib_server_t     *server;          /**< Info about the server */
    ib_array_t            *modules;         /**< Array tracking modules */
    ib_array_t            *filters;         /**< Array tracking filters */
    ib_list_t             *contexts;        /**< Configuration contexts */
    ib_hash_t             *dirmap;          /**< Hash tracking directive map */
    ib_hash_t             *tfns;            /**< Hash tracking transforms */
    ib_hash_t             *operators;       /**< Operators by name */
    ib_hash_t             *stream_operators;/**< Stream operators by name*/
    ib_hash_t             *actions;         /**< Hash tracking rules */
    ib_rule_engine_t      *rule_engine;     /**< Rule engine data */
    ib_logger_t           *logger;          /**< The engine log object. */
    ib_var_config_t       *var_config;      /**< Data configuration. */

    /* Hooks */
    ib_list_t *hooks[IB_STATE_NUM + 1]; /**< Registered hook callbacks */

    /* Context selection function registration; both active and core */
    ib_ctxsel_registration_t act_ctxsel;  /**< Active context selection reg. */
    ib_ctxsel_registration_t core_ctxsel; /**< Core context selection reg. */

    /* Block handler. */
    ib_block_handler_t block_handler;

    /* List of block pre_hooks.  Value type is ib_block_pre_hook_t* */
    ib_list_t *block_pre_hooks;

    /* List of block post_hooks.  Value type is ib_block_post_hook_t* */
    ib_list_t *block_post_hooks;
};

/**
 * Configuration context data.
 */
typedef struct ib_context_data_t ib_context_data_t;
struct ib_context_data_t {
    ib_module_t *module;      /**< Module handle */
    void        *data;        /**< Module config structure */
    size_t       data_length; /**< Length of data */
};

/**
 * Configuration context states
 */
typedef enum {
    CTX_CREATED,
    CTX_OPEN,
    CTX_CLOSED
} ib_context_state_t;

/**
 * Configuration context.
 */
struct ib_context_t {
    ib_engine_t          *ib;          /**< Engine */
    ib_mpool_t           *mp;          /**< Memory pool */
    ib_mm_t               mm;          /**< Memory manager */
    ib_cfgmap_t          *cfg;         /**< Config map */
    ib_array_t           *cfgdata;     /**< Config data */
    ib_context_t         *parent;      /**< Parent context */
    ib_list_t            *children;    /**< Child contexts */
    ib_ctype_t            ctype;       /**< Context type */
    const char           *ctx_type;    /**< Type identifier string. */
    const char           *ctx_name;    /**< Name identifier string. */
    const char           *ctx_full;    /**< Full name of context */
    const char           *ctx_cwd;     /**< Context's current directory */
    ib_auditlog_cfg_t    *auditlog;    /**< Per-context audit log cfgs. */
    ib_context_state_t    state;       /**< Context state */

    /* Data specific to the selector for this context */
    const ib_site_t          *site;     /**< Site for site/location contexts */
    const ib_site_location_t *location; /**< Location for location contexts */

    /* Filters */
    ib_list_t            *filters;     /**< Context enabled filters */

    /* Rules associated with this context */
    ib_rule_context_t    *rules;       /**< Rule context data */
};

#endif /* _IB_ENGINE_PRIVATE_H_ */
