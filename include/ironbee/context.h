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

#ifndef _IB_CONTEXT_H_
#define _IB_CONTEXT_H_

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
#include <ironbee/engine_types.h>
#include <ironbee/hash.h>

#ifdef __cplusplus
extern "C" {
#endif

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
 * Return the most specific context available given an engine, tx, and conn.
 *
 * - If @a tx has a @ref ib_context_t value set, then that is returned.
 * - If not, then @a conn is checked for a @ref ib_context_t. 
 * - Finally, the main context is fetched from @a ib, and returned.
 *
 * @param[in] ib The IronBee engine. This must be given.
 * @param[in] conn The connection. This may be NULL, in which case it is not
 *            considered in finding a @a ib_context_t.
 * @param[in] tx Transaction to check for the context. This may be NULL, in
 *            which case it will not be used to find an @a ib_context_t.
 *
 * @returns The most precise @ref ib_context_t available, give the arguments.
 */
ib_context_t *ib_context_get_context(
  ib_engine_t *ib,
  ib_conn_t   *conn,
  ib_tx_t     *tx
) NONNULL_ATTRIBUTE(1);

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
 * Get the IronBee memory pool.
 *
 * This returns a pointer to the IronBee memory pool associated with
 * the given context.
 *
 * @param ctx Config context
 *
 * @returns Pointer to the memory pool.
 */
ib_mpool_t DLL_PUBLIC *ib_context_get_mpool(const ib_context_t *ctx);

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
ib_status_t DLL_PUBLIC ib_context_module_config(const ib_context_t *ctx,
                                                const ib_module_t *m,
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
 * @} IronBeeEngine
 * @} IronBee
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_CONTEXT_H_ */
