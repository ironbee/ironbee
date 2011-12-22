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
 * @brief IronBee - Proof of Concept Signature Module
 *
 * This module serves as an example and proof of concept for
 * a signature language. The module is purposefully simplistic
 * so that it is easy to follow.
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <string.h>
#include <strings.h>

#include <ironbee/engine.h>
#include <ironbee/debug.h>
#include <ironbee/mpool.h>
#include <ironbee/cfgmap.h>
#include <ironbee/module.h>
#include <ironbee/provider.h>

/* Define the module name as well as a string version of it. */
#define MODULE_NAME               pocsig
#define MODULE_NAME_STR           IB_XSTRINGIFY(MODULE_NAME)

/* Declare the public module symbol. */
IB_MODULE_DECLARE();

typedef struct pocsig_cfg_t pocsig_cfg_t;
typedef struct pocsig_sig_t pocsig_sig_t;

/** Signature Phases */
typedef enum {
    POCSIG_PRE,                   /**< Pre transaction phase */
    POCSIG_REQHEAD,               /**< Request headers phase */
    POCSIG_REQ,                   /**< Request phase */
    POCSIG_RESHEAD,               /**< Response headers phase */
    POCSIG_RES,                   /**< Response phase */
    POCSIG_POST,                  /**< Post transaction phase */

    /* Keep track of the number of defined phases. */
    POCSIG_PHASE_NUM
} pocsig_phase_t;

/** Signature Structure */
struct pocsig_sig_t {
    const char         *target;   /**< Target name */
    const char         *patt;     /**< Pattern to match in target */
    void               *cpatt;    /**< Compiled PCRE regex */
    const char         *emsg;     /**< Event message */
};

/** Module Configuration Structure */
struct pocsig_cfg_t {
    /* Exposed as configuration parameters. */
    ib_num_t            trace;    /**< Log signature tracing */

    /* Private. */
    ib_list_t          *phase[POCSIG_PHASE_NUM]; /**< Phase signature lists */
    ib_matcher_t       *pcre;     /**< PCRE matcher */
};

/* Instantiate a module global configuration. */
static pocsig_cfg_t pocsig_global_cfg;


/* -- Directive Handlers -- */

/**
 * @internal
 * Handle a PocSigTrace directive.
 *
 * @param cp Config parser
 * @param name Directive name
 * @param p1 First parameter
 * @param cbdata Callback data (from directive registration)
 *
 * @returns Status code
 */
static ib_status_t pocsig_dir_trace(ib_cfgparser_t *cp,
                                    const char *name,
                                    const char *p1,
                                    void *cbdata)
{
    IB_FTRACE_INIT(pocsig_dir_trace);
    ib_engine_t *ib = cp->ib;
    ib_context_t *ctx = cp->cur_ctx ? cp->cur_ctx : ib_context_main(ib);
    ib_status_t rc;

    ib_log_debug(ib, 7, "%s: \"%s\" ctx=%p", name, p1, ctx);
    if (strcasecmp("On", p1) == 0) {
        rc = ib_context_set_num(ctx, MODULE_NAME_STR ".trace", 1);
        IB_FTRACE_RET_STATUS(rc);
    }
    else if (strcasecmp("Off", p1) == 0) {
        rc = ib_context_set_num(ctx, MODULE_NAME_STR ".trace", 0);
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_log_error(ib, 1, "Failed to parse directive: %s \"%s\"", name, p1);
    IB_FTRACE_RET_STATUS(IB_EINVAL);
}

/**
 * @internal
 * Handle a PocSig directive.
 *
 * @param cp Config parser
 * @param name Directive name
 * @param args List of directive arguments
 * @param cbdata Callback data (from directive registration)
 *
 * @returns Status code
 */
static ib_status_t pocsig_dir_signature(ib_cfgparser_t *cp,
                                        const char *name,
                                        ib_list_t *args,
                                        void *cbdata)
{
    IB_FTRACE_INIT(pocsig_dir_signature);
    ib_engine_t *ib = cp->ib;
    ib_context_t *ctx = cp->cur_ctx ? cp->cur_ctx : ib_context_main(ib);
    ib_list_t *list;
    const char *target;
    const char *op;
    const char *action;
    pocsig_cfg_t *cfg;
    pocsig_phase_t phase;
    pocsig_sig_t *sig;
    const char *errptr;
    int erroff;
    ib_status_t rc;

    /* Get the pocsig configuration for this context. */
    rc = ib_context_module_config(ctx, IB_MODULE_STRUCT_PTR, (void *)&cfg);
    if (rc != IB_OK) {
        ib_log_error(ib, 1, "Failed to fetch %s config: %d",
                     MODULE_NAME_STR, rc);
    }

    /* Setup the PCRE matcher. */
    if (cfg->pcre == NULL) {
        rc = ib_matcher_create(ib, ib_engine_pool_config_get(ib),
                               "pcre", &cfg->pcre);
        if (rc != IB_OK) {
            ib_log_error(ib, 2, "Could not create a PCRE matcher: %d", rc);
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Determine phase and initialize the phase list if required. */
    if (strcasecmp("PocSigPreTx", name) == 0) {
        phase = POCSIG_PRE;
        if (cfg->phase[phase] == NULL) {
            rc = ib_list_create(cfg->phase + POCSIG_PRE,
                                ib_engine_pool_config_get(ib));
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
        }
    }
    else if (strcasecmp("PocSigReqHead", name) == 0) {
        phase = POCSIG_REQHEAD;
        if (cfg->phase[phase] == NULL) {
            ib_log_debug(ib, 4, "Creating list for phase=%d", phase);
            rc = ib_list_create(&list,
                                ib_engine_pool_config_get(ib));
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
            ib_log_debug(ib, 4, "List for phase=%d list=%p", phase, list);
            cfg->phase[phase] = list;
        }
    }
    else if (strcasecmp("PocSigReq", name) == 0) {
        phase = POCSIG_REQ;
        if (cfg->phase[phase] == NULL) {
            rc = ib_list_create(&cfg->phase[phase],
                                ib_engine_pool_config_get(ib));
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
        }
    }
    else if (strcasecmp("PocSigResHead", name) == 0) {
        phase = POCSIG_RESHEAD;
        if (cfg->phase[phase] == NULL) {
            rc = ib_list_create(&cfg->phase[phase],
                                ib_engine_pool_config_get(ib));
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
        }
    }
    else if (strcasecmp("PocSigRes", name) == 0) {
        phase = POCSIG_RES;
        if (cfg->phase[POCSIG_RES] == NULL) {
            rc = ib_list_create(&cfg->phase[POCSIG_RES],
                                ib_engine_pool_config_get(ib));
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
        }
    }
    else if (strcasecmp("PocSigPostTx", name) == 0) {
        phase = POCSIG_POST;
        if (cfg->phase[phase] == NULL) {
            rc = ib_list_create(&cfg->phase[phase],
                                ib_engine_pool_config_get(ib));
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
        }
    }
    else {
        ib_log_error(ib, 2, "Invalid signature: %s", name);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Target */
    rc = ib_list_shift(args, &target);
    if (rc != IB_OK) {
        ib_log_error(ib, 1, "No PocSig target");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Operator */
    rc = ib_list_shift(args, &op);
    if (rc != IB_OK) {
        ib_log_error(ib, 1, "No PocSig operator");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Action */
    rc = ib_list_shift(args, &action);
    if (rc != IB_OK) {
        ib_log_debug(ib, 4, "No PocSig action");
        action = "";
    }

    /* Signature */
    sig = (pocsig_sig_t *)ib_mpool_alloc(ib_engine_pool_config_get(ib),
                                         sizeof(*sig));
    if (sig == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    sig->target = ib_mpool_strdup(ib_engine_pool_config_get(ib), target);
    sig->patt = ib_mpool_strdup(ib_engine_pool_config_get(ib), op);
    sig->emsg = ib_mpool_strdup(ib_engine_pool_config_get(ib), action);

    /* Compile the PCRE patt. */
    if (cfg->pcre == NULL) {
        ib_log_error(ib, 2, "No PCRE matcher available (load the pcre module?)");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    sig->cpatt = ib_matcher_compile(cfg->pcre, sig->patt, &errptr, &erroff);
    if (sig->cpatt == NULL) {
        ib_log_error(ib, 2, "Error at offset=%d of PCRE patt=\"%s\": %s",
                     erroff, sig->patt, errptr);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    ib_log_debug(ib, 4, "POCSIG: \"%s\" \"%s\" \"%s\" phase=%d ctx=%p",
                 target, op, action, phase, ctx);

    /* Add the signature to the phase list. */
    rc = ib_list_push(cfg->phase[phase], sig);
    if (rc != IB_OK) {
        ib_log_error(ib, 1, "Failed to add signature");
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}


/* -- Configuration Data -- */

/* Configuration parameter initialization structure. */
static IB_CFGMAP_INIT_STRUCTURE(pocsig_config_map) = {
    /* trace */
    IB_CFGMAP_INIT_ENTRY(
        MODULE_NAME_STR ".trace",
        IB_FTYPE_NUM,
        &pocsig_global_cfg,
        trace,
        0
    ),

    /* End */
    IB_CFGMAP_INIT_LAST
};

/* Directive initialization structure. */
static IB_DIRMAP_INIT_STRUCTURE(pocsig_directive_map) = {
    /* PocSigTrace - Enable/Disable tracing */
    IB_DIRMAP_INIT_PARAM1(
        "PocSigTrace",
        pocsig_dir_trace,
        NULL
    ),

    /* PocSig* - Define a signature in various phases */
    IB_DIRMAP_INIT_LIST(
        "PocSigPreTx",
        pocsig_dir_signature,
        NULL
    ),
    IB_DIRMAP_INIT_LIST(
        "PocSigReqHead",
        pocsig_dir_signature,
        NULL
    ),
    IB_DIRMAP_INIT_LIST(
        "PocSigReq",
        pocsig_dir_signature,
        NULL
    ),
    IB_DIRMAP_INIT_LIST(
        "PocSigResHead",
        pocsig_dir_signature,
        NULL
    ),
    IB_DIRMAP_INIT_LIST(
        "PocSigRes",
        pocsig_dir_signature,
        NULL
    ),
    IB_DIRMAP_INIT_LIST(
        "PocSigPostTx",
        pocsig_dir_signature,
        NULL
    ),

    /* End */
    IB_DIRMAP_INIT_LAST
};


/* -- Hook Handlers -- */

/**
 * @internal
 * Handle signature execution.
 *
 * @param ib Engine
 * @param tx Transaction
 * @param cbdata Phase passed as pointer value
 *
 * @return Status code
 */
static ib_status_t pocsig_handle_sigs(ib_engine_t *ib,
                                      ib_tx_t *tx,
                                      void *cbdata)
{
    IB_FTRACE_INIT(pocsig_handle_post);
    pocsig_cfg_t *cfg;
    pocsig_phase_t phase = (pocsig_phase_t)(uintptr_t)cbdata;
    ib_list_t *sigs;
    ib_list_node_t *node;
    int dbglvl;
    ib_status_t rc;

    /* Get the pocsig configuration for this context. */
    rc = ib_context_module_config(tx->ctx, IB_MODULE_STRUCT_PTR, (void *)&cfg);
    if (rc != IB_OK) {
        ib_log_error(ib, 1, "Failed to fetch %s config: %d",
                     MODULE_NAME_STR, rc);
    }

    /* If tracing is enabled, lower the log level. */
    dbglvl = cfg->trace ? 4 : 9;

    /* Get the list of sigs for this phase. */
    sigs = cfg->phase[phase];
    if (sigs == NULL) {
        ib_log_debug(ib, dbglvl, "No signatures for phase=%d ctx=%p",
                     phase, tx->ctx);
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    ib_log_debug(ib, dbglvl, "Executing %d signatures for phase=%d ctx=%p",
                 ib_list_elements(sigs), phase, tx->ctx);

    /* Run all the sigs for this phase. */
    IB_LIST_LOOP(sigs, node) {
        pocsig_sig_t *s = (pocsig_sig_t *)ib_list_node_data(node);
        ib_field_t *f;

        /* Fetch the field. */
        rc = ib_data_get(tx->dpi, s->target, &f);
        if (rc != IB_OK) {
            ib_log_error(ib, 4, "PocSig: No field named \"%s\"", s->target);
            continue;
        }

        /* Perform the match. */
        ib_log_debug(ib, dbglvl, "PocSig: Matching \"%s\" against field \"%s\"",
                     s->patt, s->target);
        rc = ib_matcher_match_field(cfg->pcre, s->cpatt, 0, f, NULL);
        if (rc == IB_OK) {
            ib_logevent_t *e;

            ib_log_debug(ib, dbglvl, "PocSig MATCH: %s at %s", s->patt, s->target);

            /* Create the event. */
            rc = ib_logevent_create(
                &e,
                tx->mp,
                "-",
                IB_LEVENT_TYPE_ALERT,
                IB_LEVENT_ACT_UNKNOWN,
                IB_LEVENT_PCLASS_UNKNOWN,
                IB_LEVENT_SCLASS_UNKNOWN,
                IB_LEVENT_SYS_UNKNOWN,
                IB_LEVENT_ACTION_IGNORE,
                IB_LEVENT_ACTION_IGNORE,
                90,
                80,
                s->emsg
            );
            if (rc != IB_OK) {
                ib_log_error(ib, 3, "PocSig: Error generating event: %d", rc);
                continue;
            }

            /* Log the event. */
            ib_clog_event(tx->ctx, e);
        }
        else {
            ib_log_debug(ib, dbglvl, "PocSig NOMATCH");
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}


/* -- Module Routines -- */

static ib_status_t pocsig_init(ib_engine_t *ib,
                               ib_module_t *m)
{
    IB_FTRACE_INIT(pocsig_init);

    /* Initialize the global config items that are not mapped to config
     * parameters as these will not have default values.
     */
    memset(pocsig_global_cfg.phase, 0, sizeof(pocsig_global_cfg.phase));
    pocsig_global_cfg.pcre = NULL;

    IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t pocsig_context_init(ib_engine_t *ib,
                                       ib_module_t *m,
                                       ib_context_t *ctx)
{
    IB_FTRACE_INIT(pocsig_context_init);
    pocsig_cfg_t *cfg;
    ib_status_t rc;

    rc = ib_context_module_config(ctx, m, (void *)&cfg);
    if (rc != IB_OK) {
        ib_log_error(ib, 1, "Failed to fetch %s config: %d",
                     MODULE_NAME_STR, rc);
    }

    /// @todo Inherit signatures from parent context???

    /* Register hooks to handle the phases. */
    ib_hook_register_context(ctx, handle_context_tx_event,
                             (ib_void_fn_t)pocsig_handle_sigs,
                             (void *)POCSIG_PRE);
    ib_hook_register_context(ctx, handle_request_headers_event,
                             (ib_void_fn_t)pocsig_handle_sigs,
                             (void *)POCSIG_REQHEAD);
    ib_hook_register_context(ctx, handle_request_event,
                             (ib_void_fn_t)pocsig_handle_sigs,
                             (void *)POCSIG_REQ);
    ib_hook_register_context(ctx, handle_response_headers_event,
                             (ib_void_fn_t)pocsig_handle_sigs,
                             (void *)POCSIG_RESHEAD);
    ib_hook_register_context(ctx, handle_response_event,
                             (ib_void_fn_t)pocsig_handle_sigs,
                             (void *)POCSIG_RES);
    ib_hook_register_context(ctx, handle_postprocess_event,
                             (ib_void_fn_t)pocsig_handle_sigs,
                             (void *)POCSIG_POST);

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Module structure.
 *
 * This structure defines some metadata, config data and various functions.
 */
IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,           /**< Default metadata */
    MODULE_NAME_STR,                     /**< Module name */
    IB_MODULE_CONFIG(&pocsig_global_cfg),/**< Global config data */
    pocsig_config_map,                   /**< Configuration field map */
    pocsig_directive_map,                /**< Config directive map */
    pocsig_init,                         /**< Initialize function */
    NULL,                                /**< Finish function */
    pocsig_context_init,                 /**< Context init function */
    NULL                                 /**< Context fini function */
);

