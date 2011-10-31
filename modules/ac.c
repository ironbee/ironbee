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

/**
 * @file
 * @brief IronBee - AhoCorasick Matcher Module
 *
 * This module adds an AhoCorasick based matcher named "ac".
 *
 * @author Pablo Rincon <pablo.rincon.crespo@gmail.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <ctype.h>

#include <ironbee/engine.h>
#include <ironbee/util.h>
#include <ironbee/module.h>
#include <ironbee/provider.h>


/* Define the module name as well as a string version of it. */
#define MODULE_NAME        ac
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

/* Informational extra data.. version of this module (should be better to
 * register it with the module itself) */
#define AC_MAJOR           0
#define AC_MINOR           1
#define AC_DATE            20110812

typedef struct modac_cfg_t modac_cfg_t;
typedef struct modac_cpatt_t modac_cpatt_t;

/* Define the public module symbol. */
IB_MODULE_DECLARE();

/**
 * @internal
 * Module Configuration Structure.
 */
struct modac_cfg_t {
/* @todo: implement limits on ahocorasick to support this options:
    match_limit and match_limit_recursion */
    ib_num_t       match_limit;           /**< Match limit */
    ib_num_t       match_limit_recursion; /**< Match recursion depth limit */
};

/* Instantiate a module global configuration. */
static modac_cfg_t modac_global_cfg;

/**
 * @internal
 * Internal representation of AC compiled patterns.
 */
struct modac_provider_data_t {
    ib_ac_t *ac_tree;                 /**< The AC tree */
};

/* Instantiate a module global configuration. */
typedef struct modac_provider_data_t modac_provider_data_t;


/* -- Matcher Interface -- */

/**
 * Add a pattern to the patterns of the matcher given a pattern and 
 * callback + extra arg
 *
 * @param mpr matcher provider
 * @param patterns pointer to the pattern container (ie: an AC tree)
 * @param patt the pattern to be added
 * @param callback the callback to register with the given pattern
 * @param arg the extra argument to pass to the callback
 * @param errptr a pointer reference to point where an error ocur
 * @param erroffset a pointer holding the offset of the error
 * 
 * @return status of the operation
 */
static ib_status_t modac_add_pattern_ex(ib_provider_inst_t *mpi,
                                        void *patterns,
                                        const char *patt,
                                        ib_void_fn_t callback,
                                        void *arg,
                                        const char **errptr,
                                        int *erroffset)
{
    IB_FTRACE_INIT(modac_add_pattern_ex);
    ib_status_t rc;
    ib_ac_t *ac_tree = (ib_ac_t *)((modac_provider_data_t*)mpi->data)->ac_tree;

    /* If the ac_tree doesn't exist, create it before adding the pattern */
    if (ac_tree == NULL) {
        rc = ib_ac_create(&ac_tree, 0, mpi->mp);
        if (rc != IB_OK || ac_tree == NULL) {
            ib_log_error(mpi->pr->ib, 4,
                         "Unable to create the AC tree at modac");
            IB_FTRACE_RET_STATUS(rc);
        }
        ((modac_provider_data_t*)mpi->data)->ac_tree = ac_tree;
    }

    rc = ib_ac_add_pattern(ac_tree, patt, (ib_ac_callback_t)callback, arg, 0);

    if (rc == IB_OK) {
        ib_log_debug(mpi->pr->ib, 4, "pattern %s added to the AC tree %x", patt,
                     ac_tree);
    }
    else {
        ib_log_error(mpi->pr->ib, 4, "Failed to load pattern %s to the AC tree %x",
                     patt, ac_tree);
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Initialize a provider instance with the given data
 *
 * @param mpi provider instance
 * @param extra data
 *
 * @return status of the operation
 */
static ib_status_t modac_provider_instance_init(ib_provider_inst_t *mpi,
                                                void *data)
{
    IB_FTRACE_INIT(modac_provider_instance_init);
    ib_status_t rc;
    modac_provider_data_t *dt;

    dt = (modac_provider_data_t *) ib_mpool_calloc(mpi->mp, 1,
                                         sizeof(modac_provider_data_t));
    if (dt == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    mpi->data = (void *)dt;
    rc = ib_ac_create(&dt->ac_tree, 0, mpi->mp);

    if (rc != IB_OK) {
        ib_log_error(mpi->pr->ib, 4, "Unable to create the AC tree at modac");
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Match against the AC tree
 *
 * @param mpi provider instance
 * @param flags extra flags
 * @param data the data to search in
 * @param dlen length of the the data to search in
 *
 * @return status of the operation
 */
static ib_status_t modac_match(ib_provider_inst_t *mpi,
                                 ib_flags_t flags,
                                 const uint8_t *data,
                                 size_t dlen, void *ctx)
{
    IB_FTRACE_INIT(modac_match);
    modac_provider_data_t *dt = mpi->data;

    if (dt == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    ib_log_debug(mpi->pr->ib, 4, "Matching AGAINST AC tree %x",
                     dt->ac_tree);


    ib_ac_t *ac_tree = dt->ac_tree;

    ib_ac_context_t *ac_mctx = (ib_ac_context_t *)ctx;

    ib_ac_reset_ctx(ac_mctx, ac_tree);

    /* Let's perform the search. Content is consumed in just one call */
    ib_status_t rc = ib_ac_consume(ac_mctx,
                                   (const char *)data,
                                   dlen,
                                   IB_AC_FLAG_CONSUME_DOLIST |
                                   IB_AC_FLAG_CONSUME_MATCHALL |
                                   IB_AC_FLAG_CONSUME_DOCALLBACK,
                                   mpi->mp);

    IB_FTRACE_RET_STATUS(rc);
}

static ib_status_t modac_compile(ib_provider_t *mpr,
                                   ib_mpool_t *pool,
                                   void *pcpatt,
                                   const char *patt,
                                   const char **errptr,
                                   int *erroffset)
{
    IB_FTRACE_INIT(modac_compile);
    IB_FTRACE_RET_STATUS(IB_ENOTIMPL);
}

static ib_status_t modac_match_compiled(ib_provider_t *mpr,
                                          void *cpatt,
                                          ib_flags_t flags,
                                          const uint8_t *data,
                                          size_t dlen, void *ctx)
{
    IB_FTRACE_INIT(modac_match_compiled);
    IB_FTRACE_RET_STATUS(IB_ENOTIMPL);
}

static ib_status_t modac_add_pattern(ib_provider_inst_t *pi,
                                       void *cpatt)
{
    IB_FTRACE_INIT(modac_add);
    IB_FTRACE_RET_STATUS(IB_ENOTIMPL);
}

static IB_PROVIDER_IFACE_TYPE(matcher) modac_matcher_iface = {
    IB_PROVIDER_IFACE_HEADER_DEFAULTS,

    /* Provider Interface */
    modac_compile,
    modac_match_compiled,

    /* Provider Instance Interface */
    modac_add_pattern,
    modac_add_pattern_ex,
    modac_match
};


/* -- Module Routines -- */

static ib_status_t modac_init(ib_engine_t *ib,
                                ib_module_t *m)
{
    IB_FTRACE_INIT(modac_init);
    ib_status_t rc;

    /* Register as a matcher provider. */
    rc = ib_provider_register(ib,
                              IB_PROVIDER_TYPE_MATCHER,
                              MODULE_NAME_STR,
                              NULL,
                              &modac_matcher_iface,
                              modac_provider_instance_init);
    if (rc != IB_OK) {
        ib_log_error(ib, 3,
                     MODULE_NAME_STR ": Error registering ac matcher provider: "
                     "%d", rc);
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    ib_log_debug(ib, 4, "AC Status: compiled=\"%d.%d %s\" AC Matcher"
                        " registered", AC_MAJOR, AC_MINOR, IB_XSTRINGIFY(AC_DATE));

    IB_FTRACE_RET_STATUS(IB_OK);
}

static IB_CFGMAP_INIT_STRUCTURE(modac_config_map) = {
    IB_CFGMAP_INIT_ENTRY(
        MODULE_NAME_STR ".match_limit",
        IB_FTYPE_NUM,
        &modac_global_cfg,
        match_limit,
        5000
    ),
    IB_CFGMAP_INIT_ENTRY(
        MODULE_NAME_STR ".match_limit_recursion",
        IB_FTYPE_NUM,
        &modac_global_cfg,
        match_limit_recursion,
        5000
    ),
    IB_CFGMAP_INIT_LAST
};

/**
 * @internal
 * Module structure.
 *
 * This structure defines some metadata, config data and various functions.
 */
IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,            /**< Default metadata */
    MODULE_NAME_STR,                      /**< Module name */
    IB_MODULE_CONFIG(&modac_global_cfg),  /**< Global config data */
    modac_config_map,                     /**< Configuration field map */
    NULL,                                 /**< Config directive map */
    modac_init,                           /**< Initialize function */
    NULL,                                 /**< Finish function */
    NULL,                                 /**< Context init function */
    NULL                                  /**< Context fini function */
);

