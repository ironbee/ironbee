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

#ifndef _IB_CONTEXT_SELECTION_H_
#define _IB_CONTEXT_SELECTION_H_

/**
 * @file
 * @brief IronBee --- Context selection
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

/**
 * @defgroup IronBeeContextSelection Context Selection
 * @ingroup IronBee
 *
 * Definitions and functions related to context selection
 *
 * @{
 */

#include <ironbee/engine_types.h>
#include <ironbee/hash.h>
#include <ironbee/mm.h>
#include <ironbee/site.h>
#include <ironbee/types.h>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Types used only in context selection */
typedef struct ib_ctxsel_registration_t ib_ctxsel_registration_t;

/**
 * Configuration Context Selection Function.
 *
 * This is the "main" site selection function.  It is the responsibility of
 * this function to search through the known contexts and select the best
 * based on the connection and transaction.  At the start of a connection,
 * this function will be invoked with a NULL transaction (tx); the
 * selection function should attempt to find the best "site" context in
 * this case; if the transaction is supplied, the selection function should
 * attempt to find the best "location" context.
 *
 * @param[in] ib Engine
 * @param[in] conn Connection to select a context for
 * @param[in] tx Transaction to select a context for (or NULL)
 * @param[in] common_cb_data Callback data passed to all registered fns
 * @param[in] fn_cb_data Function-specific callback data
 * @param[out] pctx Pointer to selected context
 *
 * @returns Status code
 */
typedef ib_status_t (* ib_ctxsel_select_fn_t)(
    const ib_engine_t *ib,
    const ib_conn_t *conn,
    const ib_tx_t *tx,
    void *common_cb_data,
    void *fn_cb_data,
    ib_context_t **pctx);

/**
 * Configuration Context Create Site Function
 *
 * This function is normally invoked by the core module in the processing of a
 * site directive.
 *
 * @param[in] ctx Site's configuration context
 * @param[in] name Site name
 * @param[in] common_cb_data Callback data passed to all registered fns
 * @param[in] fn_cb_data Function-specific callback data
 * @param[out] psite Address where site will be written / NULL
 *
 * @returns Status code
 */
typedef ib_status_t (* ib_ctxsel_site_create_fn_t) (
    ib_context_t *ctx,
    const char *name,
    void *common_cb_data,
    void *fn_cb_data,
    ib_site_t **psite);

/**
 * Configuration Context Create Location Function
 *
 * This function is normally invoked by the core module in the processing of a
 * location directive.
 *
 * @param[in] site Parent site
 * @param[in] ctx Locations' configuration context
 * @param[in] name Location string (path)
 * @param[in] common_cb_data Callback data passed to all registered fns
 * @param[in] fn_cb_data Function-specific callback data
 * @param[out] plocation Address where location will be written / NULL
 *
 * @returns Status code
 */
typedef ib_status_t (* ib_ctxsel_location_create_fn_t) (
    const ib_site_t *site,
    ib_context_t *ctx,
    const char *name,
    void *common_cb_data,
    void *fn_cb_data,
    ib_site_location_t **plocation);

/**
 * Configuration Context Create Host Function
 *
 * This function is normally invoked by the core module in the processing of a
 * host directive.
 *
 * @param[in] site Parent site
 * @param[in] host_str Host string (host name)
 * @param[in] common_cb_data Callback data passed to all registered fns
 * @param[in] fn_cb_data Function-specific callback data
 * @param[out] phost Address where host will be written / NULL
 *
 * @returns Status code
 */
typedef ib_status_t (* ib_ctxsel_host_create_fn_t) (
    const ib_site_t *site,
    const char *host_str,
    void *common_cb_data,
    void *fn_cb_data,
    ib_site_host_t **phost);

/**
 * Configuration Context Create Service Function
 *
 * This function is normally invoked by the core module in the processing of a
 * host directive.
 *
 * @param[in] site Parent site
 * @param[in] service_str Service string ([ip][:[port]])
 * @param[in] common_cb_data Callback data passed to all registered fns
 * @param[in] fn_cb_data Function-specific callback data
 * @param[out] pservice Address where service will be written / NULL
 *
 * @returns Status code
 */
typedef ib_status_t (* ib_ctxsel_service_create_fn_t) (
    const ib_site_t *site,
    const char *service_str,
    void *common_cb_data,
    void *fn_cb_data,
    ib_site_service_t **pservice);

/**
 * Configuration Context Site Open Function.
 *
 * This function is invoked when the parser has begun processing of a new
 * site, and is invoked after both the site object and context have been
 * created.  The function may perform whatever processing is required at this
 * time.
 *
 * @param[in] ib Engine
 * @param[in] site Site to open
 * @param[in] common_cb_data Common callback data
 * @param[in] fn_cb_data Function-specific callback data
 *
 * @returns Status code
 */
typedef ib_status_t (* ib_ctxsel_site_open_fn_t)(
    const ib_engine_t *ib,
    ib_site_t *site,
    void *common_cb_data,
    void *fn_cb_data);

/**
 * Configuration Context Location Open Function.
 *
 * This function is invoked when the parser has begun processing of a new
 * location, and is invoked after both the location object and context have
 * been created.  The function may perform whatever processing is required at
 * this time.
 *
 * @param[in] ib Engine
 * @param[in] location Location to open
 * @param[in] common_cb_data Common callback data
 * @param[in] fn_cb_data Function-specific callback data
 *
 * @returns Status code
 */
typedef ib_status_t (* ib_ctxsel_location_open_fn_t)(
    const ib_engine_t *ib,
    ib_site_location_t *location,
    void *common_cb_data,
    void *fn_cb_data);

/**
 * Configuration Context Site Close Function.
 *
 * This function is invoked when the parser has completed processing of a
 * site; the function may perform whatever processing is required at this
 * time.
 *
 * @param[in] ib Engine
 * @param[in] site Site to close
 * @param[in] common_cb_data Common callback data
 * @param[in] fn_cb_data Function-specific callback data
 *
 * @returns Status code
 */
typedef ib_status_t (* ib_ctxsel_site_close_fn_t)(
    const ib_engine_t *ib,
    ib_site_t *site,
    void *common_cb_data,
    void *fn_cb_data);

/**
 * Configuration Context Location Close Function.
 *
 * This function is invoked when the parser has completed processing of a
 * location; the function may perform whatever processing is required at this
 * time.
 *
 * @param[in] ib Engine
 * @param[in] location Location to close
 * @param[in] common_cb_data Common callback data
 * @param[in] fn_cb_data Function-specific callback data
 *
 * @returns Status code
 */
typedef ib_status_t (* ib_ctxsel_location_close_fn_t)(
    const ib_engine_t *ib,
    ib_site_location_t *location,
    void *common_cb_data,
    void *fn_cb_data);

/**
 * Configuration Context Finalize Function.
 *
 * This function is invoked when the parser has completed processing of the
 * entire configuration; the function may perform whatever processing is
 * required at this time.
 *
 * @param[in] ib Engine
 * @param[in] common_cb_data Common callback data
 * @param[in] fn_cb_data Function-specific callback data
 *
 * @returns Status code
 */
typedef ib_status_t (* ib_ctxsel_finalize_fn_t)(
    const ib_engine_t *ib,
    void *common_cb_data,
    void *fn_cb_data);


/**
 * Context selection registration data accessor functions.
 *
 * These functions are used to allocate, initialize and modify a context
 * selection registration object.  After the registration is complete (via @sa
 * ib_ctxsel_register()), the object may be destroyed.
 *
 * The "select_fn" is required; all others are optional (but many are probably
 * required to build a useful context selector).
 */

/**
 * Initialize a context selection object.
 *
 * This function will create and initialize the registration object.  If the
 * @a mp parameter is NULL, @sa malloc() will be used to create the object,
 * otherwise @sa ib_mm_alloc() will be used for allocations from @a mm.  If
 * @a mm is IB_MM_NULL, it is the caller's responsibility to @sa free() the
 * memory.
 *
 * The @a common_cb_data parameter is stored with the registration, and this
 * parameter will be passed as the common_cb_data parameter to all callback
 * functions.  Note that each function is also passed a function-specific
 * fn_cb_data callback data pointer, which is passed to each of the specific
 * registration store functions.
 *
 * @param[in] mm Memory manager to use for allocations or IB_MM_NULL to use
 *               malloc()
 * @param[in] module Module performing the registrations
 * @param[in] common_cb_data Callback data common to all registered functions
 * @param[out] pregistration Pointer to registration object
 *
 * @returns Status code:
 *  - IB_OK
 *  - IB_EALLOC for allocation errors
 */
ib_status_t ib_ctxsel_registration_create(
    ib_mm_t mm,
    ib_module_t *module,
    void *common_cb_data,
    ib_ctxsel_registration_t **pregistration);

/**
 * Register a selection function with a context selection object
 *
 * @param[in,out] registration Initialized registration object
 * @param[in] select_fn Context selection function
 * @param[in] fn_cb_data Function-specific callback data
 *
 * @returns Status code:
 *  - IB_OK
 *  - IB_EALLOC for allocation errors
 */
ib_status_t ib_ctxsel_registration_store_select(
    ib_ctxsel_registration_t *registration,
    ib_ctxsel_select_fn_t select_fn,
    void *fn_cb_data);

/**
 * Store a site create function with a context selection object
 *
 * @param[in,out] registration Initialized registration object
 * @param[in] site_create_fn Site create function
 * @param[in] fn_cb_data Function-specific callback data
 *
 * @returns Status code:
 *  - IB_OK
 *  - IB_EALLOC for allocation errors
 */
ib_status_t ib_ctxsel_registration_store_site_create(
    ib_ctxsel_registration_t *registration,
    ib_ctxsel_site_create_fn_t site_create_fn,
    void *fn_cb_data);

/**
 * Store a location create function with a context selection object
 *
 * @param[in,out] registration Initialized registration object
 * @param[in] location_create_fn Location create function
 * @param[in] fn_cb_data Function-specific callback data
 *
 * @returns Status code:
 *  - IB_OK
 *  - IB_EALLOC for allocation errors
 */
ib_status_t ib_ctxsel_registration_store_location_create(
    ib_ctxsel_registration_t *registration,
    ib_ctxsel_location_create_fn_t location_create_fn,
    void *fn_cb_data);

/**
 * Store a host create function with a context selection object
 *
 * @param[in,out] registration Initialized registration object
 * @param[in] host_create_fn Host create function
 * @param[in] fn_cb_data Function-specific callback data
 *
 * @returns Status code:
 *  - IB_OK
 *  - IB_EALLOC for allocation errors
 */
ib_status_t ib_ctxsel_registration_store_host_create(
    ib_ctxsel_registration_t *registration,
    ib_ctxsel_host_create_fn_t host_create_fn,
    void *fn_cb_data);

/**
 * Store a service create function with a context selection object
 *
 * @param[in,out] registration Initialized registration object
 * @param[in] service_create_fn Location open function
 * @param[in] fn_cb_data Function-specific callback data
 *
 * @returns Status code:
 *  - IB_OK
 *  - IB_EALLOC for allocation errors
 */
ib_status_t ib_ctxsel_registration_store_service_create(
    ib_ctxsel_registration_t *registration,
    ib_ctxsel_service_create_fn_t service_create_fn,
    void *fn_cb_data);

/**
 * Store a site open function with a context selection object
 *
 * @param[in,out] registration Initialized registration object
 * @param[in] site_open_fn Site open function
 * @param[in] fn_cb_data Function-specific callback data
 *
 * @returns Status code:
 *  - IB_OK
 *  - IB_EALLOC for allocation errors
 */
ib_status_t ib_ctxsel_registration_store_site_open(
    ib_ctxsel_registration_t *registration,
    ib_ctxsel_site_open_fn_t site_open_fn,
    void *fn_cb_data);

/**
 * Store a location open function with a context selection object
 *
 * @param[in,out] registration Initialized registration object
 * @param[in] location_open_fn Location open function
 * @param[in] fn_cb_data Function-specific callback data
 *
 * @returns Status code:
 *  - IB_OK
 *  - IB_EALLOC for allocation errors
 */
ib_status_t ib_ctxsel_registration_store_location_open(
    ib_ctxsel_registration_t *registration,
    ib_ctxsel_location_open_fn_t location_open_fn,
    void *fn_cb_data);

/**
 * Store a site close function with a context selection object
 *
 * @param[in,out] registration Initialized registration object
 * @param[in] site_close_fn Site close function
 * @param[in] fn_cb_data Function-specific callback data
 *
 * @returns Status code:
 *  - IB_OK
 *  - IB_EALLOC for allocation errors
 */
ib_status_t ib_ctxsel_registration_store_site_close(
    ib_ctxsel_registration_t *registration,
    ib_ctxsel_site_close_fn_t site_close_fn,
    void *fn_cb_data);

/**
 * Store a location close function with a context selection object
 *
 * @param[in,out] registration Initialized registration object
 * @param[in] location_close_fn Location close function
 * @param[in] fn_cb_data Function-specific callback data
 *
 * @returns Status code:
 *  - IB_OK
 *  - IB_EALLOC for allocation errors
 */
ib_status_t ib_ctxsel_registration_store_location_close(
    ib_ctxsel_registration_t *registration,
    ib_ctxsel_location_close_fn_t location_close_fn,
    void *fn_cb_data);

/**
 * Store a finalize function with a context selection object
 *
 * @param[in,out] registration Initialized registration object
 * @param[in] finalize_fn Finalize function
 * @param[in] fn_cb_data Function-specific callback data
 *
 * @returns Status code:
 *  - IB_OK
 *  - IB_EALLOC for allocation errors
 */
ib_status_t ib_ctxsel_registration_store_finalize(
    ib_ctxsel_registration_t *registration,
    ib_ctxsel_finalize_fn_t finalize_fn,
    void *fn_cb_data);

/**
 * Register a context selection function family
 *
 * A family of site selection functions are registered as a group to implement
 * a site selection algorithm.  Some of these functions are required, some are
 * optional.
 *
 * The registration code keeps track of the "active" module; only one module
 * is allowed to be active at any time.
 *
 * The core module is treated specially.  If there is no other active module,
 * the core module is treated as the active module.
 *
 * The following are functions are required to be valid:
 *   select_fn, site_get_fn, location_get_fn
 *
 * @param[in,out] ib The IronBee Engine to register to.
 * @param[in] registration Initialized registration object
 *
 * @returns Status code
 *  - IB_OK on success
 *  - IB_DECLINED if a module attempts to register while another is active.
 */
ib_status_t DLL_PUBLIC ib_ctxsel_registration_register(
    ib_engine_t                    *ib,
    const ib_ctxsel_registration_t *registration
);

/**
 * Determine if the specified module is active
 *
 * @param[in] ib IronBee engine
 * @param[in] module Module to check
 *
 * @returns true if the specified module is active, otherwise false
 */
bool DLL_PUBLIC ib_ctxsel_module_is_active(
    const ib_engine_t *ib,
    const ib_module_t *module
);


/**
 * Perform configuration context selection
 *
 * This function is used to perform the actual context selection.
 * The active @sa ib_ctxsel_select_fn_t function will be
 * invoked to perform the selection.
 *
 * @param[in] ib Engine
 * @param[in] conn Connection to select a context for
 * @param[in] tx Transaction to select a context for (or NULL)
 * @param[out] pctx Pointer to selected context
 *
 * @returns Status code
 */
ib_status_t ib_ctxsel_select_context(
    const ib_engine_t *ib,
    const ib_conn_t *conn,
    const ib_tx_t *tx,
    ib_context_t **pctx);

/**
 * Create a site and add it to the context's site list
 *
 * This function is normally invoked by the core module in the processing of a
 * site directive.
 *
 * @param[in,out] ctx Site's configuration context
 * @param[in] name Site name
 * @param[out] psite Address where site will be written / NULL
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_ctxsel_site_create(
    ib_context_t *ctx,
    const char *name,
    ib_site_t **psite);

/**
 * Create a site location object.
 *
 * This function is normally invoked by the core module in the processing of a
 * location directive.
 *
 * @param[in] site Parent site
 * @param[in,out] ctx Locations' configuration context
 * @param[in] location_str Location string (path)
 * @param[out] plocation Address where location will be written / NULL
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_ctxsel_location_create(
    const ib_site_t *site,
    ib_context_t *ctx,
    const char *location_str,
    ib_site_location_t **plocation);

/**
 * Create a host object.
 *
 * This function is normally invoked by the core module in the processing of a
 * host directive.
 *
 * @param[in] site Parent site
 * @param[in] host_str Host string (host name)
 * @param[out] phost Pointer to new host object / NULL
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_ctxsel_host_create(
    const ib_site_t *site,
    const char *host_str,
    ib_site_host_t **phost);

/**
 * Create a service object
 *
 * @param[in] site Parent site
 * @param[in] service_str Service string to add
 * @param[out] pservice Address where location will be written / NULL
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_ctxsel_service_create(
    const ib_site_t *site,
    const char *service_str,
    ib_site_service_t **pservice);

/**
 * Context selection: Open a site (during site creation)
 *
 * This function is normally invoked by the core module in the processing of a
 * site directive.
 *
 * @param[in] ib Engine
 * @param[in] site Site object to open
 *
 * @returns Status code
 */
ib_status_t ib_ctxsel_site_open(
    const ib_engine_t *ib,
    ib_site_t *site);

/**
 * Context selection: Open a site (during site creation)
 *
 * This function is normally invoked by the core module in the processing of a
 * location directive.
 *
 * @param[in] ib Engine
 * @param[in] location Location object to open
 *
 * @returns Status code
 */
ib_status_t ib_ctxsel_location_open(
    const ib_engine_t *ib,
    ib_site_location_t *location);

/**
 * Context selection: Close a site (during site creation)
 *
 * This function is normally invoked by the core module in the processing of a
 * site end directive.
 *
 * @param[in] ib Engine
 * @param[in] site Site object to close
 *
 * @returns Status code
 */
ib_status_t ib_ctxsel_site_close(
    const ib_engine_t *ib,
    ib_site_t *site);

/**
 * Context selection: Close a site (during site creation)
 *
 * This function is normally invoked by the core module in the processing of a
 * location end directive.
 *
 * @param[in] ib Engine
 * @param[in] location Location object to close
 *
 * @returns Status code
 */
ib_status_t ib_ctxsel_location_close(
    const ib_engine_t *ib,
    ib_site_location_t *location);

/**
 * Context selection: Finalize
 *
 * This function is normally invoked by the core module upon closing of the
 * main context.
 *
 * @param[in] ib Engine
 *
 * @returns Status code
 */
ib_status_t ib_ctxsel_finalize(
    const ib_engine_t *ib);


/** @} */

#ifdef __cplusplus
}
#endif

#endif /* _IB_CONTEXT_SELECTION_H_ */
