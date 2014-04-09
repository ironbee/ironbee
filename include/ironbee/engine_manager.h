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

#ifndef _IB_ENGINE_MANAGER_H_
#define _IB_ENGINE_MANAGER_H_

/**
 * @file
 * @brief IronBee --- Engine Manager
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include <ironbee/engine.h>
#include <ironbee/log.h>
#include <ironbee/types.h>

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeEngineManager IronBee Engine Manager
 * @ingroup IronBee
 *
 * The engine manager provides services to manage multiple IronBee engines.
 *
 * In the current implementation, all of these engines run in the same process
 * space.
 *
 * @{
 */

/* Local definitions */
#define IB_MANAGER_DEFAULT_MAX_ENGINES  8  /**< Default max # of engines */


/* Engine Manager type declarations */

/**
 * The engine manager.
 *
 * An engine manager is created via ib_manager_create().
 *
 * Servers which use the engine manager will typically create a single engine
 * manager at startup, and then use the engine manager to create engines when
 * the configuration has changed via ib_manager_engine_create().
 *
 * The engine manager will then manage the IronBee engines, with the most
 * recent one successfully created being the "current" engine.  An engine
 * managed by the manager is considered active if it is current or its
 * reference count is non-zero.
 *
 * ib_manager_engine_acquire() is used to acquire the current engine.  A
 * matching call to ib_manager_engine_release() is required to release it.  If
 * the released engine becomes inactive (e.g., the engine is not current and
 * its reference count becomes zero), the manager will destroy all inactive
 * engines.
 *
 */
typedef struct ib_manager_t ib_manager_t;

/**
 * Callback function to create a module structure using a given
 *
 * This should not call ib_module_init() as the manager will do that.
 *
 * The resulting @ref ib_module_t is copied with ib_module_dup() to avoid
 * accidental sharing of module structures.
 *
 * @param[out] module The module to create or return.
 * @param[in] ib The IronBee engine the module is being created for.
 * @param[in] cbdata Callback data.
 *
 * @returns
 * - IB_OK On success.
 * - IB_DECLINE If no module was created but the function is defined.
 * - Other on error. Creation of the IronBee engine fails.
 */
typedef ib_status_t (*ib_manager_module_create_fn_t)(
    ib_module_t **module,
    ib_engine_t  *ib,
    void         *cbdata
);

/* Engine Manager API */

/**
 * Create an engine manager.
 *
 * @param[out] pmanager Pointer to IronBee engine manager object
 * @param[in] server IronBee server object
 * @param[in] max_engines Maximum number of simultaneous engines
 *
 * @returns Status code
 * - IB_OK if all OK
 * - IB_EALLOC for allocation problems
 */
ib_status_t DLL_PUBLIC ib_manager_create(
    ib_manager_t      **pmanager,
    const ib_server_t  *server,
    size_t              max_engines
);

/**
 * Register a single module creation callback function.
 *
 * Currently only one module create function can be registered at a time.
 * Will replace any already registered callback.
 *
 * @param[in] manager Engine manager to register function with.
 * @param[in] module_fn Function to return a module structure for a server
 *            module that will be registered to a newly created IronBee
 *            engine. The module will be added before the engine is
 *            configured. This may be NULL to remove a module
 *            callback.
 * @param[in] module_data Callback data for @a module_fn.
 *
 * @returns Currently this always returns IB_OK.
 */
ib_status_t ib_manager_register_module_fn(
    ib_manager_t                  *manager,
    ib_manager_module_create_fn_t  module_fn,
    void                          *module_data
);

/**
 * A function called before @a ib is configured.
 *
 * @param[in] manager The engine manager.
 * @param[in] ib The ironbee engine.
 * @param[in] cbdata The callback data provided when this function was
 *            registered.
 * @returns
 * - IB_OK On success.
 * - Other on failure. Returning non-IB_OK will cause engine creation
 *   to fail. This function should not return non-IB_OK values
 *   unless the error is serious enough to prevent engine creation.
 */
typedef ib_status_t (* ib_manager_engine_preconfig_fn_t)(
    ib_manager_t *manager,
    ib_engine_t  *ib,
    void         *cbdata
);

/**
 * A function called after @a ib has been configured.
 *
 * @param[in] manager The engine manager.
 * @param[in] ib The ironbee engine.
 * @param[in] cbdata The callback data provided when this function was
 *            registered.
 * @returns
 * - IB_OK On success.
 * - Other on failure. Returning non-IB_OK will cause engine creation
 *   to fail. This function should not return non-IB_OK values
 *   unless the error is serious enough to prevent engine creation.
 */
typedef ib_status_t (* ib_manager_engine_postconfig_fn_t)(
    ib_manager_t *manager,
    ib_engine_t  *ib,
    void         *cbdata
);


/**
 * Add a callback to run on an engine before it is to be configured.
 *
 * @param[in] manager The engine manager.
 * @param[in] preconfig_fn The callback function.
 * @param[in] cbdata The callback data for @a preconfig_fn.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC On memory allocation error.
 * - Other on failure.
 */
ib_status_t DLL_PUBLIC ib_manager_engine_preconfig_fn_add(
    ib_manager_t                     *manager,
    ib_manager_engine_preconfig_fn_t  preconfig_fn,
    void                             *cbdata
) NONNULL_ATTRIBUTE(1);

/**
 * Add a callback to run on an engine after it has been configured.
 *
 * @param[in] manager The engine manager.
 * @param[in] postconfig_fn The callback function.
 * @param[in] cbdata The callback data for @a postconfig_fn.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC On memory allocation error.
 * - Other on failure.
 */
ib_status_t DLL_PUBLIC ib_manager_engine_postconfig_fn_add(
    ib_manager_t                      *manager,
    ib_manager_engine_postconfig_fn_t  postconfig_fn,
    void                              *cbdata
) NONNULL_ATTRIBUTE(1);

/**
 * Destroy an engine manager.
 *
 * Destroys IronBee engines managed by @a manager, and the engine manager
 * itself. Users of the Engine Manager should be sure to not destroy
 * an @ref ib_manager_t while @ref ib_engine_t s provided by it
 * are still in use.
 *
 * @param[in] manager IronBee engine manager
 */
void DLL_PUBLIC ib_manager_destroy(
    ib_manager_t *manager
);

/**
 * Create a new IronBee engine and set it as the current engine.
 *
 * The previous engine is not destroyed so other threads using it
 * can call ib_manager_engine_release() on it. If there are too many
 * engines (the max engines limit is reached) then an attempt will be
 * made to find and destroy engines with nothing referencing them.
 * If the cleanup attempt fails, then this returns @c IB_DECLINED.
 *
 * @param[in] manager IronBee engine manager
 * @param[in] config_file Configuration file path
 *
 * @sa ib_manager_enable()
 * @sa ib_manager_disable()
 *
 * @returns Status code
 * - IB_OK All OK
 * - IB_EALLOC If memory allocation fails.
 * - IB_DECLINED The max number of engines has been created and
 *   no engine could be destroyed because it is still recorded as being
 *   referenced or if the manager has been disabled by ib_manager_disable().
 * - Other on internal API failures.
 */
ib_status_t DLL_PUBLIC ib_manager_engine_create(
    ib_manager_t *manager,
    const char   *config_file
);

/**
 * Re-enable an manager after a call to ib_manager_disable().
 *
 * The manager will, after this call, act as if it had just
 * been created. It should have ib_manager_engine_create() called
 * if an engine is immediately disired.
 *
 * @param[in] manager The manager to enable.
 *
 * @sa ib_manager_disable()
 *
 * @returns
 * - IB_OK On success.
 * - Other on locking error.
 */
ib_status_t DLL_PUBLIC ib_manager_enable(
    ib_manager_t *manager
);

/**
 * Disable a manager so that IronBee is effectively @em off.
 *
 * This call has quite a few effects that all, generally, revert the
 * engine manager back to a state before the first call to
 * ib_manager_engine_create(). Calls to ib_manager_engine_create()
 * do nothing until ib_manager_enable() is called.
 *
 * More specifically:
 * - The current engine is set to NULL and the engine manager's reference to it
 *   is removed.
 * - The current engine is signaled to shut down.
 * - A flag is set in the manager to disable calls to
 *   ib_manager_engine_create(). It will return @ref IB_DECLINED
 *   until the manager is enabled.
 * - Inactive engines are destroyed. Active engines, those engines with
 *   a reference being held by a server, still exist and must be cleaned up
 *   with ib_manager_engine_cleanup().
 *
 * @param[in] manager The manager to disable.
 *
 * @sa ib_manager_enable()
 *
 * @returns
 * - IB_OK On success.
 * - Other on locking error.
 */
ib_status_t DLL_PUBLIC ib_manager_disable(
    ib_manager_t *manager
);

/**
 * Acquire the current IronBee engine.
 *
 * This function increments the reference count associated with the current
 * engine, and then returns that engine.
 *
 * Any engine provided by this interface must have
 * ib_manager_engine_release() called on it.
 *
 * @param[in] manager IronBee engine manager
 * @param[out] pengine Pointer to the current engine
 *
 * @returns Status code
 *   - IB_OK All OK
 *   - IB_DECLINED No current IronBee engine exists
 */
ib_status_t DLL_PUBLIC ib_manager_engine_acquire(
    ib_manager_t  *manager,
    ib_engine_t  **pengine
);

/**
 * Relinquish use of @a engine.
 *
 * If @a engine is not the current engine and for every call to
 * ib_manager_engine_acquire() there has been a corresponding
 * call to ib_manage_engine_release(), then the engine will be
 * destroyed.
 *
 * Engine destruction may be deferred or it may be immediate.
 *
 * @param[in] manager IronBee engine manager.
 * @param[in] engine IronBee engine to release. If this engine is
 *            unknown to the manager, this function call has no effect
 *            and IB_EINVAL will be returned.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EINVAL If @a engine is not found in @a manager.
 * - Other on unexpected failures.
 */
ib_status_t DLL_PUBLIC ib_manager_engine_release(
    ib_manager_t *manager,
    ib_engine_t  *engine
);

/**
 * Destroy any inactive engines.
 *
 * Inactive engines are those engines with a reference count of zero.
 *
 * @param[in] manager IronBee engine manager
 *
 * @returns
 * - IB_OK On success.
 * - Other on API calls.
 */
ib_status_t DLL_PUBLIC ib_manager_engine_cleanup(
    ib_manager_t *manager
);

/**
 * Get the count of IronBee engines.
 *
 * @param[in] manager IronBee engine manager
 *
 * @returns Count of total IronBee engines
 */
size_t DLL_PUBLIC ib_manager_engine_count(
    const ib_manager_t *manager
);

/** @} */

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* _IB_ENGINE_MANAGER_H_ */
