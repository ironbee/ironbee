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

#ifndef _IB_PROVIDER_H_
#define _IB_PROVIDER_H_

/**
 * @file
 * @brief IronBee --- Provider
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/engine.h>
#include <ironbee/log.h>
#include <ironbee/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeProvider Provider
 * @ingroup IronBee
 *
 * Providers allow for alternative implementation of specific functionality,
 * e.g., logging.
 *
 * @{
 */

/* -- Types -- */

/** Function called when a provider is registered. */
typedef ib_status_t (*ib_provider_register_fn_t)(ib_engine_t *ib,
                                                 ib_provider_t *pr);

/** Function called when a provider instance is created. */
typedef ib_status_t (*ib_provider_inst_init_fn_t)(ib_provider_inst_t *pi,
                                                  void *data);

struct ib_provider_def_t {
    ib_mpool_t                *mp;
    const char                *type;
    ib_provider_register_fn_t  fn_reg;
    void                      *api;
};

struct ib_provider_t {
    ib_engine_t               *ib;
    ib_mpool_t                *mp;
    const char                *type;
    void                      *data;
    void                      *iface;
    void                      *api;
    ib_provider_inst_init_fn_t fn_init;
};

struct ib_provider_inst_t {
    ib_mpool_t                *mp;
    ib_provider_t             *pr;
    void                      *data;
};


/* -- Function/Hook Routines -- */

#define IB_PROVIDER_FUNC(ret, func, param) ret (*func) param

#define IB_PROVIDER_MEMBER(type, name) type name

#define IB_PROVIDER_IMPLEMENT_HOOK_VOID \\ TODO: Implement

#define IB_PROVIDER_IMPLEMENT_HOOK_RUNALL \\ TODO: Implement

#define IB_PROVIDER_IMPLEMENT_HOOK_RUNONE \\ TODO: Implement


/* -- API Routines -- */

#define IB_PROVIDER_API_TYPE(name) ib_provider_API_##name##_t

#define IB_PROVIDER_DECLARE_API(name) \
typedef struct IB_PROVIDER_API_TYPE(name) IB_PROVIDER_API_TYPE(name); \
struct IB_PROVIDER_API_TYPE(name)

#define IB_PROVIDER_API_GET(pr, name) ((IB_PROVIDER_API_TYPE(name))((pr)->api))

#define ib_provider_api_call(pr, name, func, param) \
    IB_PROVIDER_API_GET(pr, name)->func##param


/* -- Interface Routines -- */

#define IB_PROVIDER_IFACE_TYPE(name) ib_provider_IFACE_##name##_t

#define IB_PROVIDER_DECLARE_IFACE(name) \
typedef struct IB_PROVIDER_IFACE_TYPE(name) IB_PROVIDER_IFACE_TYPE(name); \
struct IB_PROVIDER_IFACE_TYPE(name)

#define IB_PROVIDER_IFACE_HEADER \
    IB_PROVIDER_MEMBER(int, version)

#define IB_PROVIDER_IFACE_HEADER_DEFAULTS \
    0

#define ib_provider_iface_call(pr, name, func, param) \
    IB_PROVIDER_IFACE_GET(pr, name)->func##param


/* -- Provider Routines */

/**
 * Create a provider API definition.
 *
 * @param ib Engine
 * @param type Type of provider
 * @param fn_reg Registration function, executed when interface registered
 * @param api API definition
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_provider_define(ib_engine_t *ib,
                                          const char *type,
                                          ib_provider_register_fn_t fn_reg,
                                          void *api);

/**
 * Register an interface (implementation) to an existing API definition.
 *
 * @param ib Engine
 * @param type Type of provider being interfaced
 * @param key Unique key for interface lookup
 * @param ppr Location where provider is written (if non-NULL)
 * @param iface Interface definition
 * @param fn_init Initialization function, executed when instance created
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_provider_register(ib_engine_t *ib,
                                            const char *type,
                                            const char *key,
                                            ib_provider_t **ppr,
                                            void *iface,
                                            ib_provider_inst_init_fn_t fn_init);

/**
 * Lookup a registered provider.
 *
 * @param ib Engine
 * @param type Type of provider being interfaced
 * @param key Unique key for interface lookup
 * @param ppr Location where provider is written
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_provider_lookup(ib_engine_t *ib,
                                          const char *type,
                                          const char *key,
                                          ib_provider_t **ppr);

/**
 * Create an instance of a provider
 *
 * @param ib Engine
 * @param pr Provider being interfaced
 * @param ppi Location where provider instance is written
 * @param pool Pool to allocate instance
 * @param data Arbitrary data passed to init function or stored with instance
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_provider_instance_create_ex(ib_engine_t *ib,
                                                      ib_provider_t *pr,
                                                      ib_provider_inst_t **ppi,
                                                      ib_mpool_t *pool,
                                                      void *data);

/**
 * Create an instance of a registered provider by looking up with
 * the given type/key
 *
 * @param ib Engine
 * @param type Type of provider being interfaced
 * @param key Unique key for interface lookup
 * @param ppi Location where provider instance is written
 * @param pool Pool to allocate instance
 * @param data Arbitrary data passed to init function or stored with instance
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_provider_instance_create(ib_engine_t *ib,
                                                   const char *type,
                                                   const char *key,
                                                   ib_provider_inst_t **ppi,
                                                   ib_mpool_t *pool,
                                                   void *data);

/**
 * Get data stored in a provider.
 *
 * @param pr Provider
 *
 * @returns Data
 */
void DLL_PUBLIC *ib_provider_data_get(ib_provider_t *pr);

/**
 * Store arbitrary data with the provider.
 *
 * @param pr Provider
 * @param data Data to set
 */
void DLL_PUBLIC ib_provider_data_set(ib_provider_t *pr, void *data);


/* -- Builtin Provider Interface/API Definitions -- */

/* Audit */
#define IB_PROVIDER_TYPE_AUDIT     IB_XSTRINGIFY(audit)
#define IB_PROVIDER_VERSION_AUDIT  0

/** Audit Interface Definition. */
IB_PROVIDER_DECLARE_IFACE(audit) {
    IB_PROVIDER_IFACE_HEADER;
    IB_PROVIDER_FUNC(
        ib_status_t,
        open,
        (ib_provider_inst_t *pi, ib_auditlog_t *log)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        write_header,
        (ib_provider_inst_t *pi, ib_auditlog_t *log)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        write_part,
        (ib_provider_inst_t *pi, ib_auditlog_part_t *part)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        write_footer,
        (ib_provider_inst_t *pi, ib_auditlog_t *log)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        close,
        (ib_provider_inst_t *pi, ib_auditlog_t *log)
    );
};

/** Audit API Definition. */
IB_PROVIDER_DECLARE_API(audit) {
    /* ib_status_t write_log(pi) */
    IB_PROVIDER_FUNC(
        ib_status_t,
        write_log,
        (ib_provider_inst_t *pi)
    );
};

/**
 * Get the audit provider instance within a configuration context.
 *
 * @param ctx Config context
 *
 * @returns The audit provider within the given context
 */
ib_provider_inst_t DLL_PUBLIC *ib_audit_provider_get_instance(ib_context_t *ctx);

/**
 * Set the audit provider instance within a configuration context.
 *
 * @param ctx Config context
 * @param lpi Audit log provider instance
 */
void DLL_PUBLIC ib_audit_provider_set_instance(ib_context_t *ctx,
                                               ib_provider_inst_t *lpi);


/* Parser */
#define IB_PROVIDER_TYPE_PARSER     IB_XSTRINGIFY(parser)
#define IB_PROVIDER_VERSION_PARSER  0

/** Parser Interface Definition. */
IB_PROVIDER_DECLARE_IFACE(parser) {
    IB_PROVIDER_IFACE_HEADER;

    /* Connection Initialization/Cleanup Functions */
    IB_PROVIDER_FUNC(
        ib_status_t,
        conn_init,
        (ib_provider_inst_t *pi, ib_conn_t *conn)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        conn_cleanup,
        (ib_provider_inst_t *pi, ib_conn_t *conn)
    );

    /* Connect/Disconnect Functions */
    IB_PROVIDER_FUNC(
        ib_status_t,
        connect,
        (ib_provider_inst_t *pi, ib_conn_t *conn)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        disconnect,
        (ib_provider_inst_t *pi, ib_conn_t *conn)
    );

    /* Transaction Initialization/Cleanup Function */
    IB_PROVIDER_FUNC(
        ib_status_t,
        tx_init,
        (ib_provider_inst_t *pi, ib_tx_t *tx)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        tx_cleanup,
        (ib_provider_inst_t *pi, ib_tx_t *tx)
    );

    /* Preparsed Request Data Functions */
    IB_PROVIDER_FUNC(
        ib_status_t,
        request_started,
        (ib_provider_inst_t *pi, ib_tx_t *tx)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        request_line,
        (ib_provider_inst_t *pi, ib_tx_t *tx, ib_parsed_req_line_t *line)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        request_header_data,
        (ib_provider_inst_t *pi, ib_tx_t *tx, ib_parsed_header_wrapper_t *header)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        request_header_finished,
        (ib_provider_inst_t *pi, ib_tx_t *tx)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        request_body_data,
        (ib_provider_inst_t *pi, ib_tx_t *tx, ib_txdata_t *txdata)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        request_finished,
        (ib_provider_inst_t *pi, ib_tx_t *tx)
    );

    /* Preparsed Response Data Functions */
    IB_PROVIDER_FUNC(
        ib_status_t,
        response_started,
        (ib_provider_inst_t *pi, ib_tx_t *tx)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        response_line,
        (ib_provider_inst_t *pi, ib_tx_t *tx, ib_parsed_resp_line_t *line)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        response_header_data,
        (ib_provider_inst_t *pi, ib_tx_t *tx, ib_parsed_header_wrapper_t *header)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        response_header_finished,
        (ib_provider_inst_t *pi, ib_tx_t *tx)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        response_body_data,
        (ib_provider_inst_t *pi, ib_tx_t *tx, ib_txdata_t *txdata)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        response_finished,
        (ib_provider_inst_t *pi, ib_tx_t *tx)
    );

    // TODO: Need to be able to hook into parser events
    // TODO: Need to handle delayed (on demand) field generation
};

/**
 * Get the parser provider within a configuration context.
 *
 * @param ctx Config context
 *
 * @returns The parser provider within the given context
 */
ib_provider_inst_t DLL_PUBLIC *ib_parser_provider_get_instance(ib_context_t *ctx);

/**
 * Set the parser provider within a configuration context.
 *
 * @param ctx Config context
 * @param ppi Parser provider instance
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_parser_provider_set_instance(ib_context_t *ctx,
                                                       ib_provider_inst_t *ppi);

/**
 * @} IronBeeProvider
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_PROVIDER_H_ */
