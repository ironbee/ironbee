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

#ifndef _IB_MODULES_H_
#define _IB_MODULES_H_

/**
 * @file
 * @brief IronBee - Modules
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <stdint.h>

#include <ironbee/build.h>
#include <ironbee/release.h>

#include <ironbee/ironbee.h>
#include <ironbee/config.h>
#include <ironbee/util.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeModules Modules
 * @ingroup IronBee
 * @{
 */

/** Module structure symbol name. */
#define IB_MODULE_SYM                 ibmodule
#define IB_MODULE_SYM_NAME            IB_XSTRINGIFY(IB_MODULE_SYM)

/** Module declaration. */
#define IB_MODULE_DECLARE() \
    ib_module_t DLL_PUBLIC IB_MODULE_SYM

/** Module symbol initialization. */
#define IB_MODULE_INIT(...) \
    ib_module_t IB_MODULE_SYM = { \
        __VA_ARGS__ \
    }

/** Data associated with the module */
#define IB_MODULE_DATA \
    IB_MODULE_SYM.data

/** Module initialization. */
/// @todo Need a batter way
#define IB_MODULE_INIT_STATIC(name, ...) \
    ib_module_t real_##name = { \
        __VA_ARGS__ \
    }; \
    ib_module_t *name(void) { return &real_##name; } \
    typedef int ib_require_semicolon_hack_##name

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
 * @param xfn_ctx_init Context init function
 */
#define IB_MODULE_INIT_DYNAMIC(m,xfilename,xdata,xib,xname,xgcdata,xgclen,xcm_init,xdm_init,xfn_init,xfn_fini,xfn_ctx_init) \
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
        (m)->fn_ctx_init = xfn_ctx_init; \
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
#define IB_MODULE_CONFIG(ptr)         (ptr), \
                                      ((ptr!=NULL)?sizeof(*(ptr)):0)

/**
 * Function to initialize a module.
 *
 * This is called when the module is loaded.
 *
 * @param ib Engine handle
 *
 * @returns Status code
 */
typedef ib_status_t (*ib_module_fn_init_t)(ib_engine_t *ib);

/**
 * Function to finish a module.
 *
 * This is called when the module is unloaded.
 *
 * @param ib Engine handle
 *
 * @returns Status code
 */
typedef ib_status_t (*ib_module_fn_fini_t)(ib_engine_t *ib);

/**
 * Function to initialize a module configuration context.
 *
 * This is called when @ref ib_context_init() is called to initialize
 * a configuration context. This should be used to initialize
 * any per-config-context resources.
 *
 * @param ib Engine handle
 * @param ctx Config context
 *
 * @returns Status code
 */
typedef ib_status_t (*ib_module_fn_ctx_init_t)(ib_engine_t *ib,
                                               ib_context_t *ctx);

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
    const void             *gcdata;           /**< Global config data */
    size_t                  gclen;            /**< Global config data length */
    const ib_cfgmap_init_t *cm_init;          /**< Module config mapping */
    const ib_dirmap_init_t *dm_init;          /**< Module directive mapping */

    /* Functions */
    ib_module_fn_init_t     fn_init;          /**< Module init */
    ib_module_fn_fini_t     fn_fini;          /**< Module finish */
    ib_module_fn_ctx_init_t fn_ctx_init;      /**< Module context init */
};

#define CORE_MODULE_NAME         core
#define CORE_MODULE_NAME_STR     IB_XSTRINGIFY(CORE_MODULE_NAME)

/* Static module declarations */
ib_module_t *ib_core_module(void);

/**
 * Core configuration.
 */
typedef struct ib_core_cfg_t ib_core_cfg_t;
struct ib_core_cfg_t {
    uint64_t      log_level;  /**< Log level */
    char         *log_uri;    /**< Log URI */
    char         *logger;     /**< Active logger name */
    char         *parser;     /**< Active parser name */
    char         *data;       /**< Active data provider name */
};

/**
 * Inititialize an engine module.
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
 * Load and intitialize an engine module.
 *
 * This causes the module init() function to be called.
 *
 * @param ib Engine handle
 * @param name Module name defined by the module
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
 * @param ib Engine handle
 * @param name Module name defined by the module
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_module_unload(ib_module_t *m);


/**
 * Register a module with a configuration context.
 *
 * @param ib Engine handle
 * @param name Module name defined by the module
 * @param ctx Configuration context
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_module_register_context(ib_module_t *m,
                                                  ib_context_t *ctx);


/**
 * @} IronBeeModules
 */

#ifdef __cplusplus
}
#endif

#endif /* IB_MODULES_H_ */
