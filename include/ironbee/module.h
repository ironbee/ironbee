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
#include <ironbee/module_sym.h>

#ifndef _IB_MODULE_H_
#define _IB_MODULE_H_

/**
 * @file
 * @brief IronBee --- Module
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbee/action.h>
#include <ironbee/build.h>
#include <ironbee/config.h>
#include <ironbee/operator.h>
#include <ironbee/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeModule Module
 * @ingroup IronBee
 *
 * Modules add functionality to IronBee.
 *
 * @{
 */

/**
 * Initialize values for dynamic modules created with ib_module_create().
 *
 * @param[in] m                   Module
 * @param[in] xfilename           Module code filename
 * @param[in] xdata               Module data
 * @param[in] xib                 Engine handle
 * @param[in] xname               Module name
 * @param[in] xgcdata             Global config data
 * @param[in] xgclen              Global config data length
 * @param[in] xfn_cfg_copy        Config copy function
 * @param[in] xcbdata_cfg_copy    Config copy function callback data
 * @param[in] xcm_init            Configuration field map
 * @param[in] xdm_init            Config directive map
 * @param[in] xfn_init            Initialize function
 * @param[in] xcbdata_init        Initialize function callback data
 * @param[in] xfn_fini            Finish function
 * @param[in] xcbdata_fini        Finish function callback data
 */
#define IB_MODULE_INIT_DYNAMIC(m, xfilename, xdata, xib, xname, xgcdata, xgclen, xfn_cfg_copy, xcbdata_cfg_copy, xcm_init, xdm_init, xfn_init, xcbdata_init, xfn_fini, xcbdata_fini) \
    do { \
        (m)->vernum             = IB_VERNUM; \
        (m)->abinum             = IB_ABINUM; \
        (m)->version            = IB_VERSION; \
        (m)->filename           = xfilename; \
        (m)->data               = xdata; \
        (m)->ib                 = xib; \
        (m)->idx                = 0; \
        (m)->name               = xname; \
        (m)->gcdata             = xgcdata; \
        (m)->gclen              = xgclen; \
        (m)->fn_cfg_copy        = xfn_cfg_copy; \
        (m)->cbdata_cfg_copy    = xcbdata_cfg_copy; \
        (m)->cm_init            = xcm_init; \
        (m)->dm_init            = xdm_init; \
        (m)->fn_init            = xfn_init; \
        (m)->cbdata_init        = xcbdata_init; \
        (m)->fn_fini            = xfn_fini; \
        (m)->cbdata_fini        = xcbdata_fini; \
    } while (0)

/** Defaults for all module structure headers */
#define IB_MODULE_HEADER_DEFAULTS     IB_VERNUM, \
                                      IB_ABINUM, \
                                      IB_VERSION, \
                                      __FILE__, \
                                      NULL, \
                                      NULL, \
                                      0

/** Module config structure, size, and default handlers */
#define IB_MODULE_CONFIG(ptr)         (ptr), (sizeof(*(ptr))), NULL, NULL

/** Used to signify that there is no config structure for the module. */
#define IB_MODULE_CONFIG_NULL         NULL, 0, NULL, NULL

/**
 * Function which is exported in an IronBee module to return the address
 * to the module structure used to load the module.
 *
 * This module function is declared by IB_MODULE_DECLARE and defined
 * by IB_MODULE_INIT. The address of this function is looked up by
 * name (IB_MODULE_SYM) when the module is loaded and called to fetch
 * the address of the module structure built with IB_MODULE_INIT.
 *
 * @param ib Engine handle
 * @returns Address of the module structure
 */
typedef const ib_module_t *(*ib_module_sym_fn)(ib_engine_t* ib);

/**
 * Function to handle copying configuration data.
 *
 * This is called when configuration data needs to be copied from a parent
 * context to a child context.  If NULL, it defaults to memcpy.
 *
 * @param[in] ib     Engine handle
 * @param[in] m      Module
 * @param[in] dst    Destination of data.
 * @param[in] src    Source of data.
 * @param[in] length Length of data.
 * @param[in] cbdata Callback data
 *
 * @returns Status code
 */
typedef ib_status_t (*ib_module_fn_cfg_copy_t)(
    ib_engine_t *ib,
    ib_module_t *m,
    void        *dst,
    const void  *src,
    size_t       length,
    void        *cbdata
);

/**
 * Function to initialize a module.
 *
 * This is called when the module is loaded.
 *
 * @param[in] ib     Engine handle
 * @param[in] m      Module
 * @param[in] cbdata Callback data
 *
 * @returns Status code
 */
typedef ib_status_t (*ib_module_fn_init_t)(
    ib_engine_t *ib,
    ib_module_t *m,
    void        *cbdata
);

/**
 * Function to finish a module.
 *
 * This is called when the module is unloaded.
 *
 * @param[in] ib     Engine handle
 * @param[in] m      Module
 * @param[in] cbdata Callback data
 *
 * @returns Status code
 */
typedef ib_status_t (*ib_module_fn_fini_t)(
    ib_engine_t *ib,
    ib_module_t *m,
    void        *cbdata
);

/**
 * Additional functionality for IronBee.
 *
 * A module provides additional functionality to IronBee.  It can register
 * configuration values and directives, hook into events, and provide
 * functions for other modules.
 */
struct ib_module_t {
    /* Header */
    uint32_t     vernum;      /**< Engine version number */
    uint32_t     abinum;      /**< Engine ABI Number */
    const char  *version;     /**< Engine version string */
    const char  *filename;    /**< Module code filename */
    void        *data;        /**< Module data */
    ib_engine_t *ib;          /**< Engine */
    size_t       idx;         /**< Module index */


    /* Module Config */
    const char *name; /**< Module name */

    void                    *gcdata;          /**< Global config data */
    size_t                   gclen;           /**< Global config data length */
    ib_module_fn_cfg_copy_t  fn_cfg_copy;     /**< Config copy handler */
    void                    *cbdata_cfg_copy; /**< Config copy data */
    const ib_cfgmap_init_t  *cm_init;         /**< Module config mapping */

    const ib_dirmap_init_t *dm_init; /**< Module directive mapping */

    /* Functions */
    ib_module_fn_init_t         fn_init;         /**< Module init */
    void                       *cbdata_init;     /**< fn_init callback data */
    ib_module_fn_fini_t         fn_fini;         /**< Module finish */
    void                       *cbdata_fini;     /**< fn_init callback data */
};

/**
 * Initialize an engine module.
 *
 * Use this to initialize a static module.
 *
 * @param[in] m  Module handle (already loaded)
 * @param[in] ib Engine handle
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_module_init(
    ib_module_t *m,
    ib_engine_t *ib
);

/**
 * Create a module structure.
 *
 * Use this to dynamically build modules.
 *
 * @param[out] pm Address which module structure will be written
 * @param[in]  ib Engine handle
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_module_create(
    ib_module_t **pm,
    ib_engine_t  *ib
);

/**
 * Load and initialize an engine module.
 *
 * This causes the module init() function to be called.
 *
 * @param[out] pm   Address to which module handle is written
 * @param[in]  ib   Engine handle
 * @param[in]  file Filename of the module
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_module_load(
    ib_module_t **pm,
    ib_engine_t  *ib,
    const char   *file
);

/**
 * Load a module DSO but do not initialize; instead return symbol.
 *
 * @param[out] psym Address to which module symbol is written
 * @param[in]  ib   Engine handle
 * @param[in]  file Filename of the module
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_module_file_to_sym(
    ib_module_sym_fn *psym,
    ib_engine_t      *ib,
    const char       *file
);

/**
 * Initialize an engine module from a symbol.
 *
 * This causes the module init() function to be called.
 *
 * @param[out] pm  Address to which module handle is written
 * @param[in]  ib  Engine handle
 * @param[in]  sym Module symbol.
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_module_load_from_sym(
    ib_module_t      **pm,
    ib_engine_t       *ib,
    ib_module_sym_fn   sym
);

/**
 * Unload an engine module.
 *
 * @param[in] m Module
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_module_unload(
     ib_module_t *m
);


/**
 * Register a module with a configuration context.
 *
 * @param[in] m Module
 * @param[in] ctx Configuration context
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_module_register_context(
     ib_module_t  *m,
     ib_context_t *ctx
);

/**
 * Initialize module configuration for main context.
 *
 * This routine is an alternative to setting an initial structure in the
 * module declaration.  It allows modules to setup their initial configuration
 * data in their initialization functions.
 *
 * @param module Module to initialize configuration data for.
 * @param cfg Configuration data.
 * @param cfg_length Length of configuration data.
 *
 * @returns
 * - IB_OK on success.
 * - IB_EINVAL if @a m already has configuration data.
 */
ib_status_t DLL_PUBLIC ib_module_config_initialize(
    ib_module_t *module,
    void *cfg,
    size_t cfg_length);

/**
 * Duplicate a module structure to create an independent module
 * representation that may be added to a another @ref ib_engine_t.
 *
 * @param[out] module_dst The module structure to be created and copied.
 * @param[in] module_src An module that has not been added to an engine yet.
 *            This will be copied into module_final.
 * @param[in] engine_dst The engine that @a module_dst will eventually
 *            be added to.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC On an allocation error.
 * - Other on unexpected error.
 */
ib_status_t DLL_PUBLIC ib_module_dup(
    ib_module_t **module_dst,
    ib_module_t  *module_src,
    ib_engine_t  *engine_dst
);

/**
 * @} IronBeeModule
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_MODULE_H_ */
