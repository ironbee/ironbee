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

/* This is always re-included to allow for prefixing the symbol names. */
#include <ironbee/module_sym.h>

#ifndef _IB_MODULE_H_
#define _IB_MODULE_H_

/**
 * @file
 * @brief IronBee - Module
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <stdint.h>

#include <ironbee/build.h>
#include <ironbee/release.h>

#include <ironbee/engine.h>
#include <ironbee/config.h>
#include <ironbee/logformat.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeModule Module
 * @ingroup IronBee
 * @{
 */

/**
 * Initialize values for dynamic modules created with ib_module_create().
 *
 * @param m Module
 * @param xfilename Module code filename
 * @param xdata Module data
 * @param xib Engine handle
 * @param xname Module name
 * @param xgcdata Global config data
 * @param xgclen Global config data length
 * @param xcm_init Configuration field map
 * @param xdm_init Config directive map
 * @param xfn_init Initialize function
 * @param xfn_fini Finish function
 * @param xfn_ctx_open Context open function
 * @param xfn_ctx_close Context close function
 * @param xfn_ctx_destroy Context destroy function
 */
#define IB_MODULE_INIT_DYNAMIC(m,xfilename,xdata,xib,xname,xgcdata,xgclen,xcm_init,xdm_init,xfn_init,xfn_fini,xfn_ctx_open,xfn_ctx_close,xfn_ctx_destroy) \
    do { \
        (m)->vernum = IB_VERNUM; \
        (m)->abinum = IB_ABINUM; \
        (m)->version = IB_VERSION; \
        (m)->filename = xfilename; \
        (m)->data = xdata; \
        (m)->ib = xib; \
        (m)->idx = 0; \
        (m)->name = xname; \
        (m)->gcdata = xgcdata; \
        (m)->gclen = xgclen; \
        (m)->cm_init = xcm_init; \
        (m)->dm_init = xdm_init; \
        (m)->fn_init = xfn_init; \
        (m)->fn_fini = xfn_fini; \
        (m)->fn_ctx_open = xfn_ctx_open; \
        (m)->fn_ctx_close = xfn_ctx_close; \
        (m)->fn_ctx_destroy = xfn_ctx_destroy; \
    } while (0)

/** Defaults for all module structure headers */
#define IB_MODULE_HEADER_DEFAULTS     IB_VERNUM, \
                                      IB_ABINUM, \
                                      IB_VERSION, \
                                      __FILE__, \
                                      NULL, \
                                      NULL, \
                                      0

/** Module config structure and size */
#define IB_MODULE_CONFIG(ptr)         (ptr), (sizeof(*(ptr)))

/** Used to signify that there is no config structure for the module. */
#define IB_MODULE_CONFIG_NULL         NULL, 0

/**
 * Function which is exported in an IronBee module to return the address
 * to the module structure used to load the module.
 *
 * This module function is declared by @ref IB_MODULE_DECLARE and defined
 * by @ref IB_MODULE_INIT. The address of this function is looked up by
 * name (@ref IB_MODULE_SYM) when the module is loaded and called to fetch
 * the address of the module structure built with @ref IB_MODULE_INIT.
 *
 * @returns Address of the module structure
 */
typedef ib_module_t *(*ib_module_sym_fn)(void);

/**
 * Function to initialize a module.
 *
 * This is called when the module is loaded.
 *
 * @param ib Engine handle
 *
 * @returns Status code
 */
typedef ib_status_t (*ib_module_fn_init_t)(ib_engine_t *ib,
                                           ib_module_t *m);

/**
 * Function to finish a module.
 *
 * This is called when the module is unloaded.
 *
 * @param ib Engine handle
 *
 * @returns Status code
 */
typedef ib_status_t (*ib_module_fn_fini_t)(ib_engine_t *ib,
                                           ib_module_t *m);

/**
 * Function called when a context is opened.
 *
 * This is called when a context is opened in the configuration file.
 *
 * @param[in] ib Engine handle
 * @param[in] m Module.
 * @param[in] ctx Config context
 *
 * @returns Status code
 */
typedef ib_status_t (*ib_module_fn_ctx_open_t)(
     ib_engine_t  *ib,
     ib_module_t  *m,
     ib_context_t *ctx
);

/**
 * Function called when a context is closed.
 *
 * This is called when @ref ib_context_init() is called to initialize
 * a configuration context.  Which happens whenever a context is closed 
 * in the configuration file.  This should be used to initialize
 * any per-config-context resources.
 *
 * @param[in] ib Engine handle
 * @param[in] m Module.
 * @param[in] ctx Config context
 *
 * @returns Status code
 */
typedef ib_status_t (*ib_module_fn_ctx_close_t)(
     ib_engine_t  *ib,
     ib_module_t  *m,
     ib_context_t *ctx
);

/**
 * Function called when a context is destroyed.
 *
 * This is called when @ref ib_context_destroy() is called to finish
 * a configuration context. This should be used to destroy
 * any per-config-context resources.
 *
 * @param[in] ib Engine handle
 * @param[in] m Module.
 * @param[in] ctx Config context
 *
 * @returns Status code
 */
typedef ib_status_t (*ib_module_fn_ctx_destroy_t)(
     ib_engine_t  *ib,
     ib_module_t  *m,
     ib_context_t *ctx
);

struct ib_module_t {
    /* Header */
    uint32_t                vernum;           /**< Engine version number */
    uint32_t                abinum;           /**< Engine ABI Number */
    const char             *version;          /**< Engine version string */
    const char             *filename;         /**< Module code filename */
    void                   *data;             /**< Module data */
    ib_engine_t            *ib;               /**< Engine */
    size_t                  idx;              /**< Module index */


    /* Module Config */
    const char             *name;             /**< Module name */
    void                   *gcdata;           /**< Global config data */
    size_t                  gclen;            /**< Global config data length */
    const ib_cfgmap_init_t *cm_init;          /**< Module config mapping */
    const ib_dirmap_init_t *dm_init;          /**< Module directive mapping */

    /* Functions */
    ib_module_fn_init_t        fn_init;          /**< Module init */
    ib_module_fn_fini_t        fn_fini;          /**< Module finish */
    ib_module_fn_ctx_open_t    fn_ctx_open;      /**< Context open */
    ib_module_fn_ctx_close_t   fn_ctx_close;     /**< Context close */
    ib_module_fn_ctx_destroy_t fn_ctx_destroy;   /**< Context destroy */
};

/**
 * Initialize an engine module.
 *
 * Use this to initialize a static module.
 *
 * @param m Module handle (already loaded)
 * @param ib Engine handle
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_module_init(ib_module_t *m, ib_engine_t *ib);

/**
 * Create a module structure.
 *
 * Use this to dynamically build modules.
 *
 * @param pm Address which module structure will be written
 * @param ib Engine handle
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_module_create(ib_module_t **pm,
                                        ib_engine_t *ib);

/**
 * Load and initialize an engine module.
 *
 * This causes the module init() function to be called.
 *
 * @param pm Address which module handle is written
 * @param ib Engine handle
 * @param file Filename of the module
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_module_load(ib_module_t **pm,
                                      ib_engine_t *ib,
                                      const char *file);

/**
 * Unload an engine module.
 *
 * @param m Module
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_module_unload(ib_module_t *m);


/**
 * Register a module with a configuration context.
 *
 * @param m Module
 * @param ctx Configuration context
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_module_register_context(ib_module_t *m,
                                                  ib_context_t *ctx);


/**
 * @} IronBeeModule
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_MODULE_H_ */
