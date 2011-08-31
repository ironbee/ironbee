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
 * @brief IronBee - Proof of Concept using AhoCorasick AC Matcher Module
 *
 * This module serves as an example and proof of concept for
 * signatures using AC based Matcher.
 *
 * @author Pablo Rincon <pablo.rincon.crespo@gmail.com>
 */

#include <string.h>
#include <strings.h>

#include <ironbee/engine.h>
#include <ironbee/util.h>
#include <ironbee/module.h>
#include <ironbee/provider.h>

/* Define the module name as well as a string version of it. */
#define MODULE_NAME               pocacsig
#define MODULE_NAME_STR           IB_XSTRINGIFY(MODULE_NAME)

/* Declare the public module symbol. */
IB_MODULE_DECLARE();

typedef struct pocacsig_cfg_t pocacsig_cfg_t;
typedef struct pocacsig_sig_t pocacsig_sig_t;

/** Signature Phases */
typedef enum {
    POCACSIG_PRE,                   /**< Pre transaction phase */
    POCACSIG_REQHEAD,               /**< Request headers phase */
    POCACSIG_REQ,                   /**< Request phase */
    POCACSIG_RESHEAD,               /**< Response headers phase */
    POCACSIG_RES,                   /**< Response phase */
    POCACSIG_POST,                  /**< Post transaction phase */

    /* Keep track of the number of defined phases. */
    POCACSIG_PHASE_NUM
} pocacsig_phase_t;

/** Signature Structure */
struct pocacsig_sig_t {
    const char         *target;   /**< Target name */
    const char         *prequal;  /**< AC Pattern / prequalifier */
    const char         *patt;     /**< Pattern to match in target */
    void               *cpatt;    /**< Compiled PCRE regex */
    const char         *emsg;     /**< Event message */
};

/** Module Configuration Structure */
struct pocacsig_cfg_t {
    /* Exposed as configuration parameters. */
    ib_num_t            trace;    /**< Log signature tracing */

    /* Private. */
    ib_list_t          *phase[POCACSIG_PHASE_NUM]; /**< Phase signature lists */
    ib_matcher_t       *pcre;                      /**< PCRE matcher */
};

typedef struct pocacsig_fieldentry_t pocacsig_fieldentry_t;

/** Entries of fields per phase */
struct pocacsig_fieldentry_t {
    char           *target;  /**< This entry contains patterns for this field */
    ib_matcher_t   *ac_matcher; /**< AC matcher with patterns for this field
                                  (and phase) */
};

/* Instantiate a module global configuration. */
static pocacsig_cfg_t pocacsig_global_cfg;


/* -- Directive Handlers -- */

/**
 * @internal
 * Handle a PocACSigTrace directive.
 *
 * @param cp Config parser
 * @param name Directive name
 * @param p1 First parameter
 * @param cbdata Callback data (from directive registration)
 *
 * @returns Status code
 */
static ib_status_t pocacsig_dir_trace(ib_cfgparser_t *cp,
                                    const char *name,
                                    const char *p1,
                                    void *cbdata)
{
    IB_FTRACE_INIT(pocacsig_dir_trace);
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
 * Handle a PocACSig directive.
 *
 * @param cp Config parser
 * @param name Directive name
 * @param args List of directive arguments
 * @param cbdata Callback data (from directive registration)
 *
 * @returns Status code
 */
static ib_status_t pocacsig_dir_signature(ib_cfgparser_t *cp,
                                        const char *name,
                                        ib_list_t *args,
                                        void *cbdata)
{
    IB_FTRACE_INIT(pocacsig_dir_signature);
    ib_engine_t *ib = cp->ib;
    ib_context_t *ctx = cp->cur_ctx ? cp->cur_ctx : ib_context_main(ib);
    ib_list_t *list;

    const char *target;
    const char *op;
    const char *prequal;
    const char *action;

    pocacsig_cfg_t *cfg;
    pocacsig_phase_t phase;
    pocacsig_sig_t *sig;

    const char *errptr;
    int erroff;

    ib_status_t rc;

    /* Get the pocacsig configuration for this context. */
    rc = ib_context_module_config(ctx, &IB_MODULE_SYM, (void *)&cfg);
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
    if (strcasecmp("PocACSigPreTx", name) == 0) {
        phase = POCACSIG_PRE;
        if (cfg->phase[phase] == NULL) {
            rc = ib_list_create(cfg->phase + POCACSIG_PRE,
                                ib_engine_pool_config_get(ib));
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
        }
    }
    else if (strcasecmp("PocACSigReqHead", name) == 0) {
        phase = POCACSIG_REQHEAD;
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
    else if (strcasecmp("PocACSigReq", name) == 0) {
        phase = POCACSIG_REQ;
        if (cfg->phase[phase] == NULL) {
            rc = ib_list_create(&cfg->phase[phase],
                                ib_engine_pool_config_get(ib));
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
        }
    }
    else if (strcasecmp("PocACSigResHead", name) == 0) {
        phase = POCACSIG_RESHEAD;
        if (cfg->phase[phase] == NULL) {
            rc = ib_list_create(&cfg->phase[phase],
                                ib_engine_pool_config_get(ib));
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
        }
    }
    else if (strcasecmp("PocACSigRes", name) == 0) {
        phase = POCACSIG_RES;
        if (cfg->phase[POCACSIG_RES] == NULL) {
            rc = ib_list_create(&cfg->phase[POCACSIG_RES],
                                ib_engine_pool_config_get(ib));
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
        }
    }
    else if (strcasecmp("PocACSigPostTx", name) == 0) {
        phase = POCACSIG_POST;
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
        ib_log_error(ib, 1, "No PocACSig target");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Prequal (The AC pattern) */
    rc = ib_list_shift(args, &prequal);
    if (rc != IB_OK) {
        ib_log_error(ib, 1, "No PocACSig operator");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* An extra Pcre */
    rc = ib_list_shift(args, &op);
    if (rc != IB_OK) {
        ib_log_error(ib, 1, "No PocACSig operator");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Action */
    rc = ib_list_shift(args, &action);
    if (rc != IB_OK) {
        ib_log_debug(ib, 4, "No PocACSig action");
        action = "";
    }

    /* Signature */
    sig = (pocacsig_sig_t *)ib_mpool_alloc(ib_engine_pool_config_get(ib),
                                         sizeof(*sig));
    if (sig == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    sig->target = ib_mpool_memdup(ib_engine_pool_config_get(ib),
                                  target, strlen(target));
    sig->prequal = ib_mpool_memdup(ib_engine_pool_config_get(ib),
                                 prequal, strlen(prequal));
    sig->patt = ib_mpool_memdup(ib_engine_pool_config_get(ib),
                                 op, strlen(op));
    sig->emsg = ib_mpool_memdup(ib_engine_pool_config_get(ib),
                                action, strlen(action));

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

    ib_log_debug(ib, 4, "POCACSIG: \"%s\" \"%s\" \"%s\" phase=%d ctx=%p",
                 target, op, action, phase, ctx);

    ib_list_t *sigs = cfg->phase[phase];
    ib_list_node_t *node;
    pocacsig_fieldentry_t *pfe = NULL;
    uint8_t found = 0;

    /** First search if there's already an AC matcher for this field (target) */
    if (ib_list_elements(sigs) > 0) {
        /** Iterate entries */
        IB_LIST_LOOP(sigs, node) {
            pfe = (pocacsig_fieldentry_t *)ib_list_node_data(node);
            if (strcmp(pfe->target, target) == 0) {
                /** Entry found for this field. Then we just need to add it to
                   the AC tree */
                found = 1;
                break;
            }
        }
    }

    /** If no entry was found, create a new one, initialize it, and append it */
    if (found == 0) {
        ib_mpool_t *mp = ib_engine_pool_config_get(ib);
        pfe = (pocacsig_fieldentry_t *) ib_mpool_calloc(mp, 1,
                                         sizeof(pocacsig_fieldentry_t));

        /** Setup the AC matcher. */
        rc = ib_matcher_instance_create(ib, mp,
                               "ac", &pfe->ac_matcher);
        if (rc != IB_OK) {
            ib_log_error(ib, 2, "Could not create an AC matcher: %d", rc);
            IB_FTRACE_RET_STATUS(rc);
        }

        /** Copy the target field name */
        pfe->target = ib_mpool_memdup(ib_engine_pool_config_get(ib),
                                      target, strlen(target));

        /** Append it to the phase list */
        rc = ib_list_push(sigs, pfe);
        if (rc != IB_OK) {
            ib_log_error(ib, 2, "Could not add an entry to the phase %d"
                                " (ret: %d)", phase, rc);
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /** Add the pattern to the AC matcher */
    rc = ib_matcher_add_pattern_ex(pfe->ac_matcher, sig->prequal, NULL, sig,
                                   &errptr, &erroff);

    IB_FTRACE_RET_STATUS(rc);
}


/* -- Configuration Data -- */

/* Configuration parameter initialization structure. */
static IB_CFGMAP_INIT_STRUCTURE(pocacsig_config_map) = {
    /* trace */
    IB_CFGMAP_INIT_ENTRY(
        MODULE_NAME_STR ".trace",
        IB_FTYPE_NUM,
        &pocacsig_global_cfg,
        trace,
        0
    ),

    /* End */
    IB_CFGMAP_INIT_LAST
};

/* Directive initialization structure. */
static IB_DIRMAP_INIT_STRUCTURE(pocacsig_directive_map) = {
    /* PocACSigTrace - Enable/Disable tracing */
    IB_DIRMAP_INIT_PARAM1(
        "PocACSigTrace",
        pocacsig_dir_trace,
        NULL
    ),

    /* PocACSig* - Define a signature in various phases */
    IB_DIRMAP_INIT_LIST(
        "PocACSigPreTx",
        pocacsig_dir_signature,
        NULL
    ),
    IB_DIRMAP_INIT_LIST(
        "PocACSigReqHead",
        pocacsig_dir_signature,
        NULL
    ),
    IB_DIRMAP_INIT_LIST(
        "PocACSigReq",
        pocacsig_dir_signature,
        NULL
    ),
    IB_DIRMAP_INIT_LIST(
        "PocACSigResHead",
        pocacsig_dir_signature,
        NULL
    ),
    IB_DIRMAP_INIT_LIST(
        "PocACSigRes",
        pocacsig_dir_signature,
        NULL
    ),
    IB_DIRMAP_INIT_LIST(
        "PocACSigPostTx",
        pocacsig_dir_signature,
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
static ib_status_t pocacsig_handle_sigs(ib_engine_t *ib,
                                      ib_tx_t *tx,
                                      void *cbdata)
{
    IB_FTRACE_INIT(pocacsig_handle_post);
    pocacsig_cfg_t *cfg;
    pocacsig_phase_t phase = (pocacsig_phase_t)(uintptr_t)cbdata;
    ib_list_t *sigs;
    ib_list_node_t *node;
    ib_list_node_t *sig_node;
    int dbglvl;
    ib_status_t rc;

    /* Get the pocacsig configuration for this context. */
    rc = ib_context_module_config(tx->ctx, &IB_MODULE_SYM, (void *)&cfg);
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

    ib_ac_context_t ctx;
    ib_ac_context_t *ac_mctx = NULL;
    ac_mctx = &ctx;
    ib_ac_t *ac_tree = NULL;

    ib_ac_init_ctx(ac_mctx, ac_tree);

    /* Get all the fields and run the AC trees for each of them. */
    IB_LIST_LOOP(sigs, node) {
        pocacsig_fieldentry_t *pfe = NULL;
        pfe = (pocacsig_fieldentry_t *)ib_list_node_data(node);
        ib_field_t *f;

        /* Fetch the field. */
        rc = ib_data_get(tx->dpi, pfe->target, &f);
        if (rc != IB_OK) {
            ib_log_error(ib, 4, "PocACSig: No field named \"%s\"", pfe->target);
            continue;
        }

        /* Perform the match. */
        ib_log_debug(ib, dbglvl, "PocACSig: Matching tree \"%x\" against"
                                 " field \"%s\"", pfe->ac_matcher, pfe->target);

        rc = ib_matcher_exec_field(pfe->ac_matcher, 0, f, (void *)ac_mctx);
        if (rc == IB_OK) {
            /** The AC prequalifier matched!! Now let's check PCREs! */
            IB_LIST_LOOP(ac_mctx->match_list, sig_node) {
                ib_ac_match_t *acm = NULL;
                pocacsig_sig_t *s = NULL;
                acm = (ib_ac_match_t *)ib_list_node_data(sig_node);
                s = (pocacsig_sig_t *)acm->data;

                /* Perform the match. */
                ib_log_debug(ib, dbglvl, "PocSig: Matched prequal:\"%s\". Now"
                                         " Matching \"%s\" against field"
                                         " \"%s\"", s->prequal,
                                         s->patt, s->target);
                rc = ib_matcher_match_field(cfg->pcre, s->cpatt, 0, f, NULL);
                if (rc == IB_OK) {
                    /** It matched! So we should have at the ctx a list of
                        matches with references to the sigs. Let's iterate them
                        and process the pcre expression */
                    ib_logevent_t *e;
        
                    ib_log_debug(ib, dbglvl, "PocACSig MATCH: prequal:\"%s\""
                                             " pcre:\"%s\" at %s", s->prequal,
                                             s->patt, pfe->target);
        
                    /* Create the event. */
                    rc = ib_logevent_create(
                        &e,
                        tx->mp,
                        "-",
                        IB_LEVENT_TYPE_ALERT,
                        IB_LEVENT_ACT_UNKNOWN,
                        IB_LEVENT_PCLASS_UNKNOWN,
                        IB_LEVENT_SCLASS_UNKNOWN,
                        90,
                        80,
                        IB_LEVENT_SYS_UNKNOWN,
                        IB_LEVENT_ACTION_IGNORE,
                        IB_LEVENT_ACTION_IGNORE,
                        s->emsg
                    );

                    if (rc != IB_OK) {
                        ib_log_error(ib, 3, "PocACSig: Error generating "
                                     "event: %d", rc);
                        continue;
                    }
        
                    /* Log the event. */
                    ib_clog_event(tx->ctx, e);
                }
            }
        }
        else {
            ib_log_debug(ib, dbglvl, "PocACSig NOMATCH");
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}


/* -- Module Routines -- */

static ib_status_t pocacsig_init(ib_engine_t *ib,
                               ib_module_t *m)
{
    IB_FTRACE_INIT(pocacsig_init);

    /* Initialize the global config items that are not mapped to config
     * parameters as these will not have default values.
     */
    memset(pocacsig_global_cfg.phase, 0, sizeof(pocacsig_global_cfg.phase));
    pocacsig_global_cfg.pcre = NULL;

    IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t pocacsig_context_init(ib_engine_t *ib,
                                       ib_module_t *m,
                                       ib_context_t *ctx)
{
    IB_FTRACE_INIT(pocacsig_context_init);
    pocacsig_cfg_t *cfg;
    ib_status_t rc;

    rc = ib_context_module_config(ctx, m, (void *)&cfg);
    if (rc != IB_OK) {
        ib_log_error(ib, 1, "Failed to fetch %s config: %d",
                     MODULE_NAME_STR, rc);
    }

    /// @todo Inherit signatures from parent context???

    /* Register hooks to handle the phases. */
    ib_hook_register_context(ctx, handle_context_tx_event,
                             (ib_void_fn_t)pocacsig_handle_sigs,
                             (void *)POCACSIG_PRE);
    ib_hook_register_context(ctx, handle_request_headers_event,
                             (ib_void_fn_t)pocacsig_handle_sigs,
                             (void *)POCACSIG_REQHEAD);
    ib_hook_register_context(ctx, handle_request_event,
                             (ib_void_fn_t)pocacsig_handle_sigs,
                             (void *)POCACSIG_REQ);
    ib_hook_register_context(ctx, handle_response_headers_event,
                             (ib_void_fn_t)pocacsig_handle_sigs,
                             (void *)POCACSIG_RESHEAD);
    ib_hook_register_context(ctx, handle_response_event,
                             (ib_void_fn_t)pocacsig_handle_sigs,
                             (void *)POCACSIG_RES);
    ib_hook_register_context(ctx, handle_postprocess_event,
                             (ib_void_fn_t)pocacsig_handle_sigs,
                             (void *)POCACSIG_POST);

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
    IB_MODULE_CONFIG(&pocacsig_global_cfg),/**< Global config data */
    pocacsig_config_map,                   /**< Configuration field map */
    pocacsig_directive_map,                /**< Config directive map */
    pocacsig_init,                         /**< Initialize function */
    NULL,                                /**< Finish function */
    pocacsig_context_init,                 /**< Context init function */
);

