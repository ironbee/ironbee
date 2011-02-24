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
 * @brief IronBee - HTP Module
 *
 * This module adds a PCRE based matcher named "pcre".
 *
 * @author Brian Rectanus <brectanus@qualys.com>
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

#include <pcre.h>


/* Define the module name as well as a string version of it. */
#define MODULE_NAME        pcre
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

typedef struct modpcre_cfg_t modpcre_cfg_t;
typedef struct modpcre_cpatt_t modpcre_cpatt_t;

/* Define the public module symbol. */
IB_MODULE_DECLARE();

/**
 * @internal
 * Module Configuration Structure.
 */
struct modpcre_cfg_t {
    uint64_t       study;                 /**< Study compiled regexs */
    uint64_t       match_limit;           /**< Match limit */
    uint64_t       match_limit_recursion; /**< Match recursion depth limit */
};

/**
 * @internal
 * Internal representation of PCRE compiled patterns.
 */
struct modpcre_cpatt_t {
    void          *cpatt;                 /**< Compiled pattern */
    const char    *patt;                  /**< Regex pattern text */
};

/* Instantiate a module global configuration. */
static modpcre_cfg_t modpcre_global_cfg;


/* -- Matcher Interface -- */

static ib_status_t modpcre_compile(ib_provider_t *mpr,
                                   ib_mpool_t *pool,
                                   void *pcpatt,
                                   const char *patt,
                                   const char **errptr,
                                   int *erroffset)
{
    IB_FTRACE_INIT(modpcre_compile);
    void *cpatt;
    modpcre_cpatt_t *pcre_cpatt;

    cpatt = pcre_compile(patt,
                         PCRE_DOTALL | PCRE_DOLLAR_ENDONLY,
                         errptr, erroffset, NULL);

    if (cpatt == NULL) {
        *(void **)pcpatt = NULL;
        ib_util_log_error(4, "PCRE compile error for \"%s\": %s at offset %d", patt, *errptr, *erroffset);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    pcre_cpatt = (modpcre_cpatt_t *)ib_mpool_alloc(mpr->mp, sizeof(*pcre_cpatt));
    if (pcre_cpatt == NULL) {
        *(void **)pcpatt = NULL;
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    pcre_cpatt->patt = patt; /// @todo Copy
    pcre_cpatt->cpatt = cpatt;

    *(void **)pcpatt = (void *)pcre_cpatt;

    IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t modpcre_match_compiled(ib_provider_t *mpr,
                                          void *cpatt,
                                          ib_flags_t flags,
                                          const uint8_t *data,
                                          size_t dlen)
{
    IB_FTRACE_INIT(modpcre_match_compiled);
    modpcre_cpatt_t *pcre_cpatt = (modpcre_cpatt_t *)cpatt;
    int ovector[30];
    int ec;

    ec = pcre_exec(pcre_cpatt->cpatt, NULL,
                   (const char *)data, dlen,
                   0, 0, ovector, 30);
    if (ec >= 0) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    else if (ec == PCRE_ERROR_NOMATCH) {
        IB_FTRACE_RET_STATUS(IB_ENOENT);
    }

    IB_FTRACE_RET_STATUS(IB_EINVAL);
}

static ib_status_t modpcre_add_pattern(ib_provider_inst_t *pi,
                                       void *cpatt)
{
    IB_FTRACE_INIT(modpcre_add);
    IB_FTRACE_RET_STATUS(IB_ENOTIMPL);
}

static ib_status_t modpcre_match(ib_provider_inst_t *mpi,
                                 ib_flags_t flags,
                                 const uint8_t *data,
                                 size_t dlen)
{
    IB_FTRACE_INIT(modpcre_match);
    IB_FTRACE_RET_STATUS(IB_ENOTIMPL);
}

static IB_PROVIDER_IFACE_TYPE(matcher) modpcre_matcher_iface = {
    IB_PROVIDER_IFACE_HEADER_DEFAULTS,

    /* Provider Interface */
    modpcre_compile,
    modpcre_match_compiled,

    /* Provider Instance Interface */
    modpcre_add_pattern,
    modpcre_match
};


/* -- Module Routines -- */

static ib_status_t modpcre_init(ib_engine_t *ib)
{
    IB_FTRACE_INIT(modpcre_init);
    ib_status_t rc;

    /* Register as a matcher provider. */
    rc = ib_provider_register(ib,
                              IB_PROVIDER_TYPE_MATCHER,
                              MODULE_NAME_STR,
                              NULL,
                              &modpcre_matcher_iface,
                              NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, 3,
                     MODULE_NAME_STR ": Error registering pcre matcher provider: "
                     "%d", rc);
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

static IB_CFGMAP_INIT_STRUCTURE(modpcre_config_map) = {
    IB_CFGMAP_INIT_ENTRY(
        MODULE_NAME_STR ".study",
        IB_FTYPE_NUM,
        &modpcre_global_cfg,
        study,
        1
    ),
    IB_CFGMAP_INIT_ENTRY(
        MODULE_NAME_STR ".match_limit",
        IB_FTYPE_NUM,
        &modpcre_global_cfg,
        match_limit,
        5000
    ),
    IB_CFGMAP_INIT_ENTRY(
        MODULE_NAME_STR ".match_limit_recursion",
        IB_FTYPE_NUM,
        &modpcre_global_cfg,
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
    IB_MODULE_CONFIG(&modpcre_global_cfg),/**< Global config data */
    modpcre_config_map,                   /**< Configuration field map */
    NULL,                                 /**< Config directive map */
    modpcre_init,                         /**< Initialize function */
    NULL,                                 /**< Finish function */
    NULL,                                 /**< Context init function */
);

