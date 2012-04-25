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

#ifndef _IB_ENGINE_H_
#define _IB_ENGINE_H_

/**
 * @file
 * @brief IronBee
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/uuid.h>
#include <ironbee/hash.h>
#include <ironbee/field.h>
#include <ironbee/stream.h>
#include <ironbee/clock.h>
#include <ironbee/parsed_content.h>
#include <ironbee/engine_types.h>

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup IronBee IronBee
 * @{
 * @addtogroup IronBeeEngine Engine
 *
 * This is the API for the IronBee engine.
 *
 * @{
 */

/**
 * Configuration Context Function.
 *
 * This function returns IB_OK if the context should be used.
 *
 * @param ctx[in] Configuration context
 * @param type[in] Connection structure
 * @param ctxdata[in] Context data (type dependent: conn or tx)
 * @param cbdata[in] Callback data (fn_ctx_data from context)
 *
 * @returns Status code
 */
typedef ib_status_t (*ib_context_fn_t)(const ib_context_t *ctx,
                                       ib_ctype_t type,
                                       void *ctxdata,
                                       void *cbdata);

/**
 * Configuration Context Site Function.
 *
 * This function returns IB_OK if there is a site associated with
 * the context.
 *
 * @param ctx[in] Configuration context
 * @param psite[out] Address which site is written if non-NULL
 * @param cbdata[in] Callback data (fn_ctx_data from context)
 *
 * @returns Status code
 */
typedef ib_status_t (*ib_context_site_fn_t)(const ib_context_t *ctx,
                                            ib_site_t **psite,
                                            void *cbdata);


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

/** Audit Log */
struct ib_auditlog_t {
    ib_engine_t        *ib;              /**< Engine handle */
    ib_mpool_t         *mp;              /**< Connection memory pool */
    ib_context_t       *ctx;             /**< Config context */
    ib_tx_t            *tx;              /**< Transaction being logged */
    void               *cfg_data;        /**< Implementation config data */
    ib_list_t          *parts;           /**< List of parts */
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

/// @todo Maybe a ib_create_ex() that takes a config?

/**
 * Create an engine handle.
 *
 * After creating the engine, the caller must configure defaults, such as
 * initial logging parameters, and then call @ref ib_engine_init() to
 * initialize the engine configuration context.
 *
 * @param pib Address which new handle is written
 * @param plugin Information on the server plugin instantiating the engine
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_engine_create(ib_engine_t **pib, void *plugin);

/**
 * Initialize the engine configuration context.
 *
 * @param ib Engine handle
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_engine_init(ib_engine_t *ib);

/**
 * Create a main context to operate in.
 *
 * @param[in] ib IronBee engine that contains the ectx that we will use
 *            in creating the main context. The main context
 *            will be assigned to ib->ctx if it is successfully created.
 *
 * @returns IB_OK or the result of
 *          ib_context_create(ctx, ib, ib->ectx, NULL, NULL, NULL).
 */
ib_status_t ib_engine_context_create_main(ib_engine_t *ib);

/**
 * Get a module by name.
 *
 * @param ib Engine handle
 * @param name Module name
 * @param pm Address which module will be written
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_engine_module_get(ib_engine_t *ib,
                                            const char * name,
                                            ib_module_t **pm);

/**
 * Get the main engine memory pool.
 *
 * @param ib Engine handle
 *
 * @returns Memory pool
 */
ib_mpool_t DLL_PUBLIC *ib_engine_pool_main_get(ib_engine_t *ib);

/**
 * Get the engine configuration memory pool.
 *
 * This pool should be used to configure the engine.
 *
 * @param ib Engine handle
 *
 * @returns Memory pool
 */
ib_mpool_t DLL_PUBLIC *ib_engine_pool_config_get(ib_engine_t *ib);

/**
 * Get the engine temporary memory pool.
 *
 * This pool should be destroyed by the plugin after the
 * configuration phase. Therefore is should not be used for
 * anything except temporary allocations which are required
 * for performing configuration.
 *
 * @param ib Engine handle
 *
 * @returns Memory pool
 */
ib_mpool_t DLL_PUBLIC *ib_engine_pool_temp_get(ib_engine_t *ib);

/**
 * Destroy the engine temporary memory pool.
 *
 * This should be called by the plugin after configuration is
 * completed. After this call, any allocations in the temporary
 * pool will be invalid and no future allocations can be made to
 * to this pool.
 *
 * @param ib Engine handle
 */
void DLL_PUBLIC ib_engine_pool_temp_destroy(ib_engine_t *ib);

/**
 * Destroy an engine.
 *
 * @param ib Engine handle
 *
 * @returns Status code
 */
void DLL_PUBLIC ib_engine_destroy(ib_engine_t *ib);

/**
 * Create a configuration context.
 *
 * @param pctx Address which new context is written
 * @param ib Engine handle
 * @param parent Parent context (or NULL)
 * @param ctx_type String to identify context type (i.e. "Site", "Main")
 * @param ctx_name String to identify context ("foo.com", "main")
 * @param fn_ctx Context function
 * @param fn_ctx_site Context site lookup function
 * @param fn_ctx_data Context function data
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_context_create(ib_context_t **pctx,
                                         ib_engine_t *ib,
                                         ib_context_t *parent,
                                         const char *ctx_type,
                                         const char *ctx_name,
                                         ib_context_fn_t fn_ctx,
                                         ib_context_site_fn_t fn_ctx_site,
                                         void *fn_ctx_data);

/**
 * Open a configuration context.
 *
 * This causes ctx_open functions to be executed for each module
 * registered in a configuration context.
 *
 * @param[in] ctx Config context
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_context_open(ib_context_t *ctx);

/**
 * Close a configuration context.
 *
 * This causes ctx_close functions to be executed for each module
 * registered in a configuration context.  It should be called
 * after a configuration context is fully configured.
 *
 * @param[in] ctx Config context
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_context_close(ib_context_t *ctx);

/**
 * Get the parent context.
 *
 * @param ctx Context
 *
 * @returns Parent context
 */
ib_context_t DLL_PUBLIC *ib_context_parent_get(const ib_context_t *ctx);

/**
 * Set the parent context.
 *
 * @param ctx Context
 * @param parent Parent context
 */
void DLL_PUBLIC ib_context_parent_set(ib_context_t *ctx,
                                      ib_context_t *parent);

/**
 * Get the site associated with the context.
 *
 * @param ctx Configuration context
 *
 * @returns Site or NULL if none is associated
 */
ib_site_t DLL_PUBLIC *ib_context_site_get(const ib_context_t *ctx);

/**
 * Get the type identifier of the context.
 *
 * @param ctx Configuration context
 *
 * @returns Type string (or NULL)
 */
const char DLL_PUBLIC *ib_context_type_get(const ib_context_t *ctx);

/**
 * Get the name identifier of the context.
 *
 * @param ctx Configuration context
 *
 * @returns Name string (or NULL)
 */
const char DLL_PUBLIC *ib_context_name_get(const ib_context_t *ctx);

/**
 * Get the full name identifier of the context.
 *
 * @param ctx Configuration context
 *
 * @returns Full name string (or NULL)
 */
const char DLL_PUBLIC *ib_context_full_get(const ib_context_t *ctx);

/**
 * Destroy a configuration context.
 *
 * @param ctx Configuration context
 */
void DLL_PUBLIC ib_context_destroy(ib_context_t *ctx);

/**
 * Get the IronBee engine object.
 *
 * This returns a pointer to the IronBee engine associated with
 * the given context.
 *
 * @param ctx Config context
 *
 * @returns Pointer to the engine.
 */
ib_engine_t DLL_PUBLIC *ib_context_get_engine(const ib_context_t *ctx);

/**
 * Get the engine (startup) configuration context.
 *
 * @param ib Engine handle
 *
 * @returns Status code
 */
ib_context_t *ib_context_engine(const ib_engine_t *ib);

/**
 * Get the main (default) configuration context.
 *
 * @param ib Engine handle
 *
 * @returns Status code
 */
ib_context_t DLL_PUBLIC *ib_context_main(const ib_engine_t *ib);

/**
 * Initialize a configuration context.
 *
 * @param ctx Configuration context
 * @param base Base address of the structure holding the values
 * @param init Configuration map initialization structure
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_context_init_cfg(ib_context_t *ctx,
                                           void *base,
                                           const ib_cfgmap_init_t *init);

/**
 * Fetch the named module configuration data from the configuration context.
 *
 * @param ctx Configuration context
 * @param m Module
 * @param pcfg Address which module config data is written
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_context_module_config(ib_context_t *ctx,
                                                ib_module_t *m,
                                                void *pcfg);

/**
 * Set a value in the config context.
 *
 * @param ctx Configuration context
 * @param name Variable name
 * @param pval Pointer to value to set
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_context_set(ib_context_t *ctx,
                                      const char *name,
                                      void *pval);


/**
 * Set the index log value for this logging context.
 *
 * In addition to setting ctx->auditlog->index this will also close
 * index_fp if that FILE* is not NULL.
 *
 * If ctx->auditlog->owner does not match @a ctx then @a ctx is not the
 * owning context and a new auditlog structure is allocated and initialized.
 *
 * If ctx->auditlog is NULL a new auditlog structure is also, likewise,
 * allocated and initialized. All of these changes are done with a mutex
 * that is part of the auditlog initialization.
 *
 * @param[in,out] ctx The context to set the index value in.
 * @param[in] idx The index value to be copied into the context.
 *
 * @returns IB_OK, IB_EALLOC or the status of ib_lock_init.
 */
ib_status_t ib_context_set_auditlog_index(ib_context_t *ctx, const char* idx);

/**
 * Set a address value in the config context.
 *
 * @param ctx Configuration context
 * @param name Variable name
 * @param val Numeric value
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_context_set_num(ib_context_t *ctx,
                                          const char *name,
                                          ib_num_t val);

/**
 * Set a string value in the config context.
 *
 * @param ctx Configuration context
 * @param name Variable name
 * @param val String value
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_context_set_string(ib_context_t *ctx,
                                      const char *name,
                                      const char *val);

/**
 * Get a value and length from the config context.
 *
 * @param ctx Configuration context
 * @param name Variable name
 * @param pval Address to which the value is written
 * @param ptype Address to which the value type is written if non-NULL
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_context_get(ib_context_t *ctx,
                                      const char *name,
                                      void *pval, ib_ftype_t *ptype);

/**
 * Default Site/Location context chooser.
 *
 * @param ctx Configuration context
 * @param type Context data type
 * @param ctxdata Context data
 * @param cbdata Chooser callback data
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_context_siteloc_chooser(const ib_context_t *ctx,
                                                  ib_ctype_t type,
                                                  void *ctxdata,
                                                  void *cbdata);


/**
 * Default Site/Location context chooser.
 *
 * @param ctx Configuration context
 * @param psite Address which site is written
 * @param cbdata Chooser callback data
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_context_site_lookup(const ib_context_t *ctx,
                                              ib_site_t **psite,
                                              void *cbdata);

/**
 * Create a connection structure.
 *
 * @param ib Engine handle
 * @param pconn Address which new connection is written
 * @param pctx Plugin connection context
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_conn_create(ib_engine_t *ib,
                                      ib_conn_t **pconn,
                                      void *pctx);

/**
 * Set connection flags.
 *
 * @param conn Connection structure
 * @param flag Flags
 */
#define ib_conn_flags_set(conn, flag) do { (conn)->flags |= (flag); } while(0)

/**
 * Check if connection flags are all set.
 *
 * @param conn Connection structure
 * @param flag Flags
 *
 * @returns All current flags
 */
#define ib_conn_flags_isset(conn, flag) ((conn)->flags & (flag) ? 1 : 0)

/**
 * Create a connection data structure.
 *
 * @param conn Connection structure
 * @param pconndata Address which new connection data is written
 * @param dalloc Size of data to allocate
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_conn_data_create(ib_conn_t *conn,
                                           ib_conndata_t **pconndata,
                                           size_t dalloc);

/**
 * Destroy a connection structure.
 *
 * @param conn Connection structure
 */
void DLL_PUBLIC ib_conn_destroy(ib_conn_t *conn);

/**
 * Create a transaction structure.
 *
 * @param ib Engine handle
 * @param ptx Address which new transaction is written
 * @param conn Connection structure
 * @param pctx Plugin transaction context
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_tx_create(ib_engine_t *ib,
                                    ib_tx_t **ptx,
                                    ib_conn_t *conn,
                                    void *pctx);

/**
 * Set transaction flags.
 *
 * @param tx Transaction structure
 * @param flag Flags
 *
 * @returns All current flags
 */
#define ib_tx_flags_set(tx, flag) do { (tx)->flags |= (flag); } while(0)

/**
 * Check if transaction flags are all set.
 *
 * @param tx Transaction structure
 * @param flag Flags
 *
 * @returns All current flags
 */
#define ib_tx_flags_isset(tx, flag) ((tx)->flags & (flag) ? 1 : 0)

/**
 * Mark transaction as not having a body.
 *
 * @param tx Transaction structure
 */
#define ib_tx_mark_nobody(tx) ib_tx_flags_set(tx, IB_TX_FREQ_NOBODY)

/**
 * Destroy a transaction structure.
 *
 * @param tx Transaction structure
 */
void DLL_PUBLIC ib_tx_destroy(ib_tx_t *tx);

/**
 * Create a site.
 *
 * @param psite Address where site will be written
 * @param ib Engine
 * @param name Site name
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_site_create(ib_site_t **psite,
                                      ib_engine_t *ib,
                                      const char *name);

/**
 * Add IP address to a site.
 *
 * @param site Site
 * @param ip IP address to add
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_site_address_add(ib_site_t *site,
                                           const char *ip);

/**
 * Validate IP address for a site.
 *
 * @param site Site
 * @param ip IP address to add
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_site_address_validate(ib_site_t *site,
                                                const char *ip);

/**
 * Add hostname to a site.
 *
 * @param site Site
 * @param host Hostname to add
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_site_hostname_add(ib_site_t *site,
                                            const char *host);

/**
 * Validate hostname for a site.
 *
 * @param site Site
 * @param host Hostname to validate
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_site_hostname_validate(ib_site_t *site,
                                                 const char *host);

/**
 * Create a location for a site.
 *
 * @param ploc Address where location will be written
 * @param site Site
 * @param path Location path
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_site_loc_create(ib_site_t *site,
                                          ib_loc_t **ploc,
                                          const char *path);

/**
 * Create a default location for a site.
 *
 * @param ploc Address where location will be written
 * @param site Site
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_site_loc_create_default(ib_site_t *site,
                                                  ib_loc_t **ploc);


/**
 * @defgroup IronBeeEngineState State
 *
 * This API allows for sending state and data information to the engine.
 *
 * The following diagrams the general state processing order of a typical
 * transaction that is allowed to be fully processed.
 *
 * @dot
 * digraph legend1 {
 *   plugin_state [label="Plugin\nstate",shape=octagon]
 *   plugin_async_state [label="Plugin\nasyncronous\nstate",shape=octagon,peripheries=2]
 *   parser_state [label="Parser\nstate",shape=ellipse]
 *   engine_state [label="Engine\nstate",shape=diamond]
 *   handler_state [label="Handler\nstate",shape=parallelogram]
 *   entity [label="Entity",shape=box]
 *   note [label="Note",shape=note]
 *   {rank=same; plugin_state plugin_async_state parser_state engine_state handler_state note}
 * }
 * @enddot
 * @dot
 * digraph legend2 {
 *   edge1a [style=invis]
 *   edge1b [style=invis]
 *   edge2a [style=invis]
 *   edge2b [style=invis]
 *   edge3a [style=invis]
 *   edge3b [style=invis]
 *   { rank=same; edge1a edge1b edge2a edge2b edge3a edge3b }
 *
 *   edge1a -> edge1b [label="Ordered Transition\nto State"]
 *   edge2a -> edge2b [label="Influences Transition\nto State",style=dotted,arrowhead=none]
 *   edge3a -> edge3b [label="Alternate Transition\nto State",style=dashed]
 * }
 * @enddot
 * @dot
 * digraph legend3 {
 *   conn_style [label="connection event",style=bold,shape=box]
 *   tx_style [label="transaction event",style=filled,fillcolor="#e6e6e6",shape=box]
 *   data_style [label="data event",style=solid,shape=box]
 *   { rank=same; conn_style tx_style data_style }
 * }
 * @enddot
 * @dot
 * digraph legend3 {
 *   note1 [label="*  Automatically triggered if\l    not done explicitly (some\l    parsers may not be capable).\l",style=bold,shape=plaintext]
 *   note2 [label="** Special handler states allowing\l    modules to do any setup/init\l    within their context.\l",style=bold,shape=plaintext]
 *   { rank=same; note1 note2 }
 * }
 * @enddot
 *
 * Plugin states are triggered by the plugin and parser states by the
 * parser. These states cause the engine to trigger both the engine and
 * handler states. The engine states are meant to be synchronization
 * points. The handler states are meant to be handled by modules to do
 * detection and take actions, while the plugin and parser states are
 * to be used to generate fields and anything else needed in the handler
 * states.
 *
 * - Connection event hook callbacks receive a @ref ib_conn_t parameter.
 * - Connection Data event hook callbacks receive a @ref ib_conndata_t
 *   parameter.
 * - Transaction event hook callbacks receive a @ref ib_tx_t parameter.
 * - Transaction Data event hook callbacks receive a @ref ib_txdata_t
 *   parameter.
 *
 * @note Config contexts and some fields are populated during the plugin
 *       events and thus the following handler event is what should be used
 *       to use these contexts and fields for detection.
 *
 * The following diagram shows a complete connection from start to finish.
 *
 * @dot
 * digraph states {
 *
 *   start [label="start",style=bold,shape=plaintext]
 *   finish [label="finish",style=bold,shape=plaintext]
 *
 *   incoming [label="Raw (unparsed)\nIncoming\nData",style=solid,shape=box]
 *   outgoing [label="Raw (unparsed)\nOutgoing\nData",style=solid,shape=box]
 *   request [label="HTTP (parsed)\nRequest\nData",style=solid,shape=box]
 *   response [label="HTTP (parsed)\nResponse\nData",style=solid,shape=box]
 *
 *   context_conn_selected [label="At this point the connection context is\nselected. Events that follow will use the context\nselected here, which may impose a different\nconfiguration than previous events. Anything\nused in the context selection process must\nbe generated in a previous event handler.",style=bold,shape=note,URL="\ref handle_context_conn_event"]
 *   context_tx_selected [label="At this point the transaction context is\nselected. Events that follow will use the context\nselected here, which may impose a different\nconfiguration than previous events. Anything\nused in the context selection process must\nbe generated in a previous event handler.\nAdditionally, any transaction data filters will\nnot be called until after this point so that\nfilters will be called with a single context.",style=filled,fillcolor="#e6e6e6",shape=note,URL="\ref handle_context_tx_event"]
 *
 *   conn_started_event [label="conn_started",style=bold,shape=diamond,URL="\ref conn_started_event"]
 *   conn_opened_event [label="conn_opened",style=bold,shape=octagon,URL="\ref conn_opened_event"]
 *   conn_data_in_event [label="conn_data_in",style=solid,shape=octagon,peripheries=2,URL="\ref conn_data_in_event"]
 *   conn_data_out_event [label="conn_data_out",style=solid,shape=octagon,peripheries=2,URL="\ref conn_data_out_event"]
 *   conn_closed_event [label="conn_closed",style=bold,shape=octagon,URL="\ref conn_closed_event"]
 *   conn_finished_event [label="conn_finished",style=bold,shape=diamond,URL="\ref conn_finished_event"]
 *
 *   tx_started_event [label="tx_started",style=filled,fillcolor="#e6e6e6",shape=diamond,URL="\ref tx_started_event"]
 *   tx_data_in_event [label="tx_data_in",style=solid,shape=octagon,peripheries=2,URL="\ref tx_data_in_event"]
 *   tx_process_event [label="tx_process",style=filled,fillcolor="#e6e6e6",shape=diamond,URL="\ref tx_process_event"]
 *   tx_data_out_event [label="tx_data_out",style=solid,shape=octagon,peripheries=2,URL="\ref tx_data_out_event"]
 *   tx_finished_event [label="tx_finished",style=filled,fillcolor="#e6e6e6",shape=diamond,URL="\ref tx_finished_event"]
 *
 *   request_started_event [label="request_started *",style=filled,fillcolor="#e6e6e6",shape=ellipse,URL="\ref request_started_event"]
 *   request_headers_event [label="request_headers",style=filled,fillcolor="#e6e6e6",shape=ellipse,URL="\ref request_headers_event"]
 *   request_body_data_event [label="request_body",style=filled,fillcolor="#e6e6e6",shape=ellipse,URL="\ref request_body_data_event"]
 *   request_finished_event [label="request_finished",style=filled,fillcolor="#e6e6e6",shape=ellipse,URL="\ref request_finished_event"]
 *   response_started_event [label="response_started *",style=filled,fillcolor="#e6e6e6",shape=ellipse,URL="\ref response_started_event"]
 *   response_headers_event [label="response_headers",style=filled,fillcolor="#e6e6e6",shape=ellipse,URL="\ref response_headers_event"]
 *   response_body_data_event [label="response_body",style=filled,fillcolor="#e6e6e6",shape=ellipse,URL="\ref response_body_data_event"]
 *   response_finished_event [label="response_finished",style=filled,fillcolor="#e6e6e6",shape=ellipse,URL="\ref response_finished_event"]
 *
 *   handle_context_conn_event [label="handle_context_conn **",style=bold,shape=parallelogram,URL="\ref handle_context_conn_event"]
 *   handle_connect_event [label="handle_connect",style=bold,shape=parallelogram,URL="\ref handle_connect_event"]
 *   handle_context_tx_event [label="handle_context_tx **",style=filled,fillcolor="#e6e6e6",shape=parallelogram,URL="\ref handle_context_tx_event"]
 *   handle_request_headers_event [label="handle_request_headers",style=filled,fillcolor="#e6e6e6",shape=parallelogram,URL="\ref handle_request_headers_event"]
 *   handle_request_event [label="handle_request",style=filled,fillcolor="#e6e6e6",shape=parallelogram,URL="\ref handle_request_event"]
 *   handle_response_headers_event [label="handle_response_headers",style=filled,fillcolor="#e6e6e6",shape=parallelogram,URL="\ref handle_response_headers_event"]
 *   handle_response_event [label="handle_response",style=filled,fillcolor="#e6e6e6",shape=parallelogram,URL="\ref handle_response_event"]
 *   handle_disconnect_event [label="handle_disconnect",style=bold,shape=parallelogram,URL="\ref handle_disconnect_event"]
 *   handle_postprocess_event [label="handle_postprocess",style=filled,fillcolor="#e6e6e6",shape=parallelogram,URL="\ref handle_postprocess_event"]
 *
 *   // These are just for organizational purposes
 *   conn_started_event -> incoming [style=invis,weight=5.0]
 *   conn_data_in_event -> request [style=invis,weight=100.0]
 *   conn_data_out_event -> response [style=invis,weight=100.0]
 *   tx_started_event -> request [style=invis,weight=5.0]
 *   tx_started_event -> tx_finished_event [style=invis,weight=5.0]
 *   tx_process_event -> outgoing [style=invis]
 *   response_started_event -> response [style=invis,weight=5.0]
 *
 *   start -> conn_started_event [weight=500.0]
 *   incoming -> conn_data_in_event [dir=none,weight=100.0]
 *   conn_data_in_event -> conn_data_in_event [weight=0.1]
 *   conn_data_in_event -> tx_data_in_event [weight=0.1]
 *   tx_data_in_event -> tx_data_in_event [weight=0.1]
 *
 *   conn_started_event -> conn_opened_event [weight=100.0]
 *   conn_opened_event -> context_conn_selected [weight=100.0]
 *   context_conn_selected -> handle_context_conn_event [weight=100.0]
 *   handle_context_conn_event -> handle_connect_event [weight=100.0]
 *   handle_connect_event -> tx_started_event [weight=100.0]
 *
 *   request -> tx_data_in_event [dir=none,weight=100.0]
 *   tx_data_in_event -> request_started_event [style=dotted,arrowhead=none,weight=1.5]
 *   tx_data_in_event -> request_headers_event [style=dotted,arrowhead=none,weight=1.5]
 *   tx_data_in_event -> request_body_data_event [style=dotted,arrowhead=none,weight=1.5]
 *
 *   tx_started_event -> request_started_event [weight=5.0]
 *   request_started_event -> request_headers_event [weight=1.0]
 *   request_headers_event -> context_tx_selected [weight=1.0]
 *   context_tx_selected -> handle_context_tx_event [weight=1.0]
 *   handle_context_tx_event -> handle_request_headers_event [weight=1.0]
 *   handle_request_headers_event -> request_started_event [label="HTTP\nPipeline\nRequest",style=dashed,weight=10.0]
 *   handle_request_headers_event -> request_body_data_event [weight=1.0]
 *   request_body_data_event -> handle_request_event [weight=1.0]
 *   handle_request_event -> request_finished_event [weight=1.0]
 *   request_finished_event -> tx_process_event [weight=1.0]
 *
 *   tx_process_event -> response_started_event [weight=1.0]
 *
 *   response -> tx_data_out_event [dir=none,weight=100.0]
 *   outgoing -> conn_data_out_event [dir=none,weight=100.0]
 *   conn_data_out_event -> conn_data_out_event [weight=0.1]
 *   conn_data_out_event -> tx_data_out_event [weight=0.1]
 *   tx_data_out_event -> tx_data_out_event [weight=0.1]
 *
 *   tx_data_out_event -> response_started_event [style=dotted,arrowhead=none,weight=1.5]
 *   tx_data_out_event -> response_headers_event [style=dotted,arrowhead=none,weight=1.5]
 *   tx_data_out_event -> response_body_data_event [style=dotted,arrowhead=none,weight=1.5]
 *
 *   response_started_event -> response_headers_event [weight=1.0]
 *   response_headers_event -> handle_response_headers_event [weight=1.0]
 *   handle_response_headers_event -> response_body_data_event [weight=1.0]
 *   response_body_data_event -> handle_response_event [weight=1.0]
 *   handle_response_event -> response_finished_event [weight=5.0]
 *   response_finished_event -> response_started_event [label="HTTP\nPipeline\nResponse",style=dashed,weight=10.0]
 *
 *   response_finished_event -> handle_postprocess_event [weight=5.0]
 *   handle_postprocess_event -> tx_finished_event [weight=5.0]
 *
 *   tx_finished_event -> tx_started_event [weight=5.0,constraint=false]
 *   tx_finished_event -> conn_closed_event [weight=5.0]
 *
 *   conn_closed_event -> handle_disconnect_event [weight=5.0]
 *   handle_disconnect_event -> conn_finished_event [weight=10.0]
 *
 *   conn_finished_event -> finish [weight=500.0]
 * }
 * @enddot
 *
 * @{
 */

/**
 * State Event Types
 *
 * @warning Remember to update ib_state_event_name_list[] when names change.
 * @warning Remember to update ib_state_event_hook_types[] when types change.
 */
typedef enum {
    /* Engine States */
    conn_started_event,            /**< Connection started */
    conn_finished_event,           /**< Connection finished */
    tx_started_event,              /**< Transaction started */
    tx_process_event,              /**< Transaction is about to be processed */
    tx_finished_event,             /**< Transaction finished */

    /* Handler States */
    handle_context_conn_event,     /**< Handle connection context chosen */
    handle_connect_event,          /**< Handle a connect */
    handle_context_tx_event,       /**< Handle transaction context chosen */
    handle_request_headers_event,  /**< Handle the request headers */
    handle_request_event,          /**< Handle the full request */
    handle_response_headers_event, /**< Handle the response headers */
    handle_response_event,         /**< Handle the full response */
    handle_disconnect_event,       /**< Handle a disconnect */
    handle_postprocess_event,      /**< Handle transaction post processing */

    /* Plugin States */
    cfg_started_event,             /**< Plugin notified config started */
    cfg_finished_event,            /**< Plugin notified config finished */
    conn_opened_event,             /**< Plugin notified connection opened */
    conn_data_in_event,            /**< Plugin notified of incoming data */
    conn_data_out_event,           /**< Plugin notified of outgoing data */
    conn_closed_event,             /**< Plugin notified connection closed */

    /* Parser States */
    tx_data_in_event,              /**< Parser notified of request data */
    tx_data_out_event,             /**< Parser notified of response data */
    request_started_event,         /**< Parser notified request has started */
    request_headers_event,         /**< Parser notified of request headers */
    request_headers_data_event,    /**< Parser notified of request headers data */
    request_body_data_event,       /**< Parser notified of request body */
    request_finished_event,        /**< Parser notified request finished */
    response_started_event,        /**< Parser notified response started */
    response_headers_event,        /**< Parser notified of response headers */
    response_headers_data_event,   /**< Parser notified of response headers data*/
    response_body_data_event,      /**< Parser notified of response body */
    response_finished_event,       /**< Parser notified response finished */

    /* Not an event, but keeps track of the number of events. */
    IB_STATE_EVENT_NUM,
} ib_state_event_type_t;

/**
 * State Event Hook Types
 **/
typedef enum {
    IB_STATE_HOOK_NULL,     /**< Hook has no parameter */
    IB_STATE_HOOK_INVALID,  /**< Something went wrong. */
    IB_STATE_HOOK_CONN,     /**< Hook received ib_conn_t */
    IB_STATE_HOOK_CONNDATA, /**< Hook received ib_conndata_t */
    IB_STATE_HOOK_TX,       /**< Hook received ib_tx_t */
    IB_STATE_HOOK_TXDATA,   /**< Hook received ib_txdata_t */
    IB_STATE_HOOK_REQLINE,  /**< Hook received ib_parsed_req_t. */
    IB_STATE_HOOK_RESPLINE, /**< Hook received ib_parsed_resp_t. */
    IB_STATE_HOOK_HEADER    /**< Hook received ib_parsed_header_t. */
} ib_state_hook_type_t;

/**
 * Hook type for an event.
 *
 * \param[in] event Event type.
 * \return Hook type or IB_STATE_HOOK_INVALID if bad event.
 **/
ib_state_hook_type_t ib_state_hook_type(ib_state_event_type_t event);

/**
 * Dataless Event Hook Callback Function.
 *
 * @param ib Engine handle
 * @param event Which event trigger the callback.
 * @param cbdata Callback data
 */
typedef ib_status_t (*ib_state_null_hook_fn_t)(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    void *cbdata
);


/**
 * Data event for parsed headers.
 *
 * @param[in] ib Engine handle
 * @param[in] tx Transaction.
 * @param[in] event Which event trigger the callback.
 * @param[in] headers Parsed connection headers.
 * @param[in] cbdata Callback data
 */
typedef ib_status_t (*ib_state_headers_data_fn_t)(
    ib_engine_t *ib,
    ib_tx_t *tx,
    ib_state_event_type_t event,
    ib_parsed_header_t *headers,
    void *cbdata);

/**
 * Data event for the start of a request.
 *
 * This provides a request line parsed from the start of the request.
 *
 * @param[in] ib Engine handle
 * @param[in] tx Transaction.
 * @param[in] event Which event trigger the callback.
 * @param[in] line The parsed request line.
 * @param[in] cbdata Callback data
 */
typedef ib_status_t (*ib_state_request_line_fn_t)(
    ib_engine_t *ib,
    ib_tx_t *tx,
    ib_state_event_type_t event,
    ib_parsed_req_line_t *line,
    void *cbdata);

/**
 * Data event for the start of a response.
 *
 * This provides a response line parsed from the start of the response.
 *
 * @param[in] ib Engine handle
 * @param[in] tx Transaction.
 * @param[in] event Which event trigger the callback.
 * @param[in] line The parsed response line.
 * @param[in] cbdata Callback data
 */
typedef ib_status_t (*ib_state_response_line_fn_t)(
    ib_engine_t *ib,
    ib_tx_t *tx,
    ib_state_event_type_t event,
    ib_parsed_resp_line_t *line,
    void *cbdata);

/**
 * Connection Event Hook Callback Function.
 *
 * @param[in] ib Engine handle
 * @param[in] tx Transaction.
 * @param[in] event Which event trigger the callback.
 * @param[in] conn Connection.
 * @param[in] cbdata Callback data
 */
typedef ib_status_t (*ib_state_conn_hook_fn_t)(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_conn_t *conn,
    void *cbdata
);

/**
 * Connection Data Event Hook Callback Function.
 *
 * @param[in] ib Engine handle
 * @param[in] tx Transaction.
 * @param[in] event Which event trigger the callback.
 * @param[in] conndata Connection data.
 * @param[in] cbdata Callback data
 */
typedef ib_status_t (*ib_state_conndata_hook_fn_t)(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_conndata_t *conndata,
    void *cbdata
);

/**
 * Transaction Event Hook Callback Function.
 *
 * This matches the NULL callback type as tx is already passed.
 *
 * @param[in] ib Engine handle
 * @param[in] tx Transaction.
 * @param[in] event Which event trigger the callback.
 * @param[in] cbdata Callback data
 */
typedef ib_status_t (*ib_state_tx_hook_fn_t)(
    ib_engine_t *ib,
    ib_tx_t *tx,
    ib_state_event_type_t event,
    void *cbdata
);

/**
 * Transaction Data Event Hook Callback Function.
 *
 * @param[in] ib Engine handle
 * @param[in] tx Transaction.
 * @param[in] event Which event trigger the callback.
 * @param[in] txdata Transaction data.
 * @param[in] cbdata Callback data
 */
typedef ib_status_t (*ib_state_txdata_hook_fn_t)(
    ib_engine_t *ib,
    ib_tx_t *tx,
    ib_state_event_type_t event,
    ib_txdata_t *txdata,
    void *cbdata
);

/**
 * Resolve an event name.
 *
 * @param event Event type
 *
 * @returns Statically allocated event name
 */
const char *ib_state_event_name(ib_state_event_type_t event);

/**
 * @} QEngineState
 */


/**
 * @defgroup IronBeeEngineHooks Hooks
 * @{
 */

/* No data */

/**
 * Register a callback for a no data event.
 *
 * @param ib Engine handle
 * @param event Event
 * @param cb The callback to register
 * @param cdata Data passed to the callback (or NULL)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hook_null_register(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_null_hook_fn_t cb,
    void *cdata
);

/**
 * Unregister a callback for a no data event.
 *
 * @param ib Engine handle
 * @param event Event
 * @param cb The callback to unregister
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_null_hook_unregister(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_null_hook_fn_t cb
);

/* ib_conn_t data */

/**
 * Register a callback for a connection event.
 *
 * @param ib Engine handle
 * @param event Event
 * @param cb The callback to register
 * @param cdata Data passed to the callback (or NULL)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hook_conn_register(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_conn_hook_fn_t cb,
    void *cdata
);

/**
 * Unregister a callback for a connection event.
 *
 * @param ib Engine handle
 * @param event Event
 * @param cb The callback to unregister
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_conn_hook_unregister(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_conn_hook_fn_t cb
);

/* ib_conndata_t data */

/**
 * Register a callback for a connection data event.
 *
 * @param ib Engine handle
 * @param event Event
 * @param cb The callback to register
 * @param cdata Data passed to the callback (or NULL)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hook_conndata_register(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_conndata_hook_fn_t cb,
    void *cdata
);

/**
 * Unregister a callback for a connection data event.
 *
 * @param ib Engine handle
 * @param event Event
 * @param cb The callback to unregister
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_conndata_hook_unregister(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_conndata_hook_fn_t cb
);

/* ib_tx_t data */

/**
 * Register a callback for a transaction event.
 *
 * @param ib Engine handle
 * @param event Event
 * @param cb The callback to register
 * @param cdata Data passed to the callback (or NULL)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hook_tx_register(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_tx_hook_fn_t cb,
    void *cdata
);

/**
 * Unregister a callback for a transaction event.
 *
 * @param ib Engine handle
 * @param event Event
 * @param cb The callback to unregister
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_tx_hook_unregister(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_tx_hook_fn_t cb
);

/* ib_txdata_t data */

/**
 * Register a callback for a transaction data event.
 *
 * @param ib Engine handle
 * @param event Event
 * @param cb The callback to register
 * @param cdata Data passed to the callback (or NULL)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hook_txdata_register(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_txdata_hook_fn_t cb,
    void *cdata
);

/**
 * Unregister a callback for a transaction data event.
 *
 * @param ib Engine handle
 * @param event Event
 * @param cb The callback to unregister
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_txdata_hook_unregister(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_txdata_hook_fn_t cb
);

/**
 * Register a callback for a headers data event.
 *
 * @param[in] ib IronBee engine.
 * @param[in] event The specific event.
 * @param[in] cb The callback to unregister.
 * @param[in] cbdata Data to provide to the callback.
 *
 * @returns Status code.
 */
ib_status_t DLL_PUBLIC ib_hook_parsed_header_data_register(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_headers_data_fn_t cb,
    void *cbdata);

/**
 * Unregister a callback for a headers data event.
 *
 * @param[in] ib IronBee engine.
 * @param[in] event The specific event.
 * @param[in] cb The callback to unregister.
 *
 * @returns Status code.
 */
ib_status_t DLL_PUBLIC ib_hook_parsed_header_data_unregister(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_headers_data_fn_t cb);

/**
 * Register a callback for a request line event.
 *
 * @param[in] ib IronBee engine.
 * @param[in] event The specific event.
 * @param[in] cb The callback to unregister.
 * @param[in] cbdata Data to provide to the callback.
 *
 * @returns Status code.
 */
ib_status_t DLL_PUBLIC ib_hook_parsed_req_line_register(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_request_line_fn_t cb,
    void *cbdata);

/**
 * Unregister a callback for a request line event.
 *
 * @param[in] ib IronBee engine.
 * @param[in] event The specific event.
 * @param[in] cb The callback to unregister.
 *
 * @returns Status code.
 */
ib_status_t DLL_PUBLIC ib_hook_parsed_req_line_unregister(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_request_line_fn_t cb);

/**
 * Register a callback for a response line event.
 *
 * @param[in] ib IronBee engine.
 * @param[in] event The specific event.
 * @param[in] cb The callback to unregister.
 * @param[in] cbdata Data to provide to the callback.
 *
 * @returns Status code.
 */
ib_status_t DLL_PUBLIC ib_hook_parsed_resp_line_register(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_response_line_fn_t cb,
    void *cbdata);

/**
 * Unregister a callback for a response line event.
 *
 * @param[in] ib IronBee engine.
 * @param[in] event The specific event.
 * @param[in] cb The callback to unregister.
 *
 * @returns Status code.
 */
ib_status_t DLL_PUBLIC ib_hook_parsed_resp_line_unregister(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_response_line_fn_t cb);

/**
 * @} IronBeeEngineHooks
 */

/**
 * @defgroup IronBeeEngineData Data Field
 * @{
 */

/**
 * Add a data field.
 *
 * @param dpi Data provider instance
 * @param f Field
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_data_add(ib_provider_inst_t *dpi,
                                   ib_field_t *f);

/**
 * Add a data field with a different name than the field name.
 *
 * @param dpi Data provider instance
 * @param f Field
 * @param name Name
 * @param nlen Name length
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_data_add_named(ib_provider_inst_t *dpi,
                                         ib_field_t *f,
                                         const char *name,
                                         size_t nlen);

/**
 * Create and add a numeric data field (extended version).
 *
 * @param dpi Data provider instance
 * @param name Name as byte string
 * @param nlen Name length
 * @param val Numeric value
 * @param pf Pointer where new field is written if non-NULL
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_data_add_num_ex(ib_provider_inst_t *dpi,
                                          const char *name,
                                          size_t nlen,
                                          ib_num_t val,
                                          ib_field_t **pf);

/**
 * Create and add a nulstr data field (extended version).
 *
 * @param dpi Data provider instance
 * @param name Name as byte string
 * @param nlen Name length
 * @param val String value
 * @param pf Pointer where new field is written if non-NULL
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_data_add_nulstr_ex(ib_provider_inst_t *dpi,
                                             const char *name,
                                             size_t nlen,
                                             char *val,
                                             ib_field_t **pf);

/**
 * Create and add a bytestr data field (extended version).
 *
 * @param dpi Data provider instance
 * @param name Name
 * @param nlen Name length
 * @param val Byte string value
 * @param vlen Value length
 * @param pf Pointer where new field is written if non-NULL
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_data_add_bytestr_ex(ib_provider_inst_t *dpi,
                                              const char *name,
                                              size_t nlen,
                                              uint8_t *val,
                                              size_t vlen,
                                              ib_field_t **pf);

/**
 * Create and add a list data field (extended version).
 *
 * @param dpi Data provider instance
 * @param name Name as byte string
 * @param nlen Name length
 * @param pf Pointer where new field is written if non-NULL
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_data_add_list_ex(ib_provider_inst_t *dpi,
                                           const char *name,
                                           size_t nlen,
                                           ib_field_t **pf);

/**
 * Create and add a stream buffer data field (extended version).
 *
 * @param dpi Data provider instance
 * @param name Name as byte string
 * @param nlen Name length
 * @param pf Pointer where new field is written if non-NULL
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_data_add_stream_ex(ib_provider_inst_t *dpi,
                                             const char *name,
                                             size_t nlen,
                                             ib_field_t **pf);

/**
 * Get a data field (extended version).
 *
 * @param dpi Data provider instance
 * @param name Name as byte string
 * @param nlen Name length
 * @param pf Pointer where new field is written. Unlike other calls,
 *           this must not be NULL.
 *
 * @returns IB_OK on success or IB_ENOENT if the element is not found.
 */
ib_status_t DLL_PUBLIC ib_data_get_ex(ib_provider_inst_t *dpi,
                                      const char *name,
                                      size_t nlen,
                                      ib_field_t **pf);

/**
 * Get all data fields from a data provider instance.
 *
 * @param dpi Data provider instance
 * @param list List which data fields will be pushed
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_data_get_all(ib_provider_inst_t *dpi,
                                       ib_list_t *list);

/**
 * Get a data field with a transformation (extended version).
 *
 * @param dpi Data provider instance
 * @param name Name as byte string
 * @param nlen Name length
 * @param pf Pointer where new field is written if non-NULL
 * @param tfn Transformations (comma separated names)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_data_tfn_get_ex(ib_provider_inst_t *dpi,
                                          const char *name,
                                          size_t nlen,
                                          ib_field_t **pf,
                                          const char *tfn);

/**
 * Create and add a numeric data field.
 *
 * @param dpi Data provider instance
 * @param name Name as byte string
 * @param val Numeric value
 * @param pf Pointer where new field is written if non-NULL
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_data_add_num(ib_provider_inst_t *dpi,
                                       const char *name,
                                       ib_num_t val,
                                       ib_field_t **pf);

#define ib_data_add_num(dpi,name,val,pf) \
    ib_data_add_num_ex(dpi,name,strlen(name),val,pf)

/**
 * Create and add a nulstr data field.
 *
 * @param dpi Data provider instance
 * @param name Name as byte string
 * @param val String value
 * @param pf Pointer where new field is written if non-NULL
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_data_add_nulstr(ib_provider_inst_t *dpi,
                                          const char *name,
                                          char *val,
                                          ib_field_t **pf);

#define ib_data_add_nulstr(dpi,name,val,pf) \
    ib_data_add_nulstr_ex(dpi,name,strlen(name),val,pf)

/**
 * Create and add a bytestr data field.
 *
 * @param dpi Data provider instance

 * @param name Name
 * @param val Byte string value
 * @param vlen Value length
 * @param pf Pointer where new field is written if non-NULL
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_data_add_bytestr(ib_provider_inst_t *dpi,
                                           const char *name,
                                           uint8_t *val,
                                           size_t vlen,
                                           ib_field_t **pf);

#define ib_data_add_bytestr(dpi,name,val,vlen,pf) \
    ib_data_add_bytestr_ex(dpi,name,strlen(name),val,vlen,pf)

/**
 * Create and add a list data field.
 *
 * @param dpi Data provider instance
 * @param name Name as byte string
 * @param pf Pointer where new field is written if non-NULL
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_data_add_list(ib_provider_inst_t *dpi,
                                        const char *name,
                                        ib_field_t **pf);

#define ib_data_add_list(dpi,name,pf) \
    ib_data_add_list_ex(dpi,name,strlen(name),pf)

/**
 * Create and add a stream buffer data field.
 *
 * @param dpi Data provider instance
 * @param name Name as byte string
 * @param pf Pointer where new field is written if non-NULL
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_data_add_stream(ib_provider_inst_t *dpi,
                                          const char *name,
                                          ib_field_t **pf);

#define ib_data_add_stream(dpi,name,pf) \
    ib_data_add_stream_ex(dpi,name,strlen(name),pf)

/**
 * Get a data field.
 *
 * @param dpi Data provider instance
 * @param name Name as NUL terminated string
 * @param pf Pointer where new field is written if non-NULL
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_data_get(ib_provider_inst_t *dpi,
                                   const char *name,
                                   ib_field_t **pf);

#define ib_data_get(dpi,name,pf) \
    ib_data_get_ex(dpi,name,strlen(name),pf)

/**
 * Get a data field with a transformation.
 *
 * @param dpi Data provider instance
 * @param name Name as NUL terminated string
 * @param pf Pointer where new field is written if non-NULL
 * @param tfn Transformations (comma separated names)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_data_tfn_get(ib_provider_inst_t *dpi,
                                       const char *name,
                                       ib_field_t **pf,
                                       const char *tfn);
#define ib_data_tfn_get(dpi,name,pf,tfn) \
    ib_data_tfn_get_ex(dpi,name,strlen(name),pf,tfn)

/**
 * Remove a data field.
 * @param dpi Data provider instance
 * @param name Name as NUL terminated string
 * @param nlen Length of @a name.
 * @param pf Pointer where old field is written if non-NULL
 */
ib_status_t ib_data_remove_ex(ib_provider_inst_t *dpi,
                               const char *name,
                               size_t nlen,
                               ib_field_t **pf);

/**
 * Expand a string using fields from the data store.
 *
 * This function looks through @a str for instances of
 * "%{"+_name_+"}" (e.g. "%{FOO}"), then attempts to look up
 * each of such name found in the data provider @a dpi.  If _name_ is not
 * found in the @a dpi, the "%{_name_}" sub-string is replaced with an empty
 * string.  If the name is found, the associated field value is used to
 * replace "%{_name_}" sub-string for string and numeric types (numbers are
 * converted to strings); for others, the replacement is an empty string.
 *
 * @param[in] dpi Data provider instance
 * @param[in] str NUL-terminated string to expand
 * @param[out] result Pointer to the expanded string.
 *
 * @returns Status code
 */
ib_status_t ib_data_expand_str(ib_provider_inst_t *dpi,
                               const char *str,
                               char **result);

/**
 * Expand a string using fields from the data store, ex version.
 *
 * @sa ib_data_expand_str()
 *
 * @param[in] dpi Data provider instance
 * @param[in] str String to expand
 * @param[in] slen Length of string @a str to expand
 * @param[in] nul Append NUL byte to @a result?
 * @param[out] result Pointer to the expanded string.
 * @param[out] result_len Length of @a result.
 *
 * @returns Status code
 */
ib_status_t ib_data_expand_str_ex(ib_provider_inst_t *dpi,
                                  const char *str,
                                  size_t slen,
                                  ib_bool_t nul,
                                  char **result,
                                  size_t *result_len);

/**
 * Determine if a string would be expanded by ib_data_expand_str().
 *
 * This function looks through @a str for instances of "%{.+}".
 *
 * @param[in] str String to check for expansion
 * @param[out] result IB_TRUE if @a str would be expanded by expand_str().
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_data_expand_test_str(const char *str,
                                               ib_bool_t *result);

/**
 * Determine if a string would be expanded by ib_data_expand_str_ex().
 *
 * @sa ib_data_expand_str(),  ib_data_expand_str_ex(), ib_data_test_str().
 *
 * @param[in] str String to check for expansion
 * @param[in] slen Length of string @a str to expand
 * @param[out] result IB_TRUE if @a str would be expanded by expand_str().
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_data_expand_test_str_ex(const char *str,
                                                  size_t slen,
                                                  ib_bool_t *result);

/**
 * Remove a data field.
 * @param dpi Data provider instance
 * @param name Name as NUL terminated string
 * @param pf Pointer where old field is written if non-NULL
 */
ib_status_t ib_data_remove(ib_provider_inst_t *dpi,
                           const char *name,
                           ib_field_t **pf);

#define ib_data_remove(dpi,name,pf) \
    ib_data_remove_ex(dpi,name,strlen(name),pf)

/**
 * @} IronBeeEngineData
 */

/**
 * @defgroup IronBeeEngineMatcher Matcher
 * @{
 */

typedef struct ib_matcher_t ib_matcher_t;
typedef void ib_matcher_result_t; /// @todo Not implemented yet

ib_status_t DLL_PUBLIC ib_matcher_create(ib_engine_t *ib,
                                         ib_mpool_t *pool,
                                         const char *key,
                                         ib_matcher_t **pm);

ib_status_t DLL_PUBLIC ib_matcher_instance_create(ib_engine_t *ib,
                                                  ib_mpool_t *pool,
                                                  const char *key,
                                                  ib_matcher_t **pm);

void DLL_PUBLIC *ib_matcher_compile(ib_matcher_t *m,
                                    const char *patt,
                                    const char **errptr,
                                    int *erroffset);

ib_status_t DLL_PUBLIC ib_matcher_match_buf(ib_matcher_t *m,
                                            void *cpatt,
                                            ib_flags_t flags,
                                            const uint8_t *data,
                                            size_t dlen,
                                            void *ctx);

ib_status_t DLL_PUBLIC ib_matcher_match_field(ib_matcher_t *m,
                                              void *cpatt,
                                              ib_flags_t flags,
                                              ib_field_t *f,
                                              void *ctx);

ib_status_t DLL_PUBLIC ib_matcher_add_pattern(ib_matcher_t *m,
                                              const char *patt);

ib_status_t DLL_PUBLIC ib_matcher_add_pattern_ex(ib_matcher_t *m,
                                                 const char *patt,
                                                 ib_void_fn_t callback,
                                                 void *arg,
                                                 const char **errptr,
                                                 int *erroffset);

ib_status_t DLL_PUBLIC ib_matcher_exec_buf(ib_matcher_t *m,
                                           ib_flags_t flags,
                                           const uint8_t *data,
                                           size_t dlen,
                                           void *ctx);

ib_status_t DLL_PUBLIC ib_matcher_exec_field(ib_matcher_t *m,
                                             ib_flags_t flags,
                                             ib_field_t *f,
                                             void *ctx);

/**
 * @} IronBeeEngineMatcher
 */


/**
 * @defgroup IronBeeEngineLog Logging
 * @{
 */

/**
 * Logger log level.
 **/
typedef enum {
    IB_LOG_EMERGENCY = 0, /**< System unusable. */
    IB_LOG_ALERT     = 1, /**< Crisis happened; immediate attention */
    IB_LOG_CRITICAL  = 2, /**< Crisis coming; immediate attention */
    IB_LOG_ERROR     = 3, /**< Error occurred; needs attention */
    IB_LOG_WARNING   = 4, /**< Error likely to occur; needs attention */
    IB_LOG_NOTICE    = 5, /**< Something unusual happened */
    IB_LOG_INFO      = 6, /**< Something usual happened */
    IB_LOG_DEBUG     = 7, /**< Developer oriented information */
    IB_LOG_DEBUG2    = 8, /**< As above, lower priority */
    IB_LOG_DEBUG3    = 9, /**< As above, lowest priority */
    IB_LOG_TRACE     = 10 /**< Reserved for future use */
} ib_log_level_t;

/**
 * String to level conversion.
 */
ib_log_level_t DLL_PUBLIC ib_log_string_to_level(const char* s);

/**
 * Level to string conversion
 */
const char DLL_PUBLIC *ib_log_level_to_string(ib_log_level_t level);

/**
 * Logger callback.
 *
 * @param cbdata Callback data
 * @param level Log level
 * @param prefix Optional prefix to the log
 * @param file Optional source filename (or NULL)
 * @param line Optional source line number (or 0)
 * @param fmt Formatting string
 */
typedef void (*ib_log_logger_fn_t)(void *cbdata,
                                   ib_log_level_t level,
                                   const char *prefix,
                                   const char *file, int line,
                                   const char *fmt, va_list ap)
                                   VPRINTF_ATTRIBUTE(6);

/** Log Generic */
#define ib_log(ib,lvl,...) ib_log_ex((ib), (lvl), NULL, NULL, 0, __VA_ARGS__)
/** Log Emergency */
#define ib_log_emergency(ib,...) ib_log_ex((ib), IB_LOG_EMERGENCY, "EMERGENCY - ", NULL, 0, __VA_ARGS__)
/** Log Alert */
#define ib_log_alert(ib,...)     ib_log_ex((ib), IB_LOG_ALERT,     "ALERT     - ", NULL, 0, __VA_ARGS__)
/** Log Critical */
#define ib_log_critical(ib,...)  ib_log_ex((ib), IB_LOG_CRITICAL,  "CRITICAL  - ", NULL, 0, __VA_ARGS__)
/** Log Error */
#define ib_log_error(ib,...)     ib_log_ex((ib), IB_LOG_ERROR,     "ERROR     - ", NULL, 0, __VA_ARGS__)
/** Log Warning */
#define ib_log_warning(ib,...)   ib_log_ex((ib), IB_LOG_WARNING,   "WARNING   - ", NULL, 0, __VA_ARGS__)
/** Log Notice */
#define ib_log_notice(ib,...)    ib_log_ex((ib), IB_LOG_NOTICE,    "NOTICE    - ", NULL, 0, __VA_ARGS__)
/** Log Info */
#define ib_log_info(ib,...)      ib_log_ex((ib), IB_LOG_INFO,      "INFO      - ", NULL, 0, __VA_ARGS__)
/** Log Debug */
#define ib_log_debug(ib,...)     ib_log_ex((ib), IB_LOG_DEBUG,     "DEBUG     - ", __FILE__, __LINE__, __VA_ARGS__)
/** Log Debug2 */
#define ib_log_debug2(ib,...)    ib_log_ex((ib), IB_LOG_DEBUG2,    "DEBUG2    - ", __FILE__, __LINE__, __VA_ARGS__)
/** Log Debug3 */
#define ib_log_debug3(ib,...)    ib_log_ex((ib), IB_LOG_DEBUG3,    "DEBUG3    - ", __FILE__, __LINE__, __VA_ARGS__)
/** Log Trace */
#define ib_log_trace(ib,...)     ib_log_ex((ib), IB_LOG_TRACE,     "TRACE     - ", __FILE__, __LINE__, __VA_ARGS__)

/**
 * Generic Logger for engine.
 *
 * @warning There is currently a 1024 byte formatter limit when prefixing the
 *          log header data.
 *
 * @param ib IronBee engine
 * @param level Log level (0-9)
 * @param prefix String to prefix log header data (or NULL)
 * @param file Filename (or NULL)
 * @param line Line number (or 0)
 * @param fmt Printf-like format string
 */
void DLL_PUBLIC ib_log_ex(ib_engine_t *ib, int level,
                          const char *prefix, const char *file, int line,
                          const char *fmt, ...)
                          PRINTF_ATTRIBUTE(6, 0);

/**
 * Generic Logger for engine.  valist version.
 *
 * @warning There is currently a 1024 byte formatter limit when prefixing the
 *          log header data.
 *
 * @param ib IronBee engine
 * @param level Log level (0-9)
 * @param prefix String to prefix log header data (or NULL)
 * @param file Filename (or NULL)
 * @param line Line number (or 0)
 * @param fmt Printf-like format string
 * @param ap Argument list.
 */
void DLL_PUBLIC ib_vlog_ex(ib_engine_t *ib, int level,
                          const char *prefix, const char *file, int line,
                          const char *fmt,  va_list ap);


/**
 * Return the configured logging level.
 *
 * This is used to determine if optional complex processing should be
 * performed to log possibly option information.
 *
 * @param[in] ib The IronBee engine that would be used in a call to ib_log_ex.
 * @return The log level configured.
 */
int DLL_PUBLIC ib_log_get_level(ib_engine_t *ib);

/**
 * Set the IronBee log level.
 *
 * @param[in] ib The IronBee engine that would be used in a call to ib_log_ex.
 * @param[in] level The new log level.
 */
void DLL_PUBLIC ib_log_set_level(ib_engine_t *ib, int level);


// TODO: The ib_event_* functions should be ib_logevent_* below???

/**
 * Add an event to be logged.
 *
 * @param pi Provider instance
 * @param e Event
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_event_add(ib_provider_inst_t *pi,
                                    ib_logevent_t *e);

/**
 * Remove an event from the queue before it is logged.
 *
 * @param pi Provider instance
 * @param id Event id
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_event_remove(ib_provider_inst_t *pi,
                                       uint32_t id);

/**
 * Get a list of pending events to be logged.
 *
 * @note The list can be modified directly.
 *
 * @param pi Provider instance
 * @param pevents Address where list of events is written
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_event_get_all(ib_provider_inst_t *pi,
                                        ib_list_t **pevents);

/**
 * Write out any pending events to the log.
 *
 * @param pi Provider instance
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_event_write_all(ib_provider_inst_t *pi);

/**
 * Write out audit log.
 *
 * @param pi Provider instance
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_auditlog_write(ib_provider_inst_t *pi);

/**
 * @} IronBeeEngineLog
 */

/**
 * @defgroup IronBeeEngineLogEvent Log Events
 * @{
 */

/** Log Event Type */
typedef enum {
    IB_LEVENT_TYPE_UNKNOWN,
    IB_LEVENT_TYPE_OBSERVATION,
} ib_logevent_type_t;

/** Log Event Action */
typedef enum {
    IB_LEVENT_ACTION_UNKNOWN,
    IB_LEVENT_ACTION_LOG,
    IB_LEVENT_ACTION_BLOCK,
    IB_LEVENT_ACTION_IGNORE,
    IB_LEVENT_ACTION_ALLOW,
} ib_logevent_action_t;

/**
 * Lookup log event type name.
 *
 * @param num Numeric ID
 *
 * @returns String name
 */
const DLL_PUBLIC char *ib_logevent_type_name(ib_logevent_type_t num);

/**
 * Lookup log event action name.
 *
 * @param num Numeric ID
 *
 * @returns String name
 */
const DLL_PUBLIC char *ib_logevent_action_name(ib_logevent_action_t num);

/** Log Event Structure */
struct ib_logevent_t {
    ib_mpool_t              *mp;         /**< Memory pool */
    const char              *rule_id;    /**< Formatted rule ID */
    const char              *msg;        /**< Event message */
    ib_list_t               *tags;       /**< List of tag strings */
    ib_list_t               *fields;     /**< List of field name strings */
    uint32_t                 event_id;   /**< Event ID */
    ib_logevent_type_t       type;       /**< Event type */
    ib_logevent_action_t     rec_action; /**< Recommended action */
    ib_logevent_action_t     action;     /**< Action taken */
    const void              *data;       /**< Event data */
    size_t                   data_len;   /**< Event data size */
    uint8_t                  confidence; /**< Event confidence (percent) */
    uint8_t                  severity;   /**< Event severity (0-100?) */
};

/**
 * Create a logevent.
 *
 * @param[out] ple Address which new logevent is written
 * @param[in]  pool Memory pool to allocate from
 * @param[in]  rule_id Rule ID string
 * @param[in]  type Event type
 * @param[in]  rec_action Event recommended action
 * @param[in]  action Event action taken
 * @param[in]  confidence Event confidence
 * @param[in]  severity Event severity
 * @param[in]  fmt Event message format string
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_logevent_create(ib_logevent_t **ple,
                                          ib_mpool_t *pool,
                                          const char *rule_id,
                                          ib_logevent_type_t type,
                                          ib_logevent_action_t rec_action,
                                          ib_logevent_action_t action,
                                          uint8_t confidence,
                                          uint8_t severity,
                                          const char *fmt,
                                          ...);

/**
 * Add a tag to the event.
 *
 * @param[in] le Log event
 * @param[in] tag Tag to add (string will be copied)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_logevent_tag_add(ib_logevent_t *le,
                                           const char *tag);

/**
 * Add a field name to the event.
 *
 * @param[in] le Log event
 * @param[in] name Field name to add (string will be copied)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_logevent_field_add(ib_logevent_t *le,
                                             const char *name);

/**
 * Set data for the event.
 *
 * @param[in] le Log event
 * @param[in] data Arbitrary binary data
 * @param[in] dlen Data length
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_logevent_data_set(ib_logevent_t *le,
                                            const void *data,
                                            size_t dlen);

/**
 * @} IronBeeEngineLogEvent
 */


/**
 * @defgroup IronBeeFilter Filter
 * @ingroup IronBee
 * @{
 */

#define IB_FILTER_FNONE          0        /**< No filter flags were set */
#define IB_FILTER_FMDATA        (1<<0)    /**< Filter modified the data */
#define IB_FILTER_FMDLEN        (1<<1)    /**< Filter modified data length */
#define IB_FILTER_FINPLACE      (1<<2)    /**< Filter action was in-place */

#define IB_FILTER_ONONE          0        /**< No filter options set */
#define IB_FILTER_OMDATA        (1<<0)    /**< Filter may modify data */
#define IB_FILTER_OMDLEN        (1<<1)    /**< Filter may modify data length */
#define IB_FILTER_OBUF          (1<<2)    /**< Filter may buffer data */

/**
 * Filter Function.
 *
 * This function is called with data that can be analyzed and then optionally
 * modified.  Various flags can be set via pflags.
 *
 * @param f Filter
 * @param fdata Filter data
 * @param ctx Config context
 * @param pool Pool to use, should allocation be required
 * @param pflags Address to write filter processing flags
 *
 * @returns Status code
 */
typedef ib_status_t (*ib_filter_fn_t)(ib_filter_t *f,
                                      ib_fdata_t *fdata,
                                      ib_context_t *ctx,
                                      ib_mpool_t *pool,
                                      ib_flags_t *pflags);

/** IronBee Filter */
struct ib_filter_t {
    ib_engine_t             *ib;        /**< Engine */
    const char              *name;      /**< Filter name */
    ib_filter_type_t         type;      /**< Filter type */
    ib_flags_t               options;   /**< Filter options */
    size_t                   idx;       /**< Filter index */
    ib_filter_fn_t           fn_filter; /**< Filter function */
    void                    *cbdata;    /**< Filter callback data */
};

/** IronBee Filter Data */
struct ib_fdata_t {
    union {
        void                *ptr;       /**< Generic pointer for set op */
        ib_conn_t           *conn;      /**< Connection (conn filters) */
        ib_tx_t             *tx;        /**< Transaction (tx filters) */
    } udata;
    ib_stream_t             *stream;    /**< Data stream */
    void                    *state;     /**< Arbitrary state data */
};

/**
 * IronBee Filter Controller.
 *
 * Data comes into the filter controller via the @ref source, gets
 * pushed through the list of data @ref filters, into the buffer filter
 * @ref fbuffer where the data may be held while it is being processed
 * and finally makes it to the @ref sink where it is ready to be sent.
 */
struct ib_fctl_t {
    ib_fdata_t               fdata;     /**< Filter data */
    ib_engine_t             *ib;        /**< Engine */
    ib_mpool_t              *mp;        /**< Filter memory pool */
    ib_list_t               *filters;   /**< Filter list */
    ib_filter_t             *fbuffer;   /**< Buffering filter (flow control) */
    ib_stream_t             *source;    /**< Data source (new data) */
    ib_stream_t             *sink;      /**< Data sink (processed data) */
};


/* -- Filter API -- */

/**
 * Register a filter.
 *
 * @param pf Address which filter handle is written
 * @param ib Engine handle
 * @param name Filter name
 * @param type Filter type
 * @param options Filter options
 * @param fn_filter Filter callback function
 * @param cbdata Filter callback data
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_filter_register(ib_filter_t **pf,
                                          ib_engine_t *ib,
                                          const char *name,
                                          ib_filter_type_t type,
                                          ib_flags_t options,
                                          ib_filter_fn_t fn_filter,
                                          void *cbdata);

/**
 * Add a filter to a context.
 *
 * @param f Filter
 * @param ctx Config context
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_filter_add(ib_filter_t *f,
                                     ib_context_t *ctx);

#if 0
/**
 * Create a filter controller for a connection in the given context.
 *
 * @param pfc Address which filter controller handle is written
 * @param conn Connection
 * @param ctx Config context
 * @param pool Memory pool
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_fctl_conn_create(ib_fctl_t **pfc,
                                           ib_conn_t *conn,
                                           ib_context_t *ctx,
                                           ib_mpool_t *pool);
#endif

/**
 * Create a filter controller for a transaction.
 *
 * @param pfc Address which filter controller handle is written
 * @param tx Transaction
 * @param pool Memory pool
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_fctl_tx_create(ib_fctl_t **pfc,
                                         ib_tx_t *tx,
                                         ib_mpool_t *pool);

/**
 * Configure a filter controller for a given context.
 *
 * @param fc Filter controller
 * @param ctx Config context
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_fctl_config(ib_fctl_t *fc,
                                      ib_context_t *ctx);

/**
 * Process any pending data through the filters.
 *
 * @param fc Filter controller
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_fctl_process(ib_fctl_t *fc);

/**
 * Add data to the filter controller.
 *
 * This will pass through all the filters and then be fetched
 * with calls to @ref ib_fctl_drain.
 *
 * @param fc Filter controller
 * @param dtype Data type
 * @param data Data
 * @param dlen Data length
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_fctl_data_add(ib_fctl_t *fc,
                                        ib_data_type_t dtype,
                                        void *data,
                                        size_t dlen);

/**
 * Add meta data to the filter controller.
 *
 * This will pass through all the filters and then be fetched
 * with calls to @ref ib_fctl_drain.
 *
 * @param fc Filter controller
 * @param stype Stream data type
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_fctl_meta_add(ib_fctl_t *fc,
                                        ib_sdata_type_t stype);

/**
 * Drain processed data from the filter controller.
 *
 * @param fc Filter controller
 * @param pstream Address which output stream is written
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_fctl_drain(ib_fctl_t *fc,
                                     ib_stream_t **pstream);


/**
 * @} IronBeeFilter
 */

/**
 * @} IronBeeEngine
 * @} IronBee
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_ENGINE_H_ */
