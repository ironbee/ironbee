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

#ifndef _IB_CONFIG_H_
#define _IB_CONFIG_H_

/**
 * @file
 * @brief IronBee - Configuration Functions
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include <sys/types.h>
#include <unistd.h>

#include <ironbee/build.h>
#include <ironbee/release.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeConfig Configuration
 * @ingroup IronBee
 * @{
 */

typedef struct ib_cfgparser_t ib_cfgparser_t;
typedef struct ib_dirmap_init_t ib_dirmap_init_t;
typedef struct ib_site_t ib_site_t;
typedef struct ib_loc_t ib_loc_t;
typedef struct ib_strval_t ib_strval_t;

/// @todo Should probably be private structure
struct ib_cfgparser_t {
    ib_engine_t            *ib;          /**< Engine */
    ib_mpool_t             *mp;          /**< Memory pool */
    ib_list_t              *stack;       /**< Stack tracking contexts */
    ib_list_t              *block;       /**< Stack tracking blocks */

    /* Parsing states */
    ib_context_t           *cur_ctx;     /**< Current context */
    ib_site_t              *cur_site;    /**< Current site */
    ib_loc_t               *cur_loc;     /**< Current location */
    const char             *cur_blkname; /**< Current block name */
};

/**
 * Directive types.
 */
typedef enum {
    IB_DIRTYPE_ONOFF,                    /**< Boolean param directive */
    IB_DIRTYPE_PARAM1,                   /**< One param directive */
    IB_DIRTYPE_PARAM2,                   /**< Two param directive */
    IB_DIRTYPE_LIST,                     /**< List param directive */
    IB_DIRTYPE_OPFLAGS,                  /**< Option flags directive */
    IB_DIRTYPE_SBLK1,                    /**< One param subblock directive */
} ib_dirtype_t;

/** String key/numeric value pair */
struct ib_strval_t {
    const char             *str;         /**< String "key" */
    ib_num_t                val;         /**< Numeric value */
};

#define IB_STRVAL_MAP(name) ib_strval_t name[]
#define IB_STRVAL_PAIR(str,val) { str, val }
#define IB_STRVAL_PAIR_LAST { NULL, 0 }

/** Callback for ending (processing) a block */
typedef ib_status_t (*ib_config_cb_blkend_fn_t)(ib_cfgparser_t *cp,
                                                const char *name,
                                                void *cbdata);

/** Callback for ONOFF directives */
typedef ib_status_t (*ib_config_cb_onoff_fn_t)(ib_cfgparser_t *cp,
                                               const char *name,
                                               int onoff,
                                               void *cbdata);

/** Callback for PARAM1 directives */
typedef ib_status_t (*ib_config_cb_param1_fn_t)(ib_cfgparser_t *cp,
                                                const char *name,
                                                const char *p1,
                                                void *cbdata);

/** Callback for PARAM2 directives */
typedef ib_status_t (*ib_config_cb_param2_fn_t)(ib_cfgparser_t *cp,
                                                const char *name,
                                                const char *p1,
                                                const char *p2,
                                                void * cbdata);

/** Callback for LIST directives */
typedef ib_status_t (*ib_config_cb_list_fn_t)(ib_cfgparser_t *cp,
                                              const char *name,
                                              const ib_list_t *list,
                                              void *cbdata);

/** Callback for OPFLAGS directives */
typedef ib_status_t (*ib_config_cb_opflags_fn_t)(ib_cfgparser_t *cp,
                                                 const char *name,
                                                 ib_flags_t val,
                                                 ib_flags_t mask,
                                                 void *cbdata);

/** Callback for SBLK1 directives */
typedef ib_status_t (*ib_config_cb_sblk1_fn_t)(ib_cfgparser_t *cp,
                                               const char *name,
                                               const char *p1,
                                               void *cbdata);

/**
 * Directive initialization mapping structure.
 */
struct ib_dirmap_init_t {
    const char                   *name;      /**< Directive name */
    ib_dirtype_t                  type;      /**< Directive type */
    union {
        /**
         * @internal
         * C90/C++ always initializes the first member vs C99 which has
         * designated initializers, so in order to support initialization
         * there must be a generic function as the first member.
         */
        ib_void_fn_t              _init;
        ib_config_cb_onoff_fn_t   fn_onoff;  /**< On|Off directive */
        ib_config_cb_param1_fn_t  fn_param1; /**< 1 param directive */
        ib_config_cb_param2_fn_t  fn_param2; /**< 2 param directive */
        ib_config_cb_list_fn_t    fn_list;   /**< List directive */
        ib_config_cb_opflags_fn_t  fn_opflags;/**< Option flags directive */
        ib_config_cb_sblk1_fn_t   fn_sblk1;  /**< 1 param subblock directive */
    } cb;
    ib_config_cb_blkend_fn_t      fn_blkend; /**< Called when block ends */
    void                         *cbdata;    /**< Arbitrary callback data */
    ib_strval_t                  *valmap;    /**< Value map */
    /// @todo Do we need help text or error messages???
};

/// @todo Use designated initializers if supported so that better type
///       checking is performed.

/**
 * Defines a configuration directive map initialization structure.
 *
 * @param name Name of structure
 */
#define IB_DIRMAP_INIT_STRUCTURE(name) const ib_dirmap_init_t name[]


/** Directive with a single On/Off/True/False/Yes/No parameter */
#define IB_DIRMAP_INIT_ONOFF(name,cb,cbdata) \
    { (name), IB_DIRTYPE_ONOFF, { (ib_void_fn_t)(cb) }, NULL, (cbdata), NULL }

/** Directive with a single string parameter */
#define IB_DIRMAP_INIT_PARAM1(name,cb,cbdata) \
    { (name), IB_DIRTYPE_PARAM1, { (ib_void_fn_t)(cb) }, NULL, (cbdata), NULL }

/** Directive with two string parameters */
#define IB_DIRMAP_INIT_PARAM2(name,cb,cbdata) \
    { (name), IB_DIRTYPE_PARAM2, { (ib_void_fn_t)(cb) }, NULL, (cbdata), NULL }

/** Directive with list of string parameters */
#define IB_DIRMAP_INIT_LIST(name,cb,cbdata) \
    { (name), IB_DIRTYPE_LIST, { (ib_void_fn_t)(cb) }, NULL, (cbdata), NULL }

/** Directive with list of unique options string parameters */
#define IB_DIRMAP_INIT_OPFLAGS(name,cb,cbdata,valmap) \
    { (name), IB_DIRTYPE_OPFLAGS, { (ib_void_fn_t)(cb) }, NULL, (cbdata), (valmap) }

/** Block with single parameter enclosing more directives */
#define IB_DIRMAP_INIT_SBLK1(name,cb,blkend,cbdata) \
    { (name), IB_DIRTYPE_SBLK1, { (ib_void_fn_t)(cb) }, (blkend), (cbdata), NULL }

/** Required last entry. */
#define IB_DIRMAP_INIT_LAST { NULL }


/**
 * Create a new configuration parser.
 *
 * @param pcp Address where config parser handle will be written
 * @param ib Engine
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_cfgparser_create(ib_cfgparser_t **pcp,
                                           ib_engine_t *ib);

/// @todo Create a ib_cfgparser_parse_ex that can parse non-files (DBs, etc)

/**
 * Parse a configuration file.
 *
 * @param cp Parser
 * @param file Filename
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_cfgparser_parse(ib_cfgparser_t *cp,
                                          const char *file);

/**
 * Push a new context onto the stack and make it the current.
 *
 * @param cp Parser
 * @param ctx New context
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_cfgparser_context_push(ib_cfgparser_t *cp,
                                                 ib_context_t *ctx);

/**
 * Pop the current context off the stack and make the previous the current.
 *
 * @param cp Parser
 * @param pctx Address which the removed context will be written (if non-NULL)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_cfgparser_context_pop(ib_cfgparser_t *cp,
                                                ib_context_t **pctx);

/**
 * Push a new block name onto the stack and make it the current.
 *
 * @param cp Parser
 * @param name New block name
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_cfgparser_block_push(ib_cfgparser_t *cp,
                                               const char *name);

/**
 * Pop the current block name off the stack and make the previous the current.
 *
 * @param cp Parser
 * @param pname Address which the removed name will be written (if non-NULL)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_cfgparser_block_pop(ib_cfgparser_t *cp,
                                              const char **pname);

/**
 * Destroy the parser.
 *
 * @param cp Parser
 */
void DLL_PUBLIC ib_cfgparser_destroy(ib_cfgparser_t *cp);


/**
 * Register directives with the engine.
 *
 * @param ib Engine
 * @param init Directive mapping
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_config_register_directives(ib_engine_t *ib,
                                                     const ib_dirmap_init_t *init);

/**
 * Register a directive with the engine.
 *
 * Primarily this is meant for non-C languages that cannot easily build
 * the map structure for @ref ib_config_register_directives.
 *
 * @param ib Engine
 * @param name Directive name
 * @param type Directive type
 * @param fn_config Callback function handling the config
 * @param fn_blkend Callback function called at the end of a block (or NULL)
 * @param cbdata Data passed to the callback functions
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_config_register_directive(ib_engine_t *ib,
                                                    const char *name,
                                                    ib_dirtype_t type,
                                                    ib_void_fn_t fn_config,
                                                    ib_config_cb_blkend_fn_t fn_blkend,
                                                    void *cbdata);

/**
 * Process a directive.
 *
 * @param cp Config parser
 * @param name Directive name
 * @param args Directive arguments
 * @todo Need to pass back an error msg???
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_config_directive_process(ib_cfgparser_t *cp,
                                                   const char *name,
                                                   ib_list_t *args);

/**
 * Start a block.
 *
 * @param cp Config parser
 * @param name Block name
 * @param args Block arguments
 * @todo Need to pass back an error msg???
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_config_block_start(ib_cfgparser_t *cp,
                                             const char *name,
                                             ib_list_t *args);

/**
 * Process a block.
 *
 * This is called when the end of a block is reached. Any arguments
 * must be saved when @ref ib_config_block_start() is called when the
 * block started.
 *
 * @param cp Config parser
 * @param name Block name
 * @todo Need to pass back an error msg???
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_config_block_process(ib_cfgparser_t *cp,
                                               const char *name);

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
 * @} IronBeeConfig
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_CONFIG_H_ */
