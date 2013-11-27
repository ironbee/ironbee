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

/* This is always re-included to allow for prefixing the symbol names. */

#ifndef _IB_CORE_H_
#define _IB_CORE_H_

/**
 * @file
 * @brief IronBee --- Module
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/engine_types.h>
#include <ironbee/logformat.h>
#include <ironbee/module.h>
#include <ironbee/rule_defs.h>
#include <ironbee/types.h>

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeCore Core
 * @ingroup IronBee
 *
 * Core implements much of IronBee as a module.
 *
 * @{
 */


/**
 * Audit log part generator function.
 *
 * This function is called repetitively to generate the data logged
 * in an audit log part. The function should return zero when there
 * is no more data to log.
 *
 * @param part Audit log part
 * @param chunk Address in which chunk is written
 *
 * @returns Size of the chunk or zero to indicate completion
 */
typedef size_t (*ib_auditlog_part_gen_fn_t)(ib_auditlog_part_t *part,
                                            const uint8_t **chunk);

/* Forward define this structure. */
typedef struct ib_core_audit_cfg_t ib_core_audit_cfg_t;

typedef struct ib_core_cfg_t ib_core_cfg_t;

/**
 * Core audit configuration structure
 */
struct ib_core_audit_cfg_t {
    FILE                *index_fp;      /**< Index file pointer */
    FILE                *fp;            /**< Audit log file pointer */
    const char          *fn;            /**< Audit log file name */
    const char          *full_path;     /**< Audit log full path */
    const char          *temp_path;     /**< Full path to temporary file */
    int                  parts_written; /**< Parts written so far */
    const char          *boundary;      /**< Audit log boundary */
    ib_tx_t             *tx;            /**< Transaction being logged */
    const ib_core_cfg_t *core_cfg;      /**< Core configuration */
};

/** Audit Log */
struct ib_auditlog_t {
    ib_engine_t         *ib;              /**< Engine handle */
    ib_mpool_t          *mp;              /**< Connection memory pool */
    ib_context_t        *ctx;             /**< Config context */
    ib_tx_t             *tx;              /**< Transaction being logged */
    ib_core_audit_cfg_t *cfg_data;        /**< Implementation config data */
    ib_list_t           *parts;           /**< List of parts */
};

/** Audit Log Part */
struct ib_auditlog_part_t {
    ib_auditlog_t      *log;             /**< Audit log */
    const char         *name;            /**< Part name */
    const char         *content_type;    /**< Part content type */
    void               *part_data;       /**< Arbitrary part data */
    void               *gen_data;        /**< Data for generator function */
    ib_auditlog_part_gen_fn_t fn_gen;    /**< Data generator function */
};

/**
 * The possible states of the IronBee audit engine.
 */
typedef enum ib_audit_mode_t {
    IB_AUDIT_MODE_OFF,      /**< Off. No auditing. */
    IB_AUDIT_MODE_ON,       /**< On. Record all. */
    IB_AUDIT_MODE_RELEVANT, /**< Only record relevant information. */
} ib_audit_mode_t;

/**
 * InitVar entry.
 **/
typedef struct ib_core_initvar_t ib_core_initvar_t;
struct ib_core_initvar_t
{
    ib_var_source_t *source;         /**< Source to initialize. */
    const ib_field_t *initial_value; /**< Value to initialize to. */
};

/* Var sources and targets  */
typedef struct ib_core_vars_t ib_core_vars_t;
struct ib_core_vars_t {
    const ib_var_source_t *threat_level;
    ib_var_source_t *request_protocol;
    ib_var_source_t *request_method;
    ib_var_source_t *response_status;
    ib_var_source_t *response_protocol;
    ib_var_source_t *tx_capture;
    const ib_var_source_t *field_name_full;
    ib_var_target_t *flag_block;
};

typedef enum {
    IB_CORE_AUDITLOG_OPENED, /**< An audit log is about to be written. */

    /**
     * And audit log has just been written to a file.
     *
     * That file is about to be closed and renamed to the final file name,
     * but is still open.
     */
    IB_CORE_AUDITLOG_CLOSED
} ib_core_auditlog_event_en;

/**
 * Auditlog event handlers.
 *
 * @param[in] ib IronBee engine.
 * @param[in] tx Transaction.
 * @param[in] event The event type this is.
 * @param[in] audit_log The audit log.
 * @param[in] cbdata Callback data.
 */
typedef ib_status_t (*ib_core_auditlog_fn_t)(
    ib_engine_t               *ib,
    ib_tx_t                   *tx,
    ib_core_auditlog_event_en  event,
    ib_auditlog_t             *auditlog,
    void                      *cbdata
);

/**
 * Core configuration.
 */
struct ib_core_cfg_t {
    const char       *log_uri;           /**< Log URI */
    FILE             *log_fp;            /**< File pointer for log. */
    const char       *logevent;          /**< Active logevent provider key */
    ib_list_t        *initvar_list;      /**< List of ib_core_initvar_t for InitVar */
    ib_list_t        *mancoll_list;      /**< List of ib_managed_collection_t */
    ib_num_t          buffer_req;        /**< Request buffering options */
    ib_num_t          buffer_res;        /**< Response buffering options */
    ib_audit_mode_t   audit_engine;      /**< Audit engine status */
    ib_num_t          auditlog_dmode;    /**< Audit log dir create mode */
    ib_num_t          auditlog_fmode;    /**< Audit log file create mode */
    ib_num_t          auditlog_parts;    /**< Audit log parts */
    const char       *auditlog_index_fmt;/**< Audit log index format string */
    const ib_logformat_t *auditlog_index_hp; /**< Audit log index fmt helper */
    const char       *auditlog_dir;      /**< Audit log base directory */
    const char       *auditlog_sdir_fmt; /**< Audit log sub-directory format */
    /**
     * List of @ref ib_core_auditlog_fn_t and associated callback data.
     *
     * The particular struct is used as the list element is private
     * to core and necessary to hold the function pointer.
     *
     * C99 does not allow for function pointers to be stored in void*
     * containers.
     */
    ib_list_t        *auditlog_handlers;
    const char       *audit;             /**< Active audit provider key */
    const char       *data;              /**< Active data provider key */
    const char       *module_base_path;  /**< Module base path. */
    const char       *rule_base_path;    /**< Rule base path. */
    ib_num_t          rule_log_flags;    /**< Rule execution logging flags */
    ib_num_t          rule_log_level;    /**< Rule execution logging level */
    const char       *rule_debug_str;    /**< Rule debug logging level */
    ib_num_t          rule_debug_level;  /**< Rule debug logging level */
    ib_block_method_t block_method;     /**< What blocking method to use. */
    //! Status code used when blocking with @ref IB_BLOCK_METHOD_STATUS.
    ib_num_t          block_status;
    ib_num_t inspection_engine_options; /**< Inspection engine options */
    ib_num_t protection_engine_options; /**< Protection engine options */
    ib_tx_limits_t    limits;            /**< Limits used by this core. */
    ib_core_vars_t   *vars;             /**< Var sources and targets. */
};

/**
 * Get the core module
 *
 * @param[in] ib IronBee engine
 *
 * @returns Pointer to core module structure
 */
ib_module_t *ib_core_module(
    const ib_engine_t *ib);

/**
 * Fetch the core module configuration data from the configuration context.
 *
 * @param ctx Configuration context
 * @param pcfg Address which module config data is written
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_core_context_config(
    const ib_context_t  *ctx,
    ib_core_cfg_t      **pcfg);


/**
 * Retrieve the limits section of the core configuration for @a ctx.
 *
 * @param[in] ctx The current configuration context.
 * @param[out] limits Writes a pointer to the current limits.
 *
 * @return
 * - IB_OK On success.
 * - Other on error fetching the context configuration for core.
 */
ib_status_t DLL_PUBLIC ib_core_limits_get(
    ib_context_t *ctx,
    const ib_tx_limits_t **limits);

/**
 * Add a @ref ib_core_auditlog_fn_t to the core module.
 *
 * @param[in] ctx The context to add this to.
 * @param[in] auditlog_fn Audit log handler function.
 * @param[in] auditlog_cbdata Callback data for @a auditlog_fn.
 *
 * @returns
 * - IB_OK On success.
 * - Other on failure.
 */
ib_status_t DLL_PUBLIC ib_core_add_auditlog_handler(
    ib_context_t          *ctx,
    ib_core_auditlog_fn_t  auditlog_fn,
    void                  *auditlog_cbdata
);

/**
 * Dispatch a audit log to all audit log handlers.
 *
 * @param[in] tx Transaction.
 * @param[in] event The type of event.
 * @param[in] auditlog The audit log.
 *
 * @returns
 * - IB_OK
 * - Other if there was an error dispatching the event. Errors in
 *   handlers are logged but do not cause this dispatch routine to fail.
 */
ib_status_t DLL_PUBLIC ib_core_dispatch_auditlog(
    ib_tx_t                   *tx,
    ib_core_auditlog_event_en  event,
    ib_auditlog_t             *auditlog
);

/**
 * @} IronBeeCore
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_CORE_H_ */
