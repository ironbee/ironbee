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

#ifndef _IB_SITE_H_
#define _IB_SITE_H_

/**
 * @file
 * @brief IronBee --- Site functions
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/engine_types.h>
#include <ironbee/list.h>
#include <ironbee/uuid.h>

#include <stdarg.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeSite IronBee Site Functions
 * @ingroup IronBee
 *
 * This is the API for the IronBee site functions.
 *
 * @{
 */


/** Site Structure */
struct ib_site_t {
    char                id[IB_UUID_LENGTH]; /**< Site UUID */
    ib_mpool_t         *mp;                 /**< Memory pool */
    const char         *name;               /**< Site name */
    ib_context_t       *context;            /**< Site's configuration context */
    void               *ctxsel_site;        /**< Context selection site info */
};

/** Site host name entity */
struct ib_site_host_t {
    const ib_site_t    *site;            /**< Parent site object */
    const char         *hostname;        /**< The full hostname */
    const char         *suffix;          /**< Suffix of wildcarded host name */
    void               *ctxsel_host;     /**< Context selection host info */
};

/** Site service entry */
struct ib_site_service_t {
    const ib_site_t    *site;            /**< Parent site object */
    const char         *ipstr;           /**< IP address / NULL */
    int                 port;            /**< Port number / -1 */
    void               *ctxsel_service;  /**< Context selection service info */
};

/** Site location data */
struct ib_site_location_t {
    const ib_site_t    *site;            /**< Parent site */
    const char         *path;            /**< Location path */
    ib_context_t       *context;         /**< The associated location ctx */
    void               *ctxsel_location; /**< Context selection location info */
};

/**
 * Create a site and add it to the context's site list
 *
 * @param[in,out] ctx Site's configuration context
 * @param[in] name Site name
 * @param[in] ctxsel_site Context selection site info
 * @param[in] site Pre-allocated site structure to use / NULL
 * @param[out] psite Address where site will be written / NULL
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_site_create(
    ib_context_t *ctx,
    const char *name,
    void *ctxsel_site,
    ib_site_t *site,
    ib_site_t **psite);

/**
 * Close a site.
 *
 * @param[in,out] site Site to close
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_site_close(
    ib_site_t *site);

/**
 * Create a site location object
 *
 * @param[in] site Parent site
 * @param[in] ctx Location's context
 * @param[in] location_str Location string (path)
 * @param[in] ctxsel_location Context-selection specific location
 * @param[in] location Pre-allocated location structure to use / NULL
 * @param[out] plocation Address where location will be written / NULL
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_site_location_create(
    const ib_site_t *site,
    ib_context_t *ctx,
    const char *location_str,
    void *ctxsel_location,
    ib_site_location_t *location,
    ib_site_location_t **plocation);

/**
 * Create a service object
 *
 * @param[in] site Parent site
 * @param[in] service_str Service string in the form of ip[:port]
 * @param[in] ctxsel_service Context-selection specific service
 * @param[in] service Pre-allocated service structure to use / NULL
 * @param[out] pservice Pointer to new service object / NULL
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_site_service_create(
    const ib_site_t *site,
    const char *service_str,
    void *ctxsel_service,
    ib_site_service_t *service,
    ib_site_service_t **pservice);

/**
 * Create a site host object
 *
 * @param[in] site Parent site
 * @param[in] hostname Hostname string to add
 * @param[in] ctxsel_host Context-selection specific host
 * @param[in] sitehost Pre-allocated site host structure to use / NULL
 * @param[out] psitehost Pointer to new site host object / NULL
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_site_host_create(
    const ib_site_t *site,
    const char *hostname,
    void *ctxsel_host,
    ib_site_host_t *sitehost,
    ib_site_host_t **psitehost);

/**
 * @} IronBeeSite
 * @} IronBee
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_ENGINE_H_ */
