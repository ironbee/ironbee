/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
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
 */

/* Demo ident module: implement HTTP Basic authn because it offers
 * a simple proof-of-concept for an ident framework
 *
 * NOTE: this makes no attempt to check a password!
 * We only return a username set by a client.
 *
 * If we want to enable checking passwords then we'll want
 * another framework for password lookups (c.f. apache httpd).
 * But in the case of basic authn, I'm not convinced that would
 * add value to ironbee.
 */

#include <ironbee/context.h>
#include <ironbee/module.h>
#include <ironbee/engine_state.h>
#include <ironbee/parsed_content.h>
#include <ironbee/ident.h>

#include <ctype.h>
#include <assert.h>

/** Module name. */
#define MODULE_NAME        ident_authbasic
/** Stringified version of MODULE_NAME */
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

IB_MODULE_DECLARE();


typedef struct ident_authbasic_cfg_t {
    const char *realm;
} ident_authbasic_cfg_t;

/**
 * Get the value of an HTTP header
 *
 * @param pool Pool to allocate from
 * @param wrapper The header wrapper
 * @param name The header name
 * @return Header value, or NULL if not set
 *
 * FIXME: make this a general utility function
 */
static const char *header_get(ib_mpool_t *pool,
                              ib_parsed_header_wrapper_t *wrapper,
                              const char *name)
{
    ib_parsed_name_value_pair_list_t *p = wrapper->head;
    /* To check "last" condition in for() would be to omit the last element.
     * So check each element before checking for end-of-list
     */
    for (p = wrapper->head;
         strncasecmp(name, (const char*)ib_bytestr_ptr(p->name),
                     ib_bytestr_length(p->name));
         p = p->next) {
        if (p == wrapper->tail) {
            return NULL;
        }
    }

    return ib_mpool_memdup_to_str(pool, ib_bytestr_ptr(p->value),
                                  ib_bytestr_length(p->value));
}
/**
 * Decode a Base64-encoded string.  Code based on APR's base64 module.
 *
 * @param pool Pool to allocate from
 * @param encoded The encoded string
 * @return The decoded string
 */
static char *base64_decode(ib_mpool_t *pool, const char *encoded)
{
    /* ASCII table */
    static const unsigned char pr2six[256] = {
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
        64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
        64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
    };
    char *decoded;
    int len;
    register const unsigned char *bufin = (const unsigned char*) encoded;
    register unsigned char *bufout;
    register unsigned int nprbytes;

    while (pr2six[*(bufin++)] <= 63);

    nprbytes = (bufin - (const unsigned char *) encoded) - 1;
    len = (((int)nprbytes + 3) / 4) * 3;

    decoded = ib_mpool_alloc(pool, len+1);
    assert(decoded != NULL);
    bufout = (unsigned char *) decoded;
    bufin = (const unsigned char*) encoded;

    while (nprbytes > 4) {
        *(bufout++) =
            (unsigned char) (pr2six[*bufin] << 2 | pr2six[bufin[1]] >> 4);
        *(bufout++) =
            (unsigned char) (pr2six[bufin[1]] << 4 | pr2six[bufin[2]] >> 2);
        *(bufout++) =
            (unsigned char) (pr2six[bufin[2]] << 6 | pr2six[bufin[3]]);
        bufin += 4;
        nprbytes -= 4;
    }

    /* Note: (nprbytes == 1) would be an error, so just ingore that case */
    if (nprbytes > 1) {
        *(bufout++) =
            (unsigned char) (pr2six[*bufin] << 2 | pr2six[bufin[1]] >> 4);
    }
    if (nprbytes > 2) {
        *(bufout++) =
            (unsigned char) (pr2six[bufin[1]] << 4 | pr2six[bufin[2]] >> 2);
    }
    if (nprbytes > 3) {
        *(bufout++) =
            (unsigned char) (pr2six[bufin[2]] << 6 | pr2six[bufin[3]]);
    }

    *bufout = '\0';

    return decoded;
}
/**
 * Decode HTTP Basic authentication
 * NOTE: this makes no attempt to check a password!
 * We only return a username set by a client.
 *
 * If we want to enable checking passwords then we'll want
 * another framework for password lookups (c.f. apache httpd).
 *
 * @param tx The transaction
 * @return The HTTP basic authenticated username, or NULL if not authenticated
 */
static const char *basic_get_user(ib_tx_t *tx)
{
    const char *authorization;
    const char *p;
    char *decoded;
    char *outp;

    /* Get Authorization header */
    authorization = header_get(tx->mp, tx->request_header, "authorization");
    if (!authorization) {
        ib_log_debug_tx(tx, "Basic Authentication: no header!");
        return NULL;
    }

    /* Parse it for credentials string */

    p = strcasestr(authorization, "Basic");
    if (p == NULL) {
        ib_log_debug_tx(tx, "Basic Authentication: no credentials!");
        return NULL;
    }
    /* skip whitespace */
    for (p += 5; isspace(*p); ++p);

    /* base64-decode the string */
    decoded = base64_decode(tx->mp, p);

    /* return the string to the left of the first colon */
    outp = strchr(decoded, ':');
    if (outp == NULL) {
        ib_log_error_tx(tx, "Basic Authentication: Error parsing %s", decoded);
        return NULL;
    }
    *outp = '\0';
    ib_log_info_tx(tx, "Basic authentication: username %s", decoded);

    return decoded;
}
/**
 * Issue an HTTP Basic Authentication Challenge
 *
 * @param tx The transaction
 * @return status
 */
static ib_status_t basic_challenge(ib_tx_t *tx)
{
    /* Enforce basic authn on a client that didn't authenticate */
    char *challenge;
    ident_authbasic_cfg_t *cfg;
    ib_status_t rc;
    ib_module_t *m;

    rc = ib_engine_module_get(tx->ib, MODULE_NAME_STR, &m);
    assert((rc == IB_OK) && (m != NULL));
    rc = ib_context_module_config(ib_context_main(tx->ib), m, &cfg);
    assert((rc == IB_OK) && (cfg != NULL));

    ib_log_info_tx(tx, "Challenging Client (HTTP Basic Authentication)");

    challenge = ib_mpool_alloc(tx->mp, strlen("Basic realm=..")
                                         + strlen(cfg->realm) + 1);
    assert(challenge != NULL);
    sprintf(challenge, "Basic realm=\"%s\"", cfg->realm);

    ib_server_error_response(ib_plugin(), tx, 401);
    ib_server_error_header(ib_plugin(), tx, "WWW-Authenticate", challenge);
    return IB_OK;
}

/**
 * Initialisation function: register HTTP Basic provider with ident module
 *
 * @param ib The engine
 * @param m The module
 * @param cbdata Unusued
 * @return status
 */
static ib_status_t ident_authbasic_init(ib_engine_t *ib, ib_module_t *m, void *cbdata)
{
    static ib_ident_provider_t ident_authbasic_provider = {
        request_header_finished_event,
        basic_get_user,
        basic_challenge
    };
    return ib_ident_provider_register("authbasic", &ident_authbasic_provider);
}
/**
 * Configuration function to set basic authentication realm
 *
 * @param cp IronBee configuration parser
 * @param name Unused
 * @param p1 Realm value to set
 * @return OK
 */
static ib_status_t ident_authbasic_realm(ib_cfgparser_t *cp, const char *name,
                                         const char *p1, void *dummy)
{
    ident_authbasic_cfg_t *cfg;
    ib_status_t rc;
    ib_module_t *m;

    rc = ib_engine_module_get(cp->ib, MODULE_NAME_STR, &m);
    assert((rc == IB_OK) && (m != NULL));
    rc = ib_context_module_config(ib_context_main(cp->ib), m, &cfg);
    assert((rc == IB_OK) && (cfg != NULL));

    cfg->realm = p1;
    return rc;
}

static IB_DIRMAP_INIT_STRUCTURE(ident_authbasic_config) = {
    IB_DIRMAP_INIT_PARAM1(
        "AuthBasicRealm",
        ident_authbasic_realm,
        NULL
    ),

    /* End */
    IB_DIRMAP_INIT_LAST
};

static ident_authbasic_cfg_t ident_authbasic_ini = {
    "Ironbee"
};

IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,               /**< Default metadata */
    MODULE_NAME_STR,                         /**< Module name */
    IB_MODULE_CONFIG(&ident_authbasic_ini),  /**< Global config data */
    NULL,                                    /**< Configuration field map */
    ident_authbasic_config,                  /**< Config directive map */
    ident_authbasic_init, NULL,              /**< Initialize function */
    NULL, NULL,                              /**< Finish function */
);
