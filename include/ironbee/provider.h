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

#ifndef _IB_PROVIDER_H_
#define _IB_PROVIDER_H_

/**
 * @file
 * @brief IronBee &mdash; Provider
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/types.h>
#include <ironbee/engine.h>

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

/* Logger */
#define IB_PROVIDER_TYPE_LOGGER     IB_XSTRINGIFY(logger)
#define IB_PROVIDER_VERSION_LOGGER  0

/** Logger Interface Definition. */
IB_PROVIDER_DECLARE_IFACE(logger) {
    IB_PROVIDER_IFACE_HEADER;
    IB_PROVIDER_MEMBER(ib_log_logger_fn_t, logger);
};

/** Logger API Definition. */
IB_PROVIDER_DECLARE_API(logger) {
    /* void vlogmsg(pi, ctx, level, tx, prefix, file, line, fmt, ap) */
    IB_PROVIDER_FUNC(
        void,
        vlogmsg,
        (ib_provider_inst_t *pi, ib_context_t *ctx,
         int level, const ib_tx_t* tx, const char *prefix,
         const char *file, int line, const char *fmt, va_list ap)
         VPRINTF_ATTRIBUTE(8)
    );
    /* void logmsg(pi, ctx, level, tx, prefix, file, line, fmt, ...) */
    IB_PROVIDER_FUNC(
        void,
        logmsg,
        (ib_provider_inst_t *pi, ib_context_t *ctx,
         int level, const ib_tx_t *tx,const char *prefix,
         const char *file, int line, const char *fmt, ...)
         VPRINTF_ATTRIBUTE(8)
    );
};

/**
 * Get the log provider instance within a configuration context.
 *
 * @param ctx Config context
 *
 * @returns The log provider within the given context
 */
ib_provider_inst_t DLL_PUBLIC *ib_log_provider_get_instance(ib_context_t *ctx);

/**
 * Set the log provider instance within a configuration context.
 *
 * @param ctx Config context
 * @param lpi Log provider instance
 */
void DLL_PUBLIC ib_log_provider_set_instance(ib_context_t *ctx,
                                             ib_provider_inst_t *lpi);


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
    IB_PROVIDER_FUNC(
        ib_status_t,
        init,
        (ib_provider_inst_t *pi, ib_conn_t *conn)
    );
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
    IB_PROVIDER_FUNC(
        ib_status_t,
        data_in,
        (ib_provider_inst_t *pi, ib_conndata_t *cdata)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        data_out,
        (ib_provider_inst_t *pi, ib_conndata_t *cdata)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        gen_request_header_fields,
        (ib_provider_inst_t *pi, ib_tx_t *tx)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        gen_response_header_fields,
        (ib_provider_inst_t *pi, ib_tx_t *tx)
    );
    /// @todo Need to be able to hook into parser events
    /// @todo Need to handle delayed (on demand) field generation
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
 */
void DLL_PUBLIC ib_parser_provider_set_instance(ib_context_t *ctx,
                                                ib_provider_inst_t *ppi);


/* Data */
#define IB_PROVIDER_TYPE_DATA     IB_XSTRINGIFY(data)
#define IB_PROVIDER_VERSION_DATA  0

/** Data Interface Definition. */
IB_PROVIDER_DECLARE_IFACE(data) {
    IB_PROVIDER_IFACE_HEADER;
    IB_PROVIDER_FUNC(
        ib_status_t,
        add,
        (ib_provider_inst_t *pi, ib_field_t *f, const char *name, size_t nlen)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        set,
        (ib_provider_inst_t *pi, ib_field_t *f, const char *name, size_t nlen)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        set_relative,
        (ib_provider_inst_t *pi, const char *name, size_t nlen, intmax_t adjval)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        get,
        (ib_provider_inst_t *pi, const char *name, size_t nlen, ib_field_t **pf)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        get_all,
        (ib_provider_inst_t *pi, ib_list_t *list)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        remove,
        (ib_provider_inst_t *pi, const char *name, size_t nlen, ib_field_t **pf)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        clear,
        (ib_provider_inst_t *pi)
    );
    /// @todo init(table) add fields in bulk
};

/** Data API Definition. */
IB_PROVIDER_DECLARE_API(data) {
    IB_PROVIDER_FUNC(
        ib_status_t,
        add,
        (ib_provider_inst_t *pi, ib_field_t *f, const char *name, size_t nlen)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        set,
        (ib_provider_inst_t *pi, ib_field_t *f, const char *name, size_t nlen)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        set_relative,
        (ib_provider_inst_t *pi, const char *name, size_t nlen, intmax_t adjval)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        get,
        (ib_provider_inst_t *pi, const char *name, size_t nlen, ib_field_t **pf)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        get_all,
        (ib_provider_inst_t *pi, ib_list_t *list)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        remove,
        (ib_provider_inst_t *pi, const char *name, size_t nlen, ib_field_t **pf)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        clear,
        (ib_provider_inst_t *pi)
    );
    /// @todo init
};


/* Matcher */
#define IB_PROVIDER_TYPE_MATCHER     IB_XSTRINGIFY(matcher)
#define IB_PROVIDER_VERSION_MATCHER  0

/** Matcher Interface Definition. */
IB_PROVIDER_DECLARE_IFACE(matcher) {
    IB_PROVIDER_IFACE_HEADER;

    /* Provider Interface */
    IB_PROVIDER_FUNC(
        ib_status_t,
        compile,
        (ib_provider_t *mpr, ib_mpool_t *pool,
         void *pcpatt, const char *patt,
         const char **errptr, int *erroffset)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        match_compiled,
        (ib_provider_t *mpr, void *cpatt,
         ib_flags_t flags, const uint8_t *data, size_t dlen, void *ctx)
    );

    /* Provider instance Interface */
    IB_PROVIDER_FUNC(
        ib_status_t,
        add,
        (ib_provider_inst_t *pi, void *cpatt)
    );

    IB_PROVIDER_FUNC(
       ib_status_t,
       add_ex,
       (ib_provider_inst_t *mpi, void *patterns,
        const char *patt, ib_void_fn_t callback, void *arg,
        const char **errptr, int *erroffset)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        match,
        (ib_provider_inst_t *mpi,
         ib_flags_t flags, const uint8_t *data, size_t dlen, void *ctx)
    );
};

/** Matcher API Definition. */
IB_PROVIDER_DECLARE_API(matcher) {
    /* Provider API */
    IB_PROVIDER_FUNC(
        ib_status_t,
        compile_pattern,
        (ib_provider_t *mpr, ib_mpool_t *pool,
         void *pcpatt, const char *patt,
         const char **errptr, int *erroffset)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        match_compiled,
        (ib_provider_t *mpr, void *cpatt,
         ib_flags_t flags, const uint8_t *data, size_t dlen, void *ctx)
    );

    /* Provider Instance API */
    /// @todo Need to add an _ex version with match/nomatch funcs and cbdata
    IB_PROVIDER_FUNC(
        ib_status_t,
        add_pattern,
        (ib_provider_inst_t *mpi, const char *patt)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        add_pattern_ex,
        (ib_provider_inst_t *mpi, void *patterns, const char *patt,
         ib_void_fn_t callback, void *arg,
         const char **errptr, int *erroffset)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        match,
        (ib_provider_inst_t *mpi,
         ib_flags_t flags, const uint8_t *data, size_t dlen, void *ctx)
    );
};


/* Log Event */
#define IB_PROVIDER_TYPE_LOGEVENT     IB_XSTRINGIFY(logevent)
#define IB_PROVIDER_VERSION_LOGEVENT  0

/** Log Event Interface Definition. */
IB_PROVIDER_DECLARE_IFACE(logevent) {
    IB_PROVIDER_IFACE_HEADER;
    IB_PROVIDER_FUNC(
        ib_status_t,
        write,
        (ib_provider_inst_t *epi, ib_logevent_t *e)
    );
};

/** Log Event API Definition. */
IB_PROVIDER_DECLARE_API(logevent) {
    IB_PROVIDER_FUNC(
        ib_status_t,
        add_event,
        (ib_provider_inst_t *epi, ib_logevent_t *e)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        remove_event,
        /// @todo ID should be another type (structure?)
        (ib_provider_inst_t *epi, uint32_t id)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        fetch_events,
        (ib_provider_inst_t *epi, ib_list_t **pevents)
    );
    IB_PROVIDER_FUNC(
        ib_status_t,
        write_events,
        (ib_provider_inst_t *epi)
    );
};

/**
 * Get the logevent provider instance within a configuration context.
 *
 * @param ctx Config context
 *
 * @returns The logevent provider within the given context
 */
ib_provider_inst_t DLL_PUBLIC *ib_logevent_provider_get_instance(ib_context_t *ctx);

/**
 * Set the logevent provider instance within a configuration context.
 *
 * @param ctx Config context
 * @param lpi Logevent provider instance
 */
void DLL_PUBLIC ib_logevent_provider_set_instance(ib_context_t *ctx,
                                                  ib_provider_inst_t *lpi);



/**
 * @} IronBeeProvider
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_PROVIDER_H_ */
