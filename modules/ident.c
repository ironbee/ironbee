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

/**
 * @file
 * @brief IronBee --- user identity framework module.
 *
 *
 */


#include <ironbee/context.h>
#include <ironbee/module.h>
#include <ironbee/engine_state.h>
#include <ironbee/server.h>
#include <ironbee/hash.h>
#include <ironbee/ident.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>

/** Module name. */
#define MODULE_NAME        ident
/** Stringified version of MODULE_NAME */
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

IB_MODULE_DECLARE();

typedef enum { ident_off, ident_log, ident_require } ident_mode_t;

typedef struct ident_cfg_t {
    ident_mode_t mode;
    const char *type;
    bool accept_any;
} ident_cfg_t;

static const char *dummy_id(ib_tx_t *tx)
{
    ib_log_info_tx(tx, "Dummy ident check doing nothing");
    return NULL;
}
static ib_status_t dummy_forbid(ib_tx_t *tx)
{
    /* If we're supposed to issue a challenge but have no method,
     * we'll just have to deny access
     */
    ib_log_error_tx(tx, "No authentication configured to challenge client");
    ib_server_error_response(ib_plugin(), tx, 403);
    return IB_OK;
}
static ib_ident_provider_t ident_dummy_provider = {
	request_header_finished_event,
	dummy_id,
	dummy_forbid
};

static ib_hash_t *ident_providers;

/* Export register function so a module can register a provider */
ib_status_t ib_ident_provider_register(const char *name,
                                       ib_ident_provider_t *provider)
{
    return ib_hash_set(ident_providers, name, provider);
}

static ib_status_t ident_mode(ib_cfgparser_t *cp, const char *name,
                              const char *p1, void *dummy)
{
    ident_cfg_t *cfg;
    ib_status_t rc;
    ib_module_t *m;

    rc = ib_engine_module_get(cp->ib, MODULE_NAME_STR, &m);
    assert((rc == IB_OK) && (m != NULL));
    rc = ib_context_module_config(ib_context_main(cp->ib), m, &cfg);
    assert((rc == IB_OK) && (cfg != NULL));

    if (!strcasecmp(p1, "Off")) {
        cfg->mode = ident_off;
    }
    else if (!strcasecmp(p1, "Log")) {
        cfg->mode = ident_log;
    }
    else if (!strcasecmp(p1, "Require")) {
        cfg->mode = ident_require;
    }
    else {
        rc = IB_EINVAL;
    }
    return rc;
}
static ib_status_t ident_type(ib_cfgparser_t *cp, const char *name,
                              const char *p1, const char *p2, void *dummy)
{
    ident_cfg_t *cfg;
    ib_status_t rc;
    ib_module_t *m;
    char *p;

    rc = ib_engine_module_get(cp->ib, MODULE_NAME_STR, &m);
    assert((rc == IB_OK) && (m != NULL));
    rc = ib_context_module_config(ib_context_main(cp->ib), m, &cfg);
    assert((rc == IB_OK) && (cfg != NULL));

    cfg->type = p = ib_mpool_strdup(cp->mp, p1);
    assert(p != NULL);
    do {
        if (isupper(*p)) {
            *p = tolower(*p);
        }
    } while (*++p);
    if (p2 && !strcasecmp(p2, "any")) {
        cfg->accept_any = 1;
    }
    else {
        cfg->accept_any = 0;
    }
    return rc;
}

static ib_status_t ident_handler(ib_engine_t *ib, ib_tx_t *tx,
                                 ib_state_event_type_t event,
                                 void *cbdata)
{
    ident_cfg_t *cfg;
    const char *userid = NULL;
    ib_ident_provider_t *provider;
    ib_status_t rc;
    ib_module_t *m;

    assert(event == request_header_finished_event || event == request_finished_event);

    rc = ib_engine_module_get(ib, MODULE_NAME_STR, &m);
    assert((rc == IB_OK) && (m != NULL));
    rc = ib_context_module_config(ib_context_main(ib), m, &cfg);
    assert((rc == IB_OK) && (cfg != NULL));

    if (cfg->mode == ident_off) {
        return IB_OK;
    }
    if (cfg->type != NULL) {
        ib_hash_get(ident_providers, &provider, cfg->type);
        if (provider == NULL) {
            ib_log_error_tx(tx, "Identifier '%s' configured but not available", cfg->type);
            provider = &ident_dummy_provider;
        }
    }
    else {
        ib_log_error_tx(tx, "Ident module loaded but not configured!");
        provider = &ident_dummy_provider;
    }

    if (provider->event != event) {
        /* This provider doesn't check now */
        return IB_OK;
    }

    /* OK, ident is on.  Verify if there is a user ID */
    userid = provider->check_id(tx);

    if (userid == NULL && cfg->accept_any) {
        ib_hash_iterator_t *iterator = ib_hash_iterator_create(tx->mp);
        ib_ident_provider_t *p;
        for (ib_hash_iterator_first(iterator, ident_providers);
             !userid && !ib_hash_iterator_at_end(iterator);
             ib_hash_iterator_next(iterator)) {
            ib_hash_iterator_fetch(NULL, NULL, &p, iterator);
            if (p->check_id != provider->check_id) {
                userid = p->check_id(tx);
            }
        }
    }

    if (userid != NULL) {
        ib_log_info(ib, "User identified as %s", userid);
        return IB_OK;
    }

    /* If we haven't configured an ident type, don't enforce */
    if (cfg->type == NULL) {
        return IB_OK;
    }

    /* If we're enforcing ident, send a challenge */
    return provider->challenge(tx);
}

static ib_status_t ident_init(ib_engine_t *ib, ib_module_t *m, void *cbdata)
{
    ib_status_t rc;
    ib_mpool_t *pool = ib_engine_pool_main_get(ib);

    rc = ib_hash_create(&ident_providers, pool);
    if (rc != IB_OK) {
        return rc;
    }
    /* register a per-request authentication action
     *
     * Register it for both header_finished and request_finished.
     * It's up to each identifier to determine when to run.
     * Any that work on headers alone should use headers_finished,
     * while those that use request body data will need to run
     * at request_finished event.
     */
    rc = ib_hook_tx_register(ib, request_header_finished_event,
                             ident_handler, NULL);
    if (rc != IB_OK) {
        return rc;
    }
    rc = ib_hook_tx_register(ib, request_finished_event,
                             ident_handler, NULL);
    if (rc != IB_OK) {
        return rc;
    }
    return rc;
}


static IB_DIRMAP_INIT_STRUCTURE(ident_config) = {
    IB_DIRMAP_INIT_PARAM1(
        "IdentMode",
        ident_mode,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM2(
        "IdentType",
        ident_type,
        NULL
    ),

    /* End */
    IB_DIRMAP_INIT_LAST
};

static ident_cfg_t ident_cfg_ini = {
    ident_off,
    NULL,
    1
};

IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,           /**< Default metadata */
    MODULE_NAME_STR,                     /**< Module name */
    IB_MODULE_CONFIG(&ident_cfg_ini),    /**< Global config data */
    NULL,                                /**< Configuration field map */
    ident_config,                        /**< Config directive map */
    ident_init, NULL,                    /**< Initialize function */
    NULL, NULL,                          /**< Finish function */
);
