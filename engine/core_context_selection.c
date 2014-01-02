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
 * @brief IronBee --- Core Module Context Selection
 *
 * @author Nick LeRoy <nick@qualys.com>
 */

#include "ironbee_config_auto.h"


#ifndef _POSIX_SOURCE
#define _POSIX_SOURCE
#endif

#include "core_audit_private.h"
#include "core_private.h"
#include "engine_private.h"
#include "rule_engine_private.h"

#include <ironbee/context.h>
#include <ironbee/context_selection.h>
#include <ironbee/field.h>
#include <ironbee/mpool.h>
#include <ironbee/string.h>
#include <ironbee/util.h>

#include <assert.h>
#include <inttypes.h>
#include <unistd.h>


/**
 * The structures below are used for by the core context selection.  There are
 * a number of non-obvious elements in these that are used to optimize the
 * site selection run-time:
 *
 * 1. The first element of each structure is the corresponding site structure
 *    (from ironbee/site.h).  These are the standard site family structures,
 *    but additional items (see #2 and #3, below) have been tacked on here
 *    which are specific to the core site selection and/or are added as
 *    optimizations.
 *
 * 2. The length of the strings in the standard site structures is cached in
 *    the ctxsel xxx_len element as an optimization.  The strings in the
 *    standard site structures are normal NUL-terminated strings, but having
 *    length cached allows the code to avoid determining the length during the
 *    site selection process.
 *
 * 3. The match_any field in the structure is also a cached element, and
 *    allows the selection to avoid looking at the other fields in the
 *    structure.
 *
 * Note that the code does not enforce that the last item in the lists be
 * a default; it is possible to create a configuration without a default site,
 * or with a default site in the middle of the list, or a default service /
 * location in the middle of the list, or even with multiple defaults.  Don't
 * do that.  If you do, the site selection will not do what you expect.
 */

/** Core context selection site structure */
typedef struct core_site_t {
    ib_site_t              site;         /**< Site data */
    ib_list_t             *hosts;        /**< List of core_host_t* */
    ib_list_t             *services;     /**< List of core_service_t* */
    ib_list_t             *locations;    /**< List of core_location_t* */
} core_site_t;

/** Core context selection host name entity */
typedef struct core_host_t {
    ib_site_host_t         host;         /** Site host data */
    size_t                 hostname_len; /** Length of full_hostname string */
    size_t                 suffix_len;   /** Length of suffix string */
    bool                   match_any;    /** Is this a 'match any' host? */
} core_host_t;

/** Core context selection site service entry */
typedef struct core_service_t {
    ib_site_service_t      service;      /**< Site service data */
    size_t                 ip_len;       /**< Length of IP address string */
    bool                   match_any;    /** Is this a 'match any' service? */
} core_service_t;

/** Core context selection site location data */
typedef struct core_location_t {
    ib_site_location_t     location;     /**< Site location data */
    size_t                 path_len;     /**< Length of path string */
    bool                   match_any;    /** Is this a 'match any' location? */
} core_location_t;

/** Core site selection data */
typedef struct core_site_selector_t {
    const core_site_t     *site;         /**< Pointer to the site */
    const core_service_t  *service;      /**< Service (IP/Port) */
    const ib_list_t       *hosts;        /**< List of core_host_t* */
    const ib_list_t       *locations;    /**< List of core_location_t* */
} core_site_selector_t;


/**
 * Find the first 'match any' location for the given site
 *
 * @param[in] site Site
 *
 * @returns First matching location or NULL
 */
static const core_location_t *core_ctxsel_matchany_location(
    const core_site_t *site)
{
    assert(site != NULL);
    const ib_list_node_t *node;

    if (site->locations == NULL) {
        return NULL;
    }

    IB_LIST_LOOP_CONST(site->locations, node) {
        const core_location_t *location = (const core_location_t *)node->data;
        if (location->match_any) {
            return location;
        }
    }

    return NULL;
}

/**
 * Check for a matching host within a host list
 *
 * This function takes a list of hosts, and attempts to find the first
 * host that matches the transaction.  This function is intended to aid in
 * the development of context selection functions, but it's use is optional.
 * It is used by the core site selector.
 *
 * @param[in] ib IronBee engine
 * @param[in] tx Transaction to match
 * @param[in] hosts List of ib_core_host_t
 * @param[out] match true if a match was found, else false
 *
 * @returns IB_OK
 */
static ib_status_t core_ctxsel_match_host(
    const ib_engine_t *ib,
    const ib_tx_t *tx,
    const ib_list_t *hosts,
    bool *match)
{
    assert(ib != NULL);
    assert(tx != NULL);
    assert(match != NULL);

    const ib_list_node_t *node;
    size_t len;

    /* If no hosts in the list, we have an automatic match */
    if (hosts == NULL) {
        *match = true;
        return IB_OK;
    }

    /* Now, loop through the list of hostnames */
    if (tx->hostname != NULL) {
        len = strlen(tx->hostname);
        IB_LIST_LOOP_CONST(hosts, node) {
            const core_host_t *core_host = (const core_host_t *)node->data;
            const ib_site_host_t *host = &(core_host->host);

            /* If this is a "match any" host entry? */
            if (core_host->match_any) {
                *match = true;
                return IB_OK;
            }

            /* Check the suffix */
            if ( (host->suffix != NULL) && (len >= core_host->suffix_len) ) {
                const char *suffix = tx->hostname + len - core_host->suffix_len;
                if (strcasecmp(host->suffix, suffix) == 0) {
                    *match = true;
                    return IB_OK;
                }
            }

            /* Finally, do a full hostname match */
            if ( (core_host->hostname_len == len) &&
                 (strcasecmp(host->hostname, tx->hostname) == 0) )
            {
                *match = true;
                return IB_OK;
            }
        }
    }

    /* No matches */
    *match = false;
    return IB_OK;
}

/**
 * Check for a matching location within a location list
 *
 * This function takes a list of locations, and attempts to find the first
 * location that matches the transaction.  This function is intended to aid in
 * the development of context selection functions, but it's use is optional.
 * It is used by the core site selector.
 *
 * @param[in] ib IronBee engine
 * @param[in] tx Transaction to match
 * @param[in] locations List of ib_ctxsel_location_t
 * @param[out] match Pointer to matched location (or NULL)
 *
 * @returns IB_OK
 */
static ib_status_t core_ctxsel_match_location(
    const ib_engine_t *ib,
    const ib_tx_t *tx,
    const ib_list_t *locations,
    const core_location_t **match)
{
    assert(ib != NULL);
    assert(tx != NULL);
    assert(locations != NULL);
    assert(match != NULL);

    const ib_list_node_t *node;
    size_t len;

    /* Now, loop through the list of locations */
    len = strlen(tx->path);
    IB_LIST_LOOP_CONST(locations, node) {
        const core_location_t *core_location =
            (const core_location_t *)node->data;
        const ib_site_location_t *location = &(core_location->location);

        /* Is this a "match any" location? */
        if (core_location->match_any) {
            *match = core_location;
            return IB_OK;
        }

        /* Check the path */
        if ( (core_location->path_len <= len) &&
             (strncmp(location->path, tx->path, core_location->path_len) == 0) )
        {
            *match = core_location;
            return IB_OK;
        }
    }

    /* No matches */
    *match = NULL;
    return IB_OK;
}

/**
 * Create a site selector object
 *
 * @param[in,out] core_data Core module data
 * @param[in] site Site object
 * @param[in] service Service object (can be NULL)
 * @param[out] selector New selector object (or NULL)
 *
 * @returns IB_OK or IB_EALLOC
 */
static ib_status_t core_create_site_selector(
    ib_core_module_data_t *core_data,
    const core_site_t *site,
    const core_service_t *service,
    core_site_selector_t **selector)
{
    assert(core_data != NULL);
    assert(site != NULL);

    ib_status_t rc;
    core_site_selector_t *object;

    /* Create & populate a site selector object */
    object = ib_mpool_alloc(site->site.mp, sizeof(*object));
    if (object == NULL) {
        return IB_EALLOC;
    }
    object->service = service;
    object->hosts = site->hosts;
    object->locations = site->locations;
    object->site = site;

    /* Add it to the site selector list */
    rc = ib_list_push(core_data->selector_list, object);
    if (rc != IB_OK) {
        return rc;
    }

    if (selector != NULL) {
        *selector = object;
    }
    return IB_OK;
}

/**
 * Finalize the core context selection.
 *
 * This functions creates the site selector list which is used during the site
 * selection process.  It walks through the list of sites / locations, and
 * creates corresponding site selector objects.
 *
 * @param[in] ib IronBee engine
 * @param[in] common_cb_data Common callback data
 * @param[in] fn_cb_data Function-specific callback data
 *
 * @returns Return status:
 *     IB_OK
 *     Errors returned by the various functions
 */
static ib_status_t core_ctxsel_finalize(
    const ib_engine_t *ib,
    void *common_cb_data,
    void *fn_cb_data)
{
    assert(ib != NULL);

    const ib_list_node_t *site_node;
    ib_core_module_data_t *core_data = (ib_core_module_data_t *)common_cb_data;
    ib_status_t rc;

    /* Do nothing if we're not the current site selector */
    if (ib_ctxsel_module_is_active(ib, ib_core_module(ib)) == false) {
        return IB_OK;
    }

    /* If there are no sites, do nothing */
    if (core_data->site_list == NULL) {
        ib_log_alert(ib, "No site list");
        return IB_OK;
    }
    else if (ib_list_elements(core_data->site_list) == 0) {
        ib_log_alert(ib, "No sites in core site list");
        return IB_OK;
    }

    /* Create the site selector list */
    if (core_data->selector_list == NULL) {
        rc = ib_list_create(&(core_data->selector_list), ib->mp);
        if (rc != IB_OK) {
            ib_log_error(ib, "Failed to create core site selector list: %s",
                         ib_status_to_string(rc));
            return rc;
        }
    }
    else {
        ib_list_clear(core_data->selector_list);
    }

    /* Build the site selector list from the site / location list */

    /* Walk through all of the sites, and it's locations & services */
    IB_LIST_LOOP_CONST(core_data->site_list, site_node) {
        const core_site_t *site = (const core_site_t *)site_node->data;
        const ib_list_node_t *service_node;

        /* If no services defined, just create a single selector with
         * a default service */
        if (site->services == NULL) {
            rc = core_create_site_selector(core_data, site, NULL, NULL);
            if (rc != IB_OK) {
                return rc;
            }
            continue;
        }

        /* Otherwise, loop through all of the services, create a single
         * selector for each */
        IB_LIST_LOOP_CONST(site->services, service_node) {
            const core_service_t *service =
                (const core_service_t *)service_node->data;
            rc = core_create_site_selector(core_data, site, service, NULL);
            if (rc != IB_OK) {
                return rc;
            }
        }
    }

    return IB_OK;
}

/**
 * Select the correct context for a connection / transaction.
 *
 * @param[in] ib Engine
 * @param[in] conn Pointer to connection
 * @param[in] tx Pointer to transaction / NULL
 * @param[in] common_cb_data Common callback data
 * @param[in] fn_cb_data Function-specific callback data
 * @param[out] pctx Pointer to selected context
 *
 * @returns Status code
 */
static ib_status_t core_ctxsel_select(
    const ib_engine_t *ib,
    const ib_conn_t *conn,
    const ib_tx_t *tx,
    void *common_cb_data,
    void *fn_cb_data,
    ib_context_t **pctx)
{
    assert(ib != NULL);
    assert(conn != NULL);
    assert(common_cb_data != NULL);
    assert(pctx != NULL);

    const ib_list_node_t *node;
    size_t ip_len;
    ib_status_t rc;
    ib_core_module_data_t *core_data = (ib_core_module_data_t *)common_cb_data;

    /* Verify that we're the current selector */
    if (ib_ctxsel_module_is_active(ib, ib_core_module(ib)) == false) {
        return IB_EINVAL;
    }

    if (core_data->selector_list == NULL) {
        ib_log_alert(ib, "No site selection list: Using main context");
        goto select_main_context;
    }

    /* Get the length of the IP address string before the main loop */
    ip_len = strlen(conn->local_ipstr);

    /*
     * Walk through the list of site selectors, return when the first matching
     * selector is found.  At any point in the loop if a non-match is found,
     * we continue to the top of the loop, and try the next selector.
     */
    IB_LIST_LOOP_CONST(core_data->selector_list, node) {
        const core_site_selector_t *selector =
            (const core_site_selector_t *)node->data;
        const core_service_t *service = selector->service;
        const core_site_t *site = selector->site;
        const core_location_t *location;
        ib_context_t *ctx;
        const char *ctx_type;
        bool match;

        /*
         * If there is no service or it's a "match any", match is automatic.
         */
        if ( (service != NULL) && (! service->match_any) ) {
            /* Check that the port matches the service (if specified) */
            if ( (service->service.port >= 0) &&
                 (service->service.port != conn->local_port) ) {
                continue;
            }
            /* Check that the host name matches the service (if specified) */
            if ( (service->service.ipstr != NULL) &&
                 (service->ip_len == ip_len) &&
                 (strcmp(service->service.ipstr, conn->local_ipstr) != 0) )
            {
                continue;
            }
        }

        /*
         * If we're looking for a connection context, there is no hostname or
         * location, so go with this selector.
         */
        if (tx == NULL) {
            ctx = selector->site->site.context;
            ctx_type = "site";
            goto found;
        }

        /* Check if the hostname matches */
        rc = core_ctxsel_match_host(ib, tx, selector->hosts, &match);
        if (rc != IB_OK) {
            /* todo: What is the right thing to do here? */
            continue;
        }
        if (! match) {
            continue;
        }

        /* Check if the location matches */
        rc = core_ctxsel_match_location(ib, tx, selector->locations, &location);
        if (rc != IB_OK) {
            /* todo: What is the right thing to do here? */
            continue;
        }
        if (location == NULL) {
            continue;
        }

        /* Everything matches.  Use this selector's context */
        ctx = location->location.context;
        ctx_type = "location";

  found:
        ib_log_debug2(ib, "Selected %s context %p \"%s\" site=%s(%s)",
                      ctx_type, ctx, ib_context_full_get(ctx),
                      (site ? site->site.id_str : "none"),
                      (site ? site->site.name : "none"));
        *pctx = ctx;
        return IB_OK;
    }

    /*
     * If we get here, we've exhausted the list of selectors, with no matching
     * selector found
     */
    if (tx == NULL) {
        ib_log_debug(ib, "No matching site found for connection:"
                     " IP=%s port=%u",
                     conn->local_ipstr, conn->local_port);
    }
    else {
        ib_log_notice(ib, "No matching site found for transaction:"
                      " IP=%s port=%u host=\"%s\"",
                      conn->local_ipstr, conn->local_port, tx->hostname);
    }

select_main_context:
    *pctx = ib_context_main(ib);

    return IB_OK;
}

/**
 * Core context selection: Create Site Function
 *
 * @param[in] ctx Site's configuration context
 * @param[in] name Site name
 * @param[in] common_cb_data Callback data passed to all registered fns
 * @param[in] fn_cb_data Function-specific callback data
 * @param[out] psite Address where site will be written / NULL
 *
 * @returns Status code
 */
static ib_status_t core_ctxsel_site_create(
    ib_context_t *ctx,
    const char *name,
    void *common_cb_data,
    void *fn_cb_data,
    ib_site_t **psite)
{
    assert(ctx != NULL);
    assert(ctx->ctype == IB_CTYPE_SITE);
    assert(name != NULL);

    ib_status_t rc;
    core_site_t *core_site;
    ib_site_t *site;
    ib_core_module_data_t *core_data = (ib_core_module_data_t *)common_cb_data;

    if (psite != NULL) {
        *psite = NULL;
    }

    core_site = ib_mpool_calloc(ctx->mp, sizeof(*core_site), 1);
    if (core_site == NULL) {
        return IB_EALLOC;
    }
    site = &(core_site->site);
    rc = ib_site_create(ctx, name, core_site, site, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* Create the locations list
     * The host and service lists are created as required */
    rc = ib_list_create(&(core_site->locations), site->mp);
    if (rc != IB_OK) {
        return rc;
    }

    /* Add the context selection site struct to the core site list */
    rc = ib_list_push(core_data->site_list, core_site);
    if (rc != IB_OK) {
        return rc;
    }

    if (psite != NULL) {
        *psite = site;
    }
    return IB_OK;
}

/**
 * Core context selection: Create location function
 *
 * @param[in] site Parent site
 * @param[in] ctx Locations' configuration context
 * @param[in] location_str Location string (path)
 * @param[in] common_cb_data Callback data passed to all registered fns
 * @param[in] fn_cb_data Function-specific callback data
 * @param[out] plocation Address where location will be written / NULL
 *
 * @returns Status code
 */
static ib_status_t core_ctxsel_location_create(
    const ib_site_t *site,
    ib_context_t *ctx,
    const char *location_str,
    void *common_cb_data,
    void *fn_cb_data,
    ib_site_location_t **plocation)
{
    assert(ctx != NULL);
    assert(ctx->ctype == IB_CTYPE_LOCATION);
    assert(site != NULL);
    assert(location_str != NULL);

    core_location_t *core_location;
    ib_site_location_t *site_location;
    core_site_t *core_site = (core_site_t *)(site->ctxsel_site);
    ib_status_t rc;

    if (plocation != NULL) {
        *plocation = NULL;
    }

    /* Create the location structure in the site memory pool */
    core_location = (core_location_t *)
        ib_mpool_alloc(site->mp, sizeof(*core_location));
    if (core_location == NULL) {
        return IB_EALLOC;
    }
    site_location = (ib_site_location_t *)&(core_location->location);

    /* Initialize the site location */
    rc = ib_site_location_create(site, ctx, location_str, core_location,
                                 site_location, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* Fill in the context selection specific parts */
    core_location->path_len = strlen(location_str);
    core_location->match_any = (strcmp(location_str, "/") == 0);

    /* And, add it to the locations list */
    rc = ib_list_push(core_site->locations, core_location);
    if (rc != IB_OK) {
        return rc;
    }

    /* Done */
    if (plocation != NULL) {
        *plocation = site_location;
    }
    return IB_OK;
}

/**
 * Core context selection: Host create function
 *
 * @param[in] site Parent site
 * @param[in] host_str Host string (host name)
 * @param[in] common_cb_data Callback data passed to all registered fns
 * @param[in] fn_cb_data Function-specific callback data
 * @param[out] phost Address where host will be written / NULL
 *
 * @returns Status code
 */
static ib_status_t core_ctxsel_host_create(
    const ib_site_t *site,
    const char *host_str,
    void *common_cb_data,
    void *fn_cb_data,
    ib_site_host_t **phost)
{
    assert(site != NULL);
    assert(host_str != NULL);

    ib_status_t rc;
    core_host_t *core_host;
    ib_site_host_t *site_host;
    core_site_t *core_site = (core_site_t *)(site->ctxsel_site);

    if (phost != NULL) {
        *phost = NULL;
    }

    /* Create a host object */
    core_host = ib_mpool_alloc(site->mp, sizeof(*core_host));
    if (core_host == NULL) {
        return IB_EALLOC;
    }
    site_host = (ib_site_host_t *)&(core_host->host);

    /* Initialize the site host object */
    rc = ib_site_host_create(site, host_str, core_host, site_host, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* Fill in the core context selection specific parts */
    core_host->hostname_len = strlen(host_str);
    if (site_host->suffix != NULL) {
        core_host->suffix_len = strlen(site_host->suffix);
    }
    else {
        core_host->suffix_len = 0;
    }
    core_host->match_any = (strcmp(host_str, "*") == 0);

    /* Create the hostname list if this is the first service. */
    if (core_site->hosts == NULL) {
        rc = ib_list_create(&(core_site->hosts), site->mp);
        if (rc != IB_OK) {
            return rc;
        }
    }

    /* Add the host to the list */
    rc = ib_list_push(core_site->hosts, core_host);
    if (rc != IB_OK) {
        return rc;
    }

    if (phost != NULL) {
        *phost = site_host;
    }
    return IB_OK;
}

/**
 * Core context selection: Service create function
 *
 * @param[in] site Parent site
 * @param[in] service_str Service string
 * @param[in] common_cb_data Callback data passed to all registered fns
 * @param[in] fn_cb_data Function-specific callback data
 * @param[out] pservice Address where service will be written / NULL
 *
 * @returns Status code
 */
static ib_status_t core_ctxsel_service_create(
    const ib_site_t *site,
    const char *service_str,
    void *common_cb_data,
    void *fn_cb_data,
    ib_site_service_t **pservice)
{
    assert(site != NULL);
    assert(service_str != NULL);

    ib_status_t rc;
    core_site_t *core_site = (core_site_t *)site->ctxsel_site;
    ib_site_service_t *service;
    core_service_t *core_service;

    core_service = ib_mpool_alloc(site->mp, sizeof(*core_service));
    if (core_service == NULL) {
        return IB_EALLOC;
    }
    service = &(core_service->service);

    /* Create to the site service */
    rc = ib_site_service_create(site, service_str, core_service,
                                service, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* Fill in our pieces of it */
    if (service->ipstr == NULL) {
        core_service->ip_len = 0;
    }
    else {
        core_service->ip_len = strlen(service->ipstr);
    }
    core_service->match_any =
        ( (core_service->ip_len == 0) && (service->port < 0) );

    /* Create the services list if this is the first service. */
    if (core_site->services == NULL) {
        rc = ib_list_create(&(core_site->services), site->mp);
        if (rc != IB_OK) {
            return rc;
        }
    }

    /* Finally, push the new service onto the core site service list */
    rc = ib_list_push(core_site->services, core_service);
    if (rc != IB_OK) {
        return rc;
    }

    /* Done */
    if (pservice != NULL) {
        *pservice = service;
    }
    return IB_OK;
}

/**
 * Core context selection: Site open
 *
 * @param[in] ib IronBee engine
 * @param[in] site Site object to open
 * @param[in] common_cb_data Common callback data
 * @param[in] fn_cb_data Function-specific callback data
 *
 * @returns Status code:
 *  - IB_OK
 *  - Errors returned from ib_list_push()
 */
static ib_status_t core_ctxsel_site_open(
    const ib_engine_t *ib,
    ib_site_t *site,
    void *common_cb_data,
    void *fn_cb_data)
{
    assert(ib != NULL);
    assert(common_cb_data != NULL);

    return IB_OK;
}

/**
 * Core context selection: Site close
 *
 * @param[in] ib IronBee engine
 * @param[in] site Site object to close
 * @param[in] common_cb_data Common callback data
 * @param[in] fn_cb_data Function-specific callback data
 *
 * @returns Status code:
 *  - IB_OK
 */
static ib_status_t core_ctxsel_site_close(
    const ib_engine_t *ib,
    ib_site_t *site,
    void *common_cb_data,
    void *fn_cb_data)
{
    assert(ib != NULL);
    assert(common_cb_data != NULL);

    core_site_t *core_site = (core_site_t *)site->ctxsel_site;
    ib_site_location_t *location;
    ib_context_t *ctx;
    const char *path = "/";
    ib_status_t rc;

    /* If there's already match-any location for this site, do nothing. */
    if (core_ctxsel_matchany_location(core_site) != NULL) {
        return IB_OK;
    }

    /* Create the match-any location's context */
    rc = ib_context_create((ib_engine_t *)ib, site->context,
                           IB_CTYPE_LOCATION, "location", "/", &ctx);
    if (rc != IB_OK) {
        return rc;
    }
    rc = ib_context_site_set(ctx, site);
    if (rc != IB_OK) {
        return rc;
    }

    /* Open the location context */
    rc = ib_context_open(ctx);
    if (rc != IB_OK) {
        return rc;
    }

    /* Create the location */
    rc = ib_ctxsel_location_create(site, ctx, path, &location);
    if (rc != IB_OK) {
        return rc;
    }

    /* Store the location in the context */
    rc = ib_context_location_set(ctx, location);
    if (rc != IB_OK) {
        return rc;
    }

    /* Open the location object */
    rc = ib_ctxsel_location_open(ib, location);
    if (rc != IB_OK) {
        return rc;
    }

    /* Close the location object */
    rc = ib_ctxsel_location_close(ib, location);
    if (rc != IB_OK) {
        return rc;
    }

    /* Close the location context */
    rc = ib_context_close(ctx);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

/**
 * Core context selection: Location open
 *
 * @param[in] ib IronBee engine
 * @param[in] location Location object to open
 * @param[in] common_cb_data Common callback data
 * @param[in] fn_cb_data Function-specific callback data
 *
 * @returns Status code:
 *  - IB_OK
 */
static ib_status_t core_ctxsel_location_open(
    const ib_engine_t *ib,
    ib_site_location_t *location,
    void *common_cb_data,
    void *fn_cb_data)
{
    assert(ib != NULL);
    assert(common_cb_data != NULL);

    return IB_OK;
}

/**
 * Core context selection: Location close
 *
 * @param[in] ib IronBee engine
 * @param[in] location Location object to close
 * @param[in] common_cb_data Common callback data
 * @param[in] fn_cb_data Function-specific callback data
 *
 * @returns Status code:
 *  - IB_OK
 */
static ib_status_t core_ctxsel_location_close(
    const ib_engine_t *ib,
    ib_site_location_t *location,
    void *common_cb_data,
    void *fn_cb_data)
{
    assert(ib != NULL);
    assert(common_cb_data != NULL);

    return IB_OK;
}

ib_status_t ib_core_ctxsel_init(ib_engine_t *ib,
                                ib_module_t *module)
{
    assert(ib != NULL);
    assert(module != NULL);
    ib_status_t rc;
    ib_core_module_data_t *core_data;
    ib_ctxsel_registration_t *registration = NULL;
    const char *failed = "unknown";

    /* Get core module data */
    rc = ib_core_module_data(ib, NULL, &core_data);
    if (rc != IB_OK) {
        return rc;
    }
    if (core_data == NULL) {
        failed = "NULL data";
        goto cleanup;
    }

    /* Create a registration object using malloc().  The core module data
     * (core_data) is passed as the common callback data to all of the
     * registered callback functions. */
    rc = ib_ctxsel_registration_create(NULL, module, core_data, &registration);
    if (rc != IB_OK) {
        failed = "create";
        goto cleanup;
    }

    /* The function specific data passed to all of the registered functions is
     * NULL. */

    /* Store the selection function */
    rc = ib_ctxsel_registration_store_select(
        registration, core_ctxsel_select, NULL);
    if (rc != IB_OK) {
        failed = "select";
        goto cleanup;
    }

    /* Store the site-create function */
    rc = ib_ctxsel_registration_store_site_create(
        registration, core_ctxsel_site_create, NULL);
    if (rc != IB_OK) {
        failed = "site create";
        goto cleanup;
    }

    /* Store the location-create function */
    rc = ib_ctxsel_registration_store_location_create(
        registration, core_ctxsel_location_create, NULL);
    if (rc != IB_OK) {
        failed = "location create";
        goto cleanup;
    }

    /* Store the host-create function */
    rc = ib_ctxsel_registration_store_host_create(
        registration, core_ctxsel_host_create, NULL);
    if (rc != IB_OK) {
        failed = "host create";
        goto cleanup;
    }

    /* Store the service-create function */
    rc = ib_ctxsel_registration_store_service_create(
        registration, core_ctxsel_service_create, NULL);
    if (rc != IB_OK) {
        failed = "service create";
        goto cleanup;
    }

    /* Store the site-open function */
    rc = ib_ctxsel_registration_store_site_open(
        registration, core_ctxsel_site_open, NULL);
    if (rc != IB_OK) {
        failed = "site open";
        goto cleanup;
    }

    /* Store the location-open function */
    rc = ib_ctxsel_registration_store_location_open(
        registration, core_ctxsel_location_open, NULL);
    if (rc != IB_OK) {
        failed = "location open";
        goto cleanup;
    }

    /* Store the site-open function */
    rc = ib_ctxsel_registration_store_site_close(
        registration, core_ctxsel_site_close, NULL);
    if (rc != IB_OK) {
        failed = "site close";
        goto cleanup;
    }

    /* Store the location-close function */
    rc = ib_ctxsel_registration_store_location_close(
        registration, core_ctxsel_location_close, NULL);
    if (rc != IB_OK) {
        failed = "location close";
        goto cleanup;
    }

    /* Store the finalize function */
    rc = ib_ctxsel_registration_store_finalize(
        registration, core_ctxsel_finalize, NULL);
    if (rc != IB_OK) {
        failed = "finalize";
        goto cleanup;
    }

    /* And, register them all */
    rc = ib_ctxsel_registration_register(ib, registration);
    if (rc != IB_OK) {
        failed = "registration";
        goto cleanup;
    }

cleanup:
    if (rc != IB_OK) {
        ib_log_error(ib, "Context selection registration failed @ %s: %s",
                     failed, ib_status_to_string(rc));
    }
    if (registration != NULL) {
        free(registration);
    }
    return rc;
}
