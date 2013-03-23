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

#ifndef _IB_ENGINE_H_
#define _IB_ENGINE_H_

/**
 * @file
 * @brief IronBee --- Engine
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/cfgmap.h>
#include <ironbee/clock.h>
#include <ironbee/engine_types.h>
#include <ironbee/field.h>
#include <ironbee/hash.h>
#include <ironbee/parsed_content.h>
#include <ironbee/server.h>
#include <ironbee/stream.h>
#include <ironbee/uuid.h>

#include <stdarg.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeEngine IronBee Engine
 * @ingroup IronBee
 *
 * This is the API for the IronBee engine.
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
 * Initialize IronBee (before engine creation)
 *
 * @returns Status code
 *    - IB_OK
 *    - Errors returned by ib_util_initialize()
 */
ib_status_t DLL_PUBLIC ib_initialize(void);

/**
 * Shutdown IronBee (After engine destruction)
 *
 * @returns Status code
 *    - IB_OK
 *    - Errors returned by ib_util_shutdown()
 */
ib_status_t DLL_PUBLIC ib_shutdown(void);

/**
 * Create an engine handle.
 *
 * After creating the engine, the caller must configure defaults, such as
 * initial logging parameters, and then call @ref ib_engine_init() to
 * initialize the engine configuration context.
 *
 * @param pib Address which new handle is written
 * @param server Information on the server instantiating the engine
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_engine_create(ib_engine_t **pib,
                                        ib_server_t *server);

/**
 * Initialize the engine configuration context.
 *
 * @param ib Engine handle
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_engine_init(ib_engine_t *ib);

/**
 * Inform the engine that the configuration phase is starting
 *
 * @param[in] ib Engine handle
 * @param[in] cp The configuration parser
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_engine_config_started(ib_engine_t *ib,
                                                ib_cfgparser_t *cp);

/**
 * Inform the engine that the configuration phase is complete
 *
 * @param ib Engine handle
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_engine_config_finished(ib_engine_t *ib);

/**
 * Get the configuration parser
 *
 * @param[in] ib Engine handle
 * @param[out] pparser Pointer to the configuration parser.
 *
 * @returns IB_OK
 */
ib_status_t ib_engine_cfgparser_get(const ib_engine_t *ib,
                                    const ib_cfgparser_t **pparser);

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
ib_mpool_t DLL_PUBLIC *ib_engine_pool_main_get(const ib_engine_t *ib);

/**
 * Get the engine configuration memory pool.
 *
 * This pool should be used to configure the engine.
 *
 * @param ib Engine handle
 *
 * @returns Memory pool
 */
ib_mpool_t DLL_PUBLIC *ib_engine_pool_config_get(const ib_engine_t *ib);

/**
 * Get the engine temporary memory pool.
 *
 * This pool should be destroyed by the server after the
 * configuration phase. Therefore is should not be used for
 * anything except temporary allocations which are required
 * for performing configuration.
 *
 * @param ib Engine handle
 *
 * @returns Memory pool
 */
ib_mpool_t DLL_PUBLIC *ib_engine_pool_temp_get(const ib_engine_t *ib);

/**
 * Destroy the engine temporary memory pool.
 *
 * This should be called by the server after configuration is
 * completed. After this call, any allocations in the temporary
 * pool will be invalid and no future allocations can be made to
 * to this pool.
 *
 * @param ib Engine handle
 */
void DLL_PUBLIC ib_engine_pool_temp_destroy(ib_engine_t *ib);

/**
 * Destroy a memory pool.
 *
 * This destroys the memory pool @a mp.  If IB_DEBUG_MEMORY is defined,
 * it will validate and analyze the pool before destruction.
 *
 * Will do nothing if @a mp is NULL.
 *
 * @param[in] ib IronBee engine.
 * @param[in] mp Memory pool to destroy.
 */
void DLL_PUBLIC ib_engine_pool_destroy(ib_engine_t *ib, ib_mpool_t *mp);

/**
 * Destroy an engine.
 *
 * @param ib Engine handle
 *
 * @returns Status code
 */
void DLL_PUBLIC ib_engine_destroy(ib_engine_t *ib);

/**
 * Get all configuration contexts
 *
 * @param ib Engine handle
 *
 * @returns List of configuration contexts
 */
const ib_list_t *ib_context_get_all(const ib_engine_t *ib);

/**
 * Create a configuration context.
 *
 * @param ib Engine handle
 * @param parent Parent context (or NULL)
 * @param ctx_type Context type (IB_CTYPE_xx)
 * @param ctx_type_string String to identify context type (i.e. "Site", "Main")
 * @param ctx_name String to identify context ("foo.com", "main")
 * @param pctx Address which new context is written
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_context_create(ib_engine_t *ib,
                                         ib_context_t *parent,
                                         ib_ctype_t ctx_type,
                                         const char *ctx_type_string,
                                         const char *ctx_name,
                                         ib_context_t **pctx);

/**
 * Set the site associated with the current context
 *
 * @param[in,out] ctx Context to set the site for
 * @param[in] site Site object to store in context / NULL
 *
 * @returns Status code
 */
ib_status_t ib_context_site_set(
    ib_context_t *ctx,
    const ib_site_t *site);

/**
 * Get the site associated with the current context
 *
 * @param[in] ctx Context to query
 * @param[out] psite Pointer to the site object / NULL
 *
 * @returns Status code
 */
ib_status_t ib_context_site_get(
    const ib_context_t *ctx,
    const ib_site_t **psite);

/**
 * Set the location associated with the current context
 *
 * @param[in,out] ctx Context to set the location for
 * @param[in] location Location object to store in context / NULL
 *
 * @returns Status code
 */
ib_status_t ib_context_location_set(
    ib_context_t *ctx,
    const ib_site_location_t *location);

/**
 * Get the location associated with the current context
 *
 * @param[in] ctx Context to query
 * @param[out] plocation Pointer to the location object / NULL
 *
 * @returns Status code
 */
ib_status_t ib_context_location_get(
    const ib_context_t *ctx,
    const ib_site_location_t **plocation);

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
 * Get the type of a context
 *
 * @param ctx Configuration context
 *
 * @returns Context's enumerated type
 */
ib_ctype_t DLL_PUBLIC ib_context_type(const ib_context_t *ctx);

/**
 * Check the type of a context
 *
 * @param ctx Configuration context
 * @param ctype Configuration type
 *
 * @returns true if the context's type matches the @a ctype, else false
 */
bool ib_context_type_check(const ib_context_t *ctx, ib_ctype_t ctype);

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
 * Set the CWD for a context
 *
 * @param[in,out] ctx The context to operate on
 * @param[in] dir The directory to store (can be NULL)
 *
 * @returns IB_OK, or IB_EALLOC for allocation errors
 */
ib_status_t DLL_PUBLIC ib_context_set_cwd(ib_context_t *ctx,
                                          const char *dir);

/**
 * Get the CWD for a context
 *
 * @param[in,out] ctx The context to operate on
 *
 * @returns Pointer to the context's CWD, or NULL if none available
 */
const char DLL_PUBLIC *ib_context_config_cwd(const ib_context_t *ctx);

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
 * @returns Pointer to the main context (or engine context if the main
 *          context hasn't been created yet).  This should always be
 *          a valid pointer, never NULL.
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
 * @param[in] enable Enable the index?
 * @param[in] idx The index value to be copied into the context.
 *
 * @returns IB_OK, IB_EALLOC or the status of ib_lock_init.
 */
ib_status_t ib_context_set_auditlog_index(ib_context_t *ctx,
                                          bool enable,
                                          const char* idx);

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
 * Create a connection structure.
 *
 * @param ib Engine handle
 * @param pconn Address which new connection is written
 * @param server_ctx Server connection context
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_conn_create(ib_engine_t *ib,
                                      ib_conn_t **pconn,
                                      void *server_ctx);

/**
 * Get per-module per-connection data.
 *
 * @param[in]  conn Connection
 * @param[in]  module Module.
 * @param[out] data Data.
 * @returns Status code.
 */
ib_status_t DLL_PUBLIC ib_conn_get_module_data(
    const ib_conn_t    *conn,
    const ib_module_t  *module,
    void              **data
);

/**
 * Set per-module per-connection data.
 *
 * @param[in] conn Connection
 * @param[in] module Module.
 * @param[in] data Data.
 * @returns Status code.
 */
ib_status_t DLL_PUBLIC ib_conn_set_module_data(
    ib_conn_t         *conn,
    const ib_module_t *module,
    void              *data
);

/**
 * Set the connection parser context.
 *
 * @param conn Connection structure
 * @param parser_ctx Parser context
 */
void ib_conn_parser_context_set(ib_conn_t *conn,
                                void *parser_ctx);

/**
 * Get the connection parser context.
 *
 * @param conn Connection structure
 *
 * @returns The connection parser context
 */
void DLL_PUBLIC *ib_conn_parser_context_get(ib_conn_t *conn);

/**
 * Set connection flags.
 *
 * @param conn Connection structure
 * @param flag Flags
 */
#define ib_conn_flags_set(conn, flag) do { (conn)->flags |= (flag); } while(0)

/**
 * Unset connection flags.
 *
 * @param conn Connection structure
 * @param flag Flags
 */
#define ib_conn_flags_unset(conn, flag) do { (conn)->flags &= ~(flag); } while(0)

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
 * Destroy a connection structure.
 *
 * @param conn Connection structure
 */
void DLL_PUBLIC ib_conn_destroy(ib_conn_t *conn);

/**
 * Merge the base_uuid with tx data and generate the tx id string.
 *
 * This function is normally executed by ib_tx_create(), but if the tx is
 * being created in other ways (e.g. in the tests), use this to generate the
 * TX's ID.
 *
 * @param[in,out] tx Transaction to populate
 * @param[in] mp Memory pool to use.
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_tx_generate_id(ib_tx_t *tx,
                                         ib_mpool_t *mp);

/**
 * Create a transaction structure.
 *
 * @param ptx Address which new transaction is written
 * @param conn Connection structure
 * @param sctx Server transaction context
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_tx_create(ib_tx_t **ptx,
                                    ib_conn_t *conn,
                                    void *sctx);

/**
 * Get per-module per-transaction data.
 *
 * @param[in]  tx Transaction.
 * @param[in]  module Module.
 * @param[out] pdata Address which data is written
 * @returns Status code.
 */
ib_status_t DLL_PUBLIC ib_tx_get_module_data(
    const ib_tx_t *tx,
    const ib_module_t *module,
    void  *pdata
);

/**
 * Set per-module per-transaction data.
 *
 * @param[in] tx Transaction.
 * @param[in] module Module.
 * @param[in] data Data.
 * @returns Status code.
 */
ib_status_t DLL_PUBLIC ib_tx_set_module_data(
    ib_tx_t *tx,
    const ib_module_t *module,
    void *data
);

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
 * Unset transaction flags.
 *
 * @param tx Transaction structure
 * @param flag Flags
 *
 * @returns All current flags
 */
#define ib_tx_flags_unset(tx, flag) do { (tx)->flags &= ~(flag); } while(0)

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
 * @defgroup IronBeeEngineState State
 *
 * This API allows for sending state and data information to the engine.
 *
 * The following diagrams the general state processing order of a typical
 * transaction that is allowed to be fully processed.
 *
 * @dot
 * digraph legend1 {
 *   server_state [label="Server\nstate",shape=octagon]
 *   server_async_state [label="Server\nasyncronous\nstate",shape=octagon,peripheries=2]
 *   parser_state [label="Parser\nstate",shape=ellipse]
 *   engine_state [label="Engine\nstate",shape=diamond]
 *   handler_state [label="Handler\nstate",shape=parallelogram]
 *   entity [label="Entity",shape=box]
 *   note [label="Note",shape=note]
 *   {rank=same; server_state server_async_state parser_state engine_state handler_state note}
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
 * Server states are triggered by the server and parser states by the
 * parser. These states cause the engine to trigger both the engine and
 * handler states. The engine states are meant to be synchronization
 * points. The handler states are meant to be handled by modules to do
 * detection and take actions, while the server and parser states are
 * to be used to generate fields and anything else needed in the handler
 * states.
 *
 * - Connection event hook callbacks receive a @ref ib_conn_t parameter.
 * - Transaction event hook callbacks receive a @ref ib_tx_t parameter.
 * - Transaction Data event hook callbacks receive a @ref ib_txdata_t
 *   parameter.
 *
 * @note Config contexts and some fields are populated during the server
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
 *   request [label="HTTP (parsed)\nRequest\nData",style=solid,shape=box]
 *   response [label="HTTP (parsed)\nResponse\nData",style=solid,shape=box]
 *
 *   context_conn_selected [label="At this point the connection context is\nselected. Events that follow will use the context\nselected here, which may impose a different\nconfiguration than previous events. Anything\nused in the context selection process must\nbe generated in a previous event handler.",style=bold,shape=note,URL="\ref handle_context_conn_event"]
 *   context_tx_selected [label="At this point the transaction context is\nselected. Events that follow will use the context\nselected here, which may impose a different\nconfiguration than previous events. Anything\nused in the context selection process must\nbe generated in a previous event handler.\nAdditionally, any transaction data filters will\nnot be called until after this point so that\nfilters will be called with a single context.",style=filled,fillcolor="#e6e6e6",shape=note,URL="\ref handle_context_tx_event"]
 *
 *   conn_started_event [label="conn_started",style=bold,shape=diamond,URL="\ref conn_started_event"]
 *   conn_opened_event [label="conn_opened",style=bold,shape=octagon,URL="\ref conn_opened_event"]
 *   conn_closed_event [label="conn_closed",style=bold,shape=octagon,URL="\ref conn_closed_event"]
 *   conn_finished_event [label="conn_finished",style=bold,shape=diamond,URL="\ref conn_finished_event"]
 *
 *   tx_started_event [label="tx_started",style=filled,fillcolor="#e6e6e6",shape=diamond,URL="\ref tx_started_event"]
 *   tx_process_event [label="tx_process",style=filled,fillcolor="#e6e6e6",shape=diamond,URL="\ref tx_process_event"]
 *   tx_finished_event [label="tx_finished",style=filled,fillcolor="#e6e6e6",shape=diamond,URL="\ref tx_finished_event"]
 *
 *   request_started_event [label="request_started *",style=filled,fillcolor="#e6e6e6",shape=ellipse,URL="\ref request_started_event"]
 *   request_header_finished_event [label="request_header_finished",style=filled,fillcolor="#e6e6e6",shape=ellipse,URL="\ref request_header_finished_event"]
 *   request_body_data_event [label="request_body_data",style=filled,fillcolor="#e6e6e6",shape=ellipse,URL="\ref request_body_data_event"]
 *   request_finished_event [label="request_finished",style=filled,fillcolor="#e6e6e6",shape=ellipse,URL="\ref request_finished_event"]
 *   response_started_event [label="response_started *",style=filled,fillcolor="#e6e6e6",shape=ellipse,URL="\ref response_started_event"]
 *   response_header_finished_event [label="response_header_finished",style=filled,fillcolor="#e6e6e6",shape=ellipse,URL="\ref response_header_finished_event"]
 *   response_body_data_event [label="response_body_data",style=filled,fillcolor="#e6e6e6",shape=ellipse,URL="\ref response_body_data_event"]
 *   response_finished_event [label="response_finished",style=filled,fillcolor="#e6e6e6",shape=ellipse,URL="\ref response_finished_event"]
 *
 *   handle_context_conn_event [label="handle_context_conn **",style=bold,shape=parallelogram,URL="\ref handle_context_conn_event"]
 *   handle_connect_event [label="handle_connect",style=bold,shape=parallelogram,URL="\ref handle_connect_event"]
 *   handle_context_tx_event [label="handle_context_tx **",style=filled,fillcolor="#e6e6e6",shape=parallelogram,URL="\ref handle_context_tx_event"]
 *   handle_request_header_event [label="handle_request_header",style=filled,fillcolor="#e6e6e6",shape=parallelogram,URL="\ref handle_request_header_event"]
 *   handle_request_event [label="handle_request",style=filled,fillcolor="#e6e6e6",shape=parallelogram,URL="\ref handle_request_event"]
 *   handle_response_header_event [label="handle_response_header",style=filled,fillcolor="#e6e6e6",shape=parallelogram,URL="\ref handle_response_header_event"]
 *   handle_response_event [label="handle_response",style=filled,fillcolor="#e6e6e6",shape=parallelogram,URL="\ref handle_response_event"]
 *   handle_disconnect_event [label="handle_disconnect",style=bold,shape=parallelogram,URL="\ref handle_disconnect_event"]
 *   handle_postprocess_event [label="handle_postprocess",style=filled,fillcolor="#e6e6e6",shape=parallelogram,URL="\ref handle_postprocess_event"]
 *   handle_logging_event [label="handle_logging",style=filled,fillcolor="#e6e6e6",shape=parallelogram,URL="\ref handle_logging_event"]
 *
 *   // These are just for organizational purposes
 *   tx_started_event -> request [style=invis,weight=5.0]
 *   tx_started_event -> tx_finished_event [style=invis,weight=5.0]
 *   response_started_event -> response [style=invis,weight=5.0]
 *
 *   conn_started_event -> conn_opened_event [weight=100.0]
 *   conn_opened_event -> context_conn_selected [weight=100.0]
 *   context_conn_selected -> handle_context_conn_event [weight=100.0]
 *   handle_context_conn_event -> handle_connect_event [weight=100.0]
 *   handle_connect_event -> tx_started_event [weight=100.0]
 *
 *   tx_started_event -> request_started_event [weight=5.0]
 *   request_started_event -> request_header_finished_event [weight=1.0]
 *   request_header_finished_event -> context_tx_selected [weight=1.0]
 *   context_tx_selected -> handle_context_tx_event [weight=1.0]
 *   handle_context_tx_event -> handle_request_header_event [weight=1.0]
 *   handle_request_header_event -> request_started_event [label="HTTP\nPipeline\nRequest",style=dashed,weight=10.0]
 *   handle_request_header_event -> request_body_data_event [weight=1.0]
 *   request_body_data_event -> request_finished_event [weight=1.0]
 *   request_finished_event -> handle_request_event [weight=1.0]
 *   handle_request_event -> tx_process_event [weight=1.0]
 *
 *   tx_process_event -> response_started_event [weight=1.0]
 *
 *   response_started_event -> response_header_finished_event [weight=1.0]
 *   response_header_finished_event -> handle_response_header_event [weight=1.0]
 *   handle_response_header_event -> response_body_data_event [weight=1.0]
 *   response_body_data_event -> response_finished_event [weight=1.0]
 *   response_finished_event -> handle_response_event [weight=5.0]
 *   handle_response_event -> response_started_event [label="HTTP\nPipeline\nResponse",style=dashed,weight=10.0]
 *
 *   handle_response_event -> handle_postprocess_event [weight=5.0]
 *   handle_postprocess_event -> handle_logging_event [weight=5.0]
 *   handle_logging_event -> tx_finished_event [weight=5.0]
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
 * @warning Remember to update ib_event_table_init() in engine.c when names
 * change, states are added or removed, etc..
 */
typedef enum {
    /* Engine States */
    conn_started_event,           /**< Connection started
                                   * (Hook type:@ref ib_state_conn_hook_fn_t) */
    conn_finished_event,          /**< Connection finished
                                   * (Hook type:@ref ib_state_conn_hook_fn_t) */
    tx_started_event,             /**< Transaction started
                                   * (Hook type:@ref ib_state_tx_hook_fn_t) */
    tx_process_event,             /**< Transaction is about to be processed
                                   * (Hook type:@ref ib_state_tx_hook_fn_t) */
    tx_finished_event,            /**< Transaction finished
                                   * (Hook type:@ref ib_state_tx_hook_fn_t) */

    /* Handler States */
    handle_context_conn_event,    /**< Handle connection context chosen
                                   * (Hook type:@ref ib_state_conn_hook_fn_t) */
    handle_connect_event,         /**< Handle a connect
                                   * (Hook type:@ref ib_state_conn_hook_fn_t) */
    handle_context_tx_event,      /**< Handle transaction context chosen
                                   * (Hook type:@ref ib_state_tx_hook_fn_t) */
    handle_request_header_event,  /**< Handle the request header
                                   * (Hook type:@ref ib_state_tx_hook_fn_t) */
    handle_request_event,         /**< Handle the full request
                                   * (Hook type:@ref ib_state_tx_hook_fn_t) */
    handle_response_header_event, /**< Handle the response header
                                   * (Hook type:@ref ib_state_tx_hook_fn_t) */
    handle_response_event,        /**< Handle the full response
                                   * (Hook type:@ref ib_state_tx_hook_fn_t) */
    handle_disconnect_event,      /**< Handle a disconnect
                                   * (Hook type:@ref ib_state_conn_hook_fn_t) */
    handle_postprocess_event,     /**< Handle transaction post processing
                                   * (Hook type:@ref ib_state_tx_hook_fn_t) */
    handle_logging_event,         /**< Handle transaction logging
                                   * (Hook type:@ref ib_state_tx_hook_fn_t) */

    /* Server States */
    conn_opened_event,            /**< Server notified connection opened
                                   * (Hook type:@ref ib_state_conn_hook_fn_t) */
    conn_closed_event,            /**< Server notified connection closed
                                   * (Hook type:@ref ib_state_conn_hook_fn_t) */

    /* Parser States */
    request_started_event,        /**< Parser notified request has started
                                   * (Hook type:@ref ib_state_request_line_fn_t) */
    request_header_data_event,    /**< Parser notified of request header data
                                   * (Hook type:@ref ib_state_header_data_fn_t) */
    request_header_finished_event, /**< Parser notified of request header
                                    * (Hook type:@ref ib_state_tx_hook_fn_t) */
    request_body_data_event,       /**< Parser notified of request body
                                    * (Hook type:@ref ib_state_txdata_hook_fn_t) */
    request_finished_event,        /**< Parser notified request finished
                                    * (Hook type:@ref ib_state_tx_hook_fn_t) */
    response_started_event,        /**< Parser notified response started
                                    * (Hook type:@ref ib_state_response_line_fn_t) */
    response_header_data_event,    /**< Parser notified of response header data
                                    * (Hook type:@ref ib_state_header_data_fn_t) */
    response_header_finished_event,/**< Parser notified of response header
                                    * (Hook type:@ref ib_state_tx_hook_fn_t) */
    response_body_data_event,      /**< Parser notified of response body
                                    * (Hook type:@ref ib_state_txdata_hook_fn_t) */
    response_finished_event,       /**< Parser notified response finished
                                    * (Hook type:@ref ib_state_tx_hook_fn_t( */

    /* Logevent updated */
    handle_logevent_event,         /**< Logevent updated
                                    * (Hook type:@ref ib_state_tx_hook_fn_t) */

    /* Not an event, but keeps track of the number of events. */
    IB_STATE_EVENT_NUM,
} ib_state_event_type_t;

/**
 * State Event Hook Types
 **/
typedef enum {
    IB_STATE_HOOK_NULL,     /**< Hook has no parameter
                             * (Hook type: @ref ib_state_null_hook_fn_t) */
    IB_STATE_HOOK_INVALID,  /**< Something went wrong
                             * (Hook type: None) */
    IB_STATE_HOOK_CONN,     /**< Hook receives connection data
                             * (Hook type: @ref ib_state_conn_hook_fn_t) */
    IB_STATE_HOOK_TX,       /**< Hook receives ib_tx_t
                             * (Hook type: @ref ib_state_tx_hook_fn_t) */
    IB_STATE_HOOK_TXDATA,   /**< Hook receives ib_txdata_t
                             * (Hook type: @ref ib_state_txdata_hook_fn_t) */
    IB_STATE_HOOK_REQLINE,  /**< Hook receives ib_parsed_req_t
                             * (Hook type: @ref ib_state_request_line_fn_t) */
    IB_STATE_HOOK_RESPLINE, /**< Hook receives ib_parsed_resp_t
                             * (Hook type: @ref ib_state_response_line_fn_t) */
    IB_STATE_HOOK_HEADER    /**< Hook receives ib_parsed_header_t
                             * (Hook type: @ref ib_state_header_data_fn_t) */
} ib_state_hook_type_t;

/**
 * Hook type for an event.
 *
 * @param[in] event Event type.
 * @return Hook type or IB_STATE_HOOK_INVALID if bad event.
 **/
ib_state_hook_type_t ib_state_hook_type(ib_state_event_type_t event);

/**
 * Dataless Event Hook Callback Function.
 *
 * Related registration functions:
 * - ib_hook_null_register()
 * - ib_hook_null_unregister()
 *
 * Handles events: (None)
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
 * Data event for parsed header.
 *
 * Related registration functions:
 * - ib_hook_parsed_header_data_register()
 * - ib_hook_parsed_header_data_unregister()
 *
 * Handles events:
 * - @ref request_header_data_event
 * - @ref response_header_data_event
 *
 * @param[in] ib Engine handle
 * @param[in] tx Transaction.
 * @param[in] event Which event trigger the callback.
 * @param[in] header Parsed connection header.
 * @param[in] cbdata Callback data
 */
typedef ib_status_t (*ib_state_header_data_fn_t)(
    ib_engine_t *ib,
    ib_tx_t *tx,
    ib_state_event_type_t event,
    ib_parsed_header_t *header,
    void *cbdata);

/**
 * Data event for the start of a request.
 *
 * This provides a request line parsed from the start of the request.
 *
 * Related registration functions:
 * - ib_hook_parsed_req_line_register()
 * - ib_hook_parsed_req_line_unregister()
 *
 * Handles events:
 * - @ref request_started_event
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
 * Related registration functions:
 * - ib_hook_parsed_resp_line_register()
 * - ib_hook_parsed_resp_line_unregister()
 *
 * Handles events:
 * - @ref response_started_event
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
 * Related registration functions:
 * - ib_hook_conn_register()
 * - ib_hook_conn_unregister()
 *
 * Handles events:
 * - @ref conn_started_event
 * - @ref conn_finished_event
 * - @ref handle_context_conn_event
 * - @ref handle_connect_event
 * - @ref handle_disconnect_event
 * - @ref conn_opened_event
 * - @ref conn_closed_event
 *
 * @param[in] ib Engine handle
 * @param[in] tx Transaction.
 * @param[in] event Which event trigger the callback.
 * @param[in] conn Connection.
 * @param[in] cbdata Callback data
 */
// FIXME: event and conn parameters seem reversed in order
typedef ib_status_t (*ib_state_conn_hook_fn_t)(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_conn_t *conn,
    void *cbdata
);

/**
 * Transaction Event Hook Callback Function.
 *
 * This matches the NULL callback type as tx is already passed.
 *
 * Related registration functions:
 * - ib_hook_tx_register()
 * - ib_hook_tx_unregister()
 *
 * Handles events:
 * - @ref tx_started_event
 * - @ref tx_process_event
 * - @ref tx_finished_event
 * - @ref handle_context_tx_event
 * - @ref handle_request_header_event
 * - @ref handle_request_event
 * - @ref handle_response_header_event
 * - @ref handle_response_event
 * - @ref handle_postprocess_event
 * - @ref handle_logging_event
 * - @ref request_header_finished_event
 * - @ref request_finished_event
 * - @ref response_header_finished_event
 * - @ref response_finished_event
 * - @ref handle_logevent_event
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
 * Related registration functions:
 * - ib_hook_txdata_register()
 * - ib_hook_txdata_unregister()
 *
 * Handles events:
 * - @ref request_body_data_event
 * - @ref response_body_data_event
 *
 * @param[in] ib Engine handle
 * @param[in] tx Transaction.
 * @param[in] event Which event trigger the callback.
 * @param[in] txdata Transaction data.
 * @param[in] cbdata Callback data
 */
// FIXME: Should take: ib,tx,event,buf,len,cbdata?
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
ib_status_t DLL_PUBLIC ib_hook_null_unregister(
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
ib_status_t DLL_PUBLIC ib_hook_conn_unregister(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_conn_hook_fn_t cb
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
ib_status_t DLL_PUBLIC ib_hook_tx_unregister(
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
ib_status_t DLL_PUBLIC ib_hook_txdata_unregister(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_txdata_hook_fn_t cb
);

/**
 * Register a callback for a header data event.
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
    ib_state_header_data_fn_t cb,
    void *cbdata);

/**
 * Unregister a callback for a header data event.
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
    ib_state_header_data_fn_t cb);

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
 * Write out audit log.
 *
 * @param pi Provider instance
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_auditlog_write(ib_provider_inst_t *pi);

/**
 * @} IronBeeEngineEvent
 */

/**
 * @defgroup IronBeeFilter Filter
 * @ingroup IronBeeEngine
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
 * @param data Data
 * @param dlen Data length
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_fctl_data_add(ib_fctl_t *fc,
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
