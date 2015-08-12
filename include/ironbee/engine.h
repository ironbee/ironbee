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
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/cfgmap.h>
#include <ironbee/clock.h>
#include <ironbee/engine_types.h>
#include <ironbee/field.h>
#include <ironbee/hash.h>
#include <ironbee/logger.h>
#include <ironbee/parsed_content.h>
#include <ironbee/server.h>
#include <ironbee/stream.h>
#include <ironbee/strval.h>
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
 * IronBee engine version string.
 *
 * @returns The IronBee version string for the loaded library.
 */
const char DLL_PUBLIC *ib_engine_version(void);

/**
 * IronBee engine product name.
 *
 * @returns The IronBee product and version name for the loaded library.
 */
const char DLL_PUBLIC *ib_engine_product_name(void);

/**
 * IronBee engine version number.
 *
 * @returns The IronBee version number for the loaded library.
 */
uint32_t DLL_PUBLIC ib_engine_version_number(void);

/**
 * IronBee engine ABI number.
 *
 * @returns The IronBee ABI number for the loaded library.
 */
uint32_t DLL_PUBLIC ib_engine_abi_number(void);

/**
 * Create an engine handle.
 *
 * After creating the engine, the caller must configure defaults, such as
 * initial logging parameters.
 *
 * @param pib Address which new handle is written
 * @param server Information on the server instantiating the engine
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_engine_create(ib_engine_t **pib,
                                        const ib_server_t *server);

/**
 * Return the server object for an engine.
 *
 * @param[in] ib Engine to fetch server for.
 * @returns Server.
 **/
const ib_server_t DLL_PUBLIC *ib_engine_server_get(const ib_engine_t *ib);

/**
 * Get the engine's stream pump for the request.
 *
 * @param[in] ib The engine to fetch the request @ref ib_stream_pump_t out of.
 *
 * @returns The request @ref ib_stream_pump_t.
 */
ib_stream_pump_t DLL_PUBLIC *ib_engine_request_stream_pump(ib_engine_t *ib);

/**
 * Get the engine's stream pump for the response.
 *
 * @param[in] ib The engine to fetch the response @ref ib_stream_pump_t out of.
 *
 * @returns The response @ref ib_stream_pump_t.
 */
ib_stream_pump_t DLL_PUBLIC *ib_engine_response_stream_pump(ib_engine_t *ib);

/**
 * Return the logger object constructed for this engine.
 *
 * Use the returned object to add writers or change the log level.
 *
 * This will never return NULL as ib_engine_create() would have failed
 * with an IB_EALLOC.
 *
 * @returns Pointer to the logger structure for @a ib.
 */
ib_logger_t DLL_PUBLIC *ib_engine_logger_get(const ib_engine_t *ib);

/**
 * Get the engine's instance UUID
 *
 * @param ib Engine handle
 *
 * @returns Pointer to engine's instance UUID
 */
const char DLL_PUBLIC *ib_engine_instance_id(const ib_engine_t *ib);

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
 * @param[in] ib Engine handle
 *
 * @returns Status code:
 *  - IB_OK - All OK
 *  - Status from ib_context_close() (including the context close functions)
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
 * Returns the @ref ib_stream_processor_registry_t of this engine.
 */
ib_stream_processor_registry_t DLL_PUBLIC *
ib_engine_stream_processor_registry(
    ib_engine_t *ib
) NONNULL_ATTRIBUTE(1);

/**
 * Get a module by name.
 *
 * @param ib Engine handle
 * @param name Module name
 * @param pm Address which module will be written
 *
 * @returns
 * - IB_OK On success.
 * - IB_ENOENT If the named module is not found.
 */
ib_status_t DLL_PUBLIC ib_engine_module_get(const ib_engine_t *ib,
                                            const char * name,
                                            ib_module_t **pm);

/**
 * Get the main engine memory pool as memory manager.
 *
 * @param ib Engine handle
 *
 * @returns Memory manager
 */
ib_mm_t DLL_PUBLIC ib_engine_mm_main_get(const ib_engine_t *ib);

/**
 * Get the engine configuration memory pool as memory manager.
 *
 * @param ib Engine handle
 *
 * @returns Memory manager
 */
ib_mm_t DLL_PUBLIC ib_engine_mm_config_get(const ib_engine_t *ib);

/**
 * Get the temp engine memory pool as memory manager.
 *
 * The underlying pool should be destroyed by the server after the
 * configuration phase. Therefore is should not be used for
 * anything except temporary allocations which are required
 * for performing configuration.
 *
 * @param ib Engine handle
 *
 * @returns Memory manager
 */
ib_mm_t DLL_PUBLIC ib_engine_mm_temp_get(const ib_engine_t *ib);

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
 * Get var configuration of engine.
 *
 * @param[in] ib IronBee engine.
 * @return Var configuration.
 **/
ib_var_config_t DLL_PUBLIC *ib_engine_var_config_get(
    ib_engine_t *ib
);

/**
 * Get var configuration of engine (const version).
 *
 * @param[in] ib IronBee engine.
 * @return var configuration.
 **/
const ib_var_config_t DLL_PUBLIC *ib_engine_var_config_get_const(
    const ib_engine_t *ib
);

//! Callback for handling @ref ib_logevent_t generation.
typedef ib_status_t (*ib_engine_notify_logevent_fn)(
    ib_engine_t   *ib,
    ib_tx_t       *tx,
    ib_logevent_t *logevent,
    void          *cbdata
);

/**
 * Register a callback function to handle newly created events.
 *
 * @param[in] ib IronBee engine.
 * @param[in] fn The function to call on @ref ib_logevent_t generation.
 * @param[in] cbdata Callback data for @a fn.
 *
 * @returns
 * - IB_OK On succes.
 * - IB_EALLOC On allocation error.
 * - Other on unexpected failure.
 */
ib_status_t DLL_PUBLIC ib_engine_notify_logevent_register(
    ib_engine_t                  *ib,
    ib_engine_notify_logevent_fn  fn,
    void                         *cbdata
);

/**
 * Notify the state machine that a logevent event has occurred
 *
 * @param[in] ib Engine handle
 * @param[in] tx Transaction data
 * @param[in] logevent The log event.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
ib_status_t DLL_PUBLIC ib_engine_notify_logevent(
    ib_engine_t   *ib,
    ib_tx_t       *tx,
    ib_logevent_t *logevent
) NONNULL_ATTRIBUTE(1, 2, 3);

/**
 * Destroy an engine.
 *
 * @param ib Engine handle
 */
void DLL_PUBLIC ib_engine_destroy(ib_engine_t *ib);

/**
 * Merge the base_uuid with conn data and generate the conn id string.
 *
 * This function is normally executed by ib_conn_create(), but if the conn is
 * being created in other ways (e.g. in the tests), use this to generate the
 * CONN's ID.
 *
 * @param[in,out] conn Connection to populate
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_conn_generate_id(ib_conn_t *conn);

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
 * @param[out] data Data.  Can be any handle, i.e., `T **`.

 * @returns
 * - IB_OK on success.
 * - IB_ENOENT if @a conn does not know about @a module.
 */
ib_status_t DLL_PUBLIC ib_conn_get_module_data(
    const ib_conn_t   *conn,
    const ib_module_t *module,
    void              *data
);

/**
 * Set per-module per-connection data.
 *
 * @param[in] conn Connection
 * @param[in] module Module.
 * @param[in] data Data.  Set to NULL to unset.
 * @returns Status code.
 */
ib_status_t DLL_PUBLIC ib_conn_set_module_data(
    ib_conn_t         *conn,
    const ib_module_t *module,
    void              *data
);

/**
 * Returns the stream pump for the response body.
 *
 * @param[in] tx The transaction.
 *
 * @returns the stream pump for the response body.
 */
ib_stream_pump_t DLL_PUBLIC *ib_tx_response_body_pump(
    ib_tx_t *tx
) NONNULL_ATTRIBUTE(1);

/**
 * Returns the stream pump for the request body.
 *
 * @param[in] tx The transaction.
 *
 * @returns the stream pump for the request body.
 */
ib_stream_pump_t DLL_PUBLIC *ib_tx_request_body_pump(
    ib_tx_t *tx
) NONNULL_ATTRIBUTE(1);

/**
 * Set @a flags in the @a tx and the corresponding var value.
 *
 * @param[in] tx The transaction to set the flags in.
 * @param[in] flags The set of flags to set.
 *
 * @returns
 * - IB_OK On success.
 * - Other on var errors.
 */
ib_status_t DLL_PUBLIC ib_tx_flags_set(ib_tx_t *tx, ib_flags_t flags);

/**
 * Unset @a flags in the @a tx and the corresponding var value.
 *
 * @param[in] tx The transaction to set the flags in.
 * @param[in] flags The set of flags to set.
 *
 * @returns
 * - IB_OK On success.
 * - Other on var errors.
 */
ib_status_t DLL_PUBLIC ib_tx_flags_unset(ib_tx_t *tx, ib_flags_t flags);

/**
 * Get the string value for a specific TX flag
 *
 * @note If more than one flag is set, the string matching the first
 * one will be returned.
 *
 * @param[in] flags The flags to lookup
 *
 * @returns
 * "None" if no flags are set
 * String representation of first set flag
 */
const char DLL_PUBLIC *ib_tx_flags_name(ib_flags_t flags);

/**
 * Get the first TX flag strval
 *
 * @returns Pointer to the first flag strval
 */
const ib_strval_t DLL_PUBLIC *ib_tx_flags_strval_first();

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
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_tx_generate_id(ib_tx_t *tx);

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
 * @param[in]  tx     Transaction.
 * @param[in]  module Module.
 * @param[out] data   Data.  Can be any handle, i.e., `T **`.
 * @returns
 * - IB_OK on success.
 * - IB_ENOENT if @a tx does not know about @a module.
 */
ib_status_t DLL_PUBLIC ib_tx_get_module_data(
    const ib_tx_t     *tx,
    const ib_module_t *module,
    void              *data
);

/**
 * Set per-module per-transaction data.
 *
 * @param[in] tx Transaction.
 * @param[in] module Module.
 * @param[in] data Data.  Set to NULL to unset.
 * @returns Status code.
 */
ib_status_t DLL_PUBLIC ib_tx_set_module_data(
    ib_tx_t           *tx,
    const ib_module_t *module,
    void              *data
);

/**
 * Set server error status code.
 *
 * @param[in] tx The transaction.
 * @param[in] status The status code.
 *
 * @returns
 *   - IB_OK On success.
 *   - IB_ENOTIMPL If not implemented by the server plugin.
 */
ib_status_t DLL_PUBLIC ib_tx_server_error(
    ib_tx_t *tx,
    int status
);

/**
 * Set server error header.
 *
 * @param[in] tx The transaction.
 * @param[in] name Name of header.
 * @param[in] name_length Length of @a name.
 * @param[in] value value of header.
 * @param[in] value_length Length of @a value.
 *
 * @returns
 *   - IB_OK On success.
 *   - IB_ENOTIMPL If not implemented by the server plugin.
 */
ib_status_t DLL_PUBLIC ib_tx_server_error_header(
    ib_tx_t    *tx,
    const char *name,
    size_t      name_length,
    const char *value,
    size_t      value_length
);

/**
 * Set server error data.
 *
 * @param[in] tx The transaction.
 * @param[in] data The data to set.
 * @param[in] dlen The length of the data to send.
 *
 * @returns
 *   - IB_OK On success.
 *   - IB_ENOTIMPL If not implemented by the server plugin.
 */
ib_status_t DLL_PUBLIC ib_tx_server_error_data(
    ib_tx_t *tx,
    const uint8_t *data,
    size_t dlen
);

/**
 * Set a header to be either requested or responded to in the server.
 *
 * @param[in] tx The transaction.
 * @param[in] dir Direction. This indicates if this is for the request or
 *            the response of the HTTP transaction.
 * @param[in] action How to add this header. Add, delete, set, etc.
 * @param[in] name Name of header.
 * @param[in] name_length Length of @a name.
 * @param[in] value value of header.
 * @param[in] value_length Length of @a value.
 *
 * @returns
 *   - IB_OK On success.
 *   - IB_ENOTIMPL If not implemented by the server plugin.
 */
ib_status_t DLL_PUBLIC ib_tx_server_header(
    ib_tx_t                   *tx,
    ib_server_direction_t      dir,
    ib_server_header_action_t  action,
    const char                *name,
    size_t                     name_length,
    const char                *value,
    size_t                     value_length
);

/**
 * Destroy a transaction structure.
 *
 * The transaction @a tx MUST be the first transaction in the parent
 * connection's (tx->conn) transaction list.  This function also removes
 * @a tx from the parent connection's list, and updates the connection's
 * first (tx_first) and last (tx_last) transaction pointers.
 *
 * @param tx Transaction structure
 */
void DLL_PUBLIC ib_tx_destroy(ib_tx_t *tx) NONNULL_ATTRIBUTE(1);

/**
 * @} IronBeeEngineEvent
 */

/**
 * @defgroup IronBeeResponse Response
 * @ingroup IronBeeEngine
 * @{
 */

/**
 * Send a response through the IronBee server API for @a tx.
 *
 * @param[in] tx Transaction.
 * @param[in] status The HTTP status code.
 * @param[in] headers Headers to include in the response or NULL.
 * @param[in] body The body to include in the response or NULL.
 *
 * @return
 * - IB_OK On success.
 * - IB_ENOIMPL If the server does not implement a required callback.
 * - IB_EALLOC On allocation errors.
 * - IB_EINVAL If @a status is not valid.
 * - Other on unexpected errors.
 */
ib_status_t DLL_PUBLIC ib_tx_response(
    ib_tx_t             *tx,
    int                  status,
    ib_parsed_headers_t *headers,
    ib_bytestr_t        *body
) NONNULL_ATTRIBUTE(1);

/**
 * @} IronBeeResponse
 */

/**
 * @defgroup IronBeeBlock Blocking
 * @ingroup IronBeeEngine
 * @{
 */

/**
 * Block transaction.
 *
 * This function works as follow:
 *
 * 1. If {{ib_tx_block()}} has already been called on this transaction,
 *    return IB_OK.  Record that {{ib_tx_block()}} has been called on this
 *    transaction.
 * 2. If {{ib_tx_allow()}} has been called on this transaction,
 *    return IB_DECLINED.
 * 3. Call all pre-block hooks.  See ib_register_block_pre_hook().
 * 4. If a block handler is registered, call it to get the blocking info.
 *    If it returns IB_DECLINED, return IB_DECLINED.  See
 *    ib_block_register_handler() and @ref ib_block_info_t.
 * 5. If no block handler is registered, call a default block handler to
 *    get the blocking info.
 * 6. If blocking is not enabled, the function returns IB_DECLINED.  See
 *    ib_tx_is_blocking_enabled(), ib_tx_enable_blocking(), and
 *    ib_tx_disable_blocking().
 * 7. Communicate the blocking info to the server and mark the transaction as
 *    blocked (see ib_tx_is_blocked()).
 * 8. Call all post-block hooks.
 *
 * @note Hooks and the handler are called at most once.  Per-block hooks are
 * called the first time {{ib_tx_block()}} is called on a transaction.  If
 * blocking is enabled, then the handler is called.  If the handler succeeds,
 * then post-block hooks are called.
 *
 * @note Pre-block hooks are allowed to enable or disable blocking.
 *
 * @note The default block handler returns a 403 status code.
 *
 * @note The default state of whether blocking is enabled is set by the core
 * module based on the protection engine configuration.  Blocking enabled is
 * determined by the presence of IB_TX_FBLOCKING_MODE being set in the
 * tx flags.
 *
 * @return
 * - IB_OK on success (including if @a tx already blocked).
 * - IB_DECLINED if blocking is disabled or block handler returns IB_DECLINED.
 * - IB_ENOTIMPL if the server does not support the desired blocking method.
 * - Other if server, handler, or callback reports error.
 **/
ib_status_t DLL_PUBLIC ib_tx_block(ib_tx_t *tx) NONNULL_ATTRIBUTE(1);

/**
 * Explicitly allow a transaction.
 *
 * This is the compliment of {{ib_tx_block()}}, not the reciprocal.
 * That is, this will force a TX to be allowed. An allowed
 * TX cannot be blocked later. A blocked TX cannot be "unblocked" by this
 * function.
 *
 * @param[in] tx The transaction to allow.
 *
 * @return
 * - IB_OK On success or if @a tx is already allowed.
 * - IB_DECLINED If @a tx has already been blocked by a call to
 *   {{ib_tx_block()}}.
 */
ib_status_t DLL_PUBLIC ib_tx_allow(ib_tx_t *tx) NONNULL_ATTRIBUTE(1);

/**
 * Enable blocking for transaction @a tx.
 *
 * Equivalent to setting the IB_TX_FBLOCKING_MODE tx flag.
 *
 * @param[in] tx Transaction to enable blocking on.
 **/
void DLL_PUBLIC ib_tx_enable_blocking(ib_tx_t *tx) NONNULL_ATTRIBUTE(1);

/**
 * Disable blocking for transaction @a tx.
 *
 * Equivalent to unsetting the IB_TX_FBLOCKING_MODE tx flag.
 *
 * @param[in] tx Transaction to enable blocking on.
 **/
void DLL_PUBLIC ib_tx_disable_blocking(ib_tx_t *tx) NONNULL_ATTRIBUTE(1);

/**
 * Is blocking enabled for transaction @a tx.
 *
 * Equivalent to checking the IB_TX_FBLOCKING_MODE tx flag.
 *
 * @param[in] tx Transaction to check.
 * @return true iff blocking is enabled for transaction.
 **/
bool DLL_PUBLIC ib_tx_is_blocking_enabled(const ib_tx_t *tx)
    NONNULL_ATTRIBUTE(1);

/**
 * Check if transaction is blocked.
 *
 * A transaction is blocked, if ib_tx_block() was called on it.
 *
 * @param[in] tx Transaction to check.
 * @return True iff @a tx is blocked.
 **/
bool DLL_PUBLIC ib_tx_is_blocked(const ib_tx_t *tx) NONNULL_ATTRIBUTE(1);

/**
 * Check if transaction is explicitly allowed.
 *
 * A transaction is allowed, if ib_tx_allowed() was called on it.
 *
 * @param[in] tx Transaction to check.
 * @return True iff @a tx is allowed.
 **/
bool DLL_PUBLIC ib_tx_is_allowed(const ib_tx_t *tx) NONNULL_ATTRIBUTE(1);

/**
 * Check if transaction has communicated its block to the server plugin.
 *
 * A transaction is blocked if ib_tx_block() was called on it
 * and the block handler returned IB_OK (instead of IB_DECLINED or an error).
 *
 * @param[in] tx Transaction to check.
 * @return True iff @a tx has communicated its block to the server plugin.
 **/
bool DLL_PUBLIC ib_tx_block_applied(const ib_tx_t *tx) NONNULL_ATTRIBUTE(1);

/**
 * Fetch block information.
 *
 * If ib_tx_is_blocked() is false, return is undefined.
 *
 * @param[in] tx Transaction to check.
 * @return Block info for transaction.
 **/
ib_block_info_t ib_tx_block_info(const ib_tx_t* tx) NONNULL_ATTRIBUTE(1);

/**
 * Transaction block handler.
 *
 * A block handler determines how to block a transaction.  It is allowed to
 * decline to block, but this feature should be used cautiously.  It is
 * preferable to allow other code, such as a block pre-hooks to determine
 * whether to block.
 *
 * @param[in]  tx Transaction to block.
 * @param[out] info Block information to communicate to server.
 * @param[in]  cbdata Callback data.
 * @return
 * - IB_DECLINED if decline to block.
 * - IB_OK on success.
 * - Other on error.
 **/
typedef ib_status_t (*ib_block_handler_fn_t)(
    ib_tx_t         *tx,
    ib_block_info_t *info,
    void            *cbdata
);

/**
 * Transaction block pre-hook.
 *
 * Block pre-hooks are called on the first block request  They are allowed to
 * call ib_tx_enable_blocking() and ib_tx_disable_blocking().  Note, however,
 * that if a transaction has already been blocked (see ib_tx_is_blocked()),
 * then any enabling/disabling of blocking will have no effect.  In many
 * cases, it is advisable to have your pre-hook check if the transaction was
 * already blocked before doing anything else.
 *
 * @param[in] tx Transaction.
 * @param[in] cbdata Callback data.
 * @return
 * - IB_OK on success.
 * - Other on error.
 **/
typedef ib_status_t (*ib_block_pre_hook_fn_t)(
    ib_tx_t *tx,
    void    *cbdata
);

/**
 * Transaction block post-hook.
 *
 * Block pre-hooks are called at most once per transaction: immediately after
 * the block handler is called.
 *
 * @param[in] tx Transaction.
 * @param[in] info How the transaction was blocked.
 * @param[in] cbdata Callback data.
 * @return
 * - IB_OK on success.
 * - Other on error.
 **/
typedef ib_status_t (*ib_block_post_hook_fn_t)(
    ib_tx_t               *tx,
    const ib_block_info_t *info,
    void                  *cbdata
);

/**
 * Register a transaction block handler.
 *
 * There can be only one transaction block handler.
 *
 * @param[in] ib Engine to register with.
 * @param[in] name Name of handler for use in logging.
 * @param[in] handler Handler to register.
 * @param[in] cbdata Callback data.
 * @return
 * - IB_OK on success.
 * - IB_EINVAL if already a handler registered.
 * - IB_EALLOC on allocation failure.
 **/
ib_status_t DLL_PUBLIC ib_register_block_handler(
    ib_engine_t           *ib,
    const char            *name,
    ib_block_handler_fn_t  handler,
    void                  *cbdata
);

/**
 * Register a transaction pre-block callback.
 *
 * @param[in] ib Engine to register with.
 * @param[in] name Name of hook for use in logging.
 * @param[in] pre_hook hook to register.
 * @param[in] cbdata Callback data.
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 **/
ib_status_t DLL_PUBLIC ib_register_block_pre_hook(
    ib_engine_t            *ib,
    const char             *name,
    ib_block_pre_hook_fn_t  pre_hook,
    void                   *cbdata
);

/**
 * Register a transaction post-block callback.
 *
 * @param[in] ib Engine to register with.
 * @param[in] name Name of hook for use in logging.
 * @param[in] post_hook hook to register.
 * @param[in] cbdata Callback data.
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 **/
ib_status_t DLL_PUBLIC ib_register_block_post_hook(
    ib_engine_t             *ib,
    const char              *name,
    ib_block_post_hook_fn_t  post_hook,
    void                    *cbdata
);

/**
 * @} IronBeeBlock
 **/

/**
 * Return the sensor ID.
 *
 * @param[in] ib IronBee engine.
 *
 * @returns The sensor ID value in the engine.
 */
const char DLL_PUBLIC *ib_engine_sensor_id(const ib_engine_t *ib);

/**
 * @} IronBeeEngine
 * @} IronBee
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_ENGINE_H_ */
