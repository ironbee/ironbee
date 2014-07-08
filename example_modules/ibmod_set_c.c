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
 * @brief IronBee --- Example Module: Set (C Version)
 *
 * This file is the C implementation of the Set example module.  There is
 * also a C++ (ibmod_set.cpp) implementation.
 *
 * @par Summary
 * This module provides set membership of named sets.  It is similar
 * to the `@match` and `@imatch` operators except that sets are defined
 * outside of rules via directives rather than inline as arguments to the
 * operator.  Defining sets via directives is superior when sets will be
 * reused across multiple rules.
 *
 * @par Operators
 * - `@set_match set` -- True iff input is in set named `set`.  Supports
 *   streaming and non-streaming rules as well as NULL input but does not
 *   capture.
 *
 * @par Directives
 * - `SetDefine set member1...` -- Create a case sensitive set named `set`
 *   with members given by later arguments.
 * - `SetDefineInsensitive set member1...` -- As `SetDefine` but case
 *   insensitive.
 * - `SetDefineFromFile set path` -- As `SetDefine` but members are read
 *   from file at `path`, one item per line.
 * - `SetDefineInsensitiveFromFile` -- As `SetDefineFromFile` but case
 *   insensitive.
 *
 * @par Configuration
 * - `Set set.debug 1` -- Turn on debugging information for the current
 *   context.  Will log every membership query.
 *
 * @par Note
 * The operator has access to all the sets defined in its context and any
 * ancestor context.  It does not have access to sets defined in other
 * contexts.  Similarly, it is an error to create a new set with the same
 * name as a set in current context or any ancestor context, but not an error
 * to create a set with the same name as a set in other contexts.
 *
 * @par C specific comments
 * - This implementation uses `ib_hash_t` with trivial (`(void *)1`) values
 *   as the underlying datastructure.  It uses memory pools to manage its
 *   state lifetime.
 * - The C API makes heavy use of callback functions.  All callbacks are a
 *   pair of a C function pointer and a `void *` pointer known as the
 *   "callback data".  The callback data is always passed to the callback
 *   functions as the final argument.  Callback data allows a single C
 *   function to be used in multiple callbacks, distinguished by the callback
 *   data, and allows data to be transmitted from the registration location
 *   to the execution location.  The C++ API makes heavy use of callback data
 *   to trampoline the C callbacks to C++ functionals.
 * - The C module definition code centers around carefully constructed static
 *   structures that are passed to the engine when the module loads.  This
 *   approach makes simple cases easy as demonstrated in this module.  More
 *   complex behavior, however, requires programmatic setup in the
 *   initialization function.
 * - Comprehensible errors, such as incorrect user usage, are handled.  Other
 *   errors are simply asserted.  They represent either a misunderstanding of
 *   the API or unrecoverable engine problems.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

/* See `man 7 feature_test_macros` on certain Linux flavors. */
#define _POSIX_C_SOURCE 200809L

/* IronBee has a canonical header order, exemplified in this module.  It is
 * not required for third party development.
 *
 * Headers are divided into sections, more or less:
 *
 * - The autoconf config file containing information about configure.
 * - For implementation files, the public header file.
 * - Any corresponding private header file.
 * - Headers for the framework the current file is party of.
 * - IronBee++
 * - IronBee
 * - Third party libraries, e.g., boost.
 * - Standard library.
 *
 * Within each section, includes are arranged alphabetically.
 *
 * The order is, more or less, specific to general and is arranged as such to
 * increase the chance of catching missing includes.
 */

#include <ironbee/context.h>
#include <ironbee/engine.h>
#include <ironbee/engine_state.h>
#include <ironbee/hash.h>
#include <ironbee/module.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef NDEBUG
#warning "NDEBUG is inappropriate.  Disabling."
#undef NDEBUG
#endif

/** Name of module */
#define MODULE_NAME set

/** MODULE_NAME as string */
#define MODULE_NAME_STR IB_XSTRINGIFY(MODULE_NAME)

/** The public module symbol */
IB_MODULE_DECLARE();

/**
 * Per-Configuration Context Data
 *
 * A @ref per_context_t will be created for each configuration context and
 * will hold module data specific to that context.  The first one will be
 * created as a copy of @ref c_per_context_initial.  Later ones will be
 * created as copies of the parent's @ref per_context_t.
 *
 * The function `ctx_open()` will be called at the beginning of  every
 * context.  It will create a new hash, copy the existing (parent's) @c sets
 * member into the new hash, and then set the @c sets member to the new hash.
 * In this way, each child will know of all the sets of its parent but any
 * sets it defines will not be added to the parents @c sets hash.
 **/
typedef struct {
    /**
     * Index of set by set name.
     *
     * Value type will be: `const ib_hash_t *`
     *
     * This hash, but not the hashes its values point to, will be duplicated
     * for children.  Thus children can access sets defined in parent contexts
     * but not those defined in sibling or child contexts.
     **/
    ib_hash_t *sets;

    /**
     * If 1, log queries.
     *
     * This member is an @ref ib_num_t in order to interact with the
     * configuration map code.  The configuration map code makes it easy for
     * module writers to expose members of their per-context data to the
     * configuration language.  However, doing so requires that those members
     * have types based on the field code.
     *
     * @sa field.h
     * @sa cfgmap.h
     **/
    ib_num_t debug;
} per_context_t;

/**
 * Per-Operator Instance data.
 *
 * Every time the `set_member` operator is used in a rule, operator_create()
 * will be called.  It will construct and populate one of these structures
 * which will then be stored by the engine.  When the rule is evaluated,
 * operator_execute() will be called and provided with the this structure.
 **/
typedef struct {
    /**
     * The set to check membership in.
     *
     * Values are invalid pointers and should be ignored.
     **/
    const ib_hash_t *set;

    /**
     * Whether to log queries.
     *
     * This member will be true iff per_context_t::debug is 1 for the context
     * the operator was created in at operator creation.
     **/
    bool debug;

    /**
     * Name of set.
     *
     * Used for query logging.
     **/
    const char *set_name;
} per_operator_t;

/**
 * @name Helper Functions
 *
 * These functions are used by other functions.
 */
/*@{*/

/**
 * Fetch per-context data.
 *
 * Helper function to fetch the per-context data for a context.
 *
 * @param[in] ctx Context to fetch for.
 * @return Per context data.
 **/
static
per_context_t *fetch_per_context(ib_context_t *ctx);

/**
 * Define a set.
 *
 * Helper function to define a set.  This function is intended to be called
 * as the final part one of the set defining directives.
 *
 * @param[in] cp               Configuration parser.  This parameter is used
 *                             to log errors in a manner that also reports the
 *                             configuration line that caused the error.  It
 *                             also provides access to the IronBee engine.
 * @param[in] case_insensitive If true, create a case insensitive set.
 * @param[in] directive_name   Name of directive defining set.  Used for
 *                             better log messages.
 * @param[in] set_name         Name of set to define.
 * @param[in] items            Items, as a list node.  Using a list node
 *                             rather than a list makes it easy to forward
 *                             the tail of a list of parameters.
 * @return
 * - IB_OK on success.
 * - IB_EOTHER on if a set with same name already exists.
 **/
static
ib_status_t define_set(
    ib_cfgparser_t       *cp,
    bool                  case_insensitive,
    const char           *directive_name,
    const char           *set_name,
    const ib_list_node_t *items
);

/*@}*/

/**
 * @name Callbacks
 *
 * These functions are called by the IronBee engine.
 */
/*@{*/

/**
 * Initialize module.
 *
 * Called at module initialization.  In this module we will initialize the
 * per-context data for the main context and tell the engine about the
 * operator.  These two items are all this module uses initialization for,
 * but other common uses are:
 *
 * - Register directives.  This module uses a directive map to register
 *   directives, but it could instead register them here, if, e.g., it wanted
 *   to set up complex callback data.
 * - Register per-context data.  This module has simple per-context data and
 *   can simply provide the initial value to IB_MODULE_INIT().  More complex
 *   modules could register the per-context data during initialization with
 *   ib_module_config_initialize().
 * - Register hook callbacks.  Modules can register callbacks to be called
 *   at state transitions as the engine processes traffic.
 * - Set up module state.
 *
 * @param[in] ib     IronBee engine.
 * @param[in] m      This module.
 * @param[in] cbdata Callback data; unused.
 * @return
 * - IB_OK on success.
 * - IB_EOTHER if an operator named @c set_member already exists.
 **/
static
ib_status_t init(
    ib_engine_t *ib,
    ib_module_t *m,
    void        *cbdata
);

/**
 * Handle @c SetDefine and @c SetDefineInsensitive directives.
 *
 * @param[in] cp     Configuration parser representing state of configuration
 *                   handling.  Can be used to access engine or report errors.
 * @param[in] name   Name of directive.
 * @param[in] params List of `const char *` representing parameters to
 *                   directive.
 * @param[in] cbdata Callback data; case insensitive iff non-NULL.
 * @return
 * - IB_OK on success.
 * - IB_EINVAL if less than two parameters provided.
 **/
static
ib_status_t dir_define(
    ib_cfgparser_t  *cp,
    const char      *name,
    const ib_list_t *params,
    void            *cbdata
);

/**
 * Handle @c SetDefineFromFile and @c SetDefineInsensitiveFromFile directives.
 *
 * @param[in] cp       Configuration parser representing state of
 *                     configuration handling.  Can be used to access engine
 *                     or report errors.
 * @param[in] name     Name of directive.
 * @param[in] set_name Name of set.  First parameter to directive.
 * @param[in] path     Path to file of items.  Second parameter to directive.
 * @param[in] cbdata Callback data; case insensitive iff non-NULL.
 * @return
 * - IB_OK on success.
 * - IB_EINVAL on file system error.
 **/
static
ib_status_t dir_define_from_file(
    ib_cfgparser_t *cp,
    const char     *name,
    const char     *set_name,
    const char     *path,
    void           *cbdata
);

/**
 * Handle creation of a @c set_member instance.
 *
 * This callback is called every time the @c set_member operator in
 * instantiated.  It is responsible for setting up the data needed to execute
 * the operator and returning a pointer to that data via @a instance_data.
 *
 * The canonical example of operator instantiation is when the operator is
 * is used in a rule.  However, there are other possibilities such as the
 * `operator` call in Predicate.
 *
 * It will create and set up a @ref per_operator_t.
 *
 * @param[in]  ctx           Configuration context of operator.
 * @param[in]  mm            Memory manager.
 * @param[in]  set_name      Name of set to check membership in.
 * @param[out] instance_data Instance data; will be a @ref per_operator_t.
 * @param[in]  cbdata        Callback data; not used.
 * @return IB_OK
 **/
static
ib_status_t operator_create(
    ib_context_t *ctx,
    ib_mm_t       mm,
    const char   *set_name,
    void         *instance_data,
    void         *cbdata
);

/**
 * Handle execution of a @c set_member instance.
 *
 * This callback is called when the @c set_member operator is executed.  It is
 * provided with the instance data produced by operator_create().
 *
 * It will interpret @a field as a bytestring and check for membership in the
 * set defined in @a instance_data and output whether a match is found to
 * @a result.
 *
 * @param[in]  tx            Current transaction.
 * @param[in]  field         Input to operator.
 * @param[in]  capture       Collection to store captured data in.
 *                           @c set_member does not support capture and
 *                           ignores this parameter.  It can be used to store
 *                           output beyond the result.
 * @param[out] result        Result of operator.  1 = true, 0 = false.
 * @param[in]  instance_data Instance data produced by operator_create().
 * @param[in]  cbdata        Callback data; ignored.
 * @return
 * - IB_OK on success.
 * - IB_EINVAL if @a field is not a bytestring field.
 **/
static
ib_status_t operator_execute(
    ib_tx_t *tx,
    const ib_field_t *field,
    ib_field_t *capture,
    ib_num_t *result,
    void *instance_data,
    void *cbdata
);

/**
 * Called at open of every configuration context.
 *
 * This callback is called at the beginning of every configuration context
 * during configuration parsing.  This module uses it to set up the
 * per-context data.
 *
 * Note that, as modules are loaded after the main context is opened, this
 * function will never be called for the main context.  Per-context data for
 * the main context is handled in init().
 *
 * It will create a new hash for per_context_t::sets and copy the parent's
 * sets into it.
 *
 * @param[in] ib     IronBee engine.
 * @param[in] ctx    Current configuration context.
 * @param[in] state  Which state we entered.
 * @param[in] cbdata Callback data; unused.
 *
 * @return IB_OK
 **/
static
ib_status_t context_open(
    ib_engine_t  *ib,
    ib_context_t *ctx,
    ib_state_t    state,
    void         *cbdata
);

/*@}*/

/**
 * @name Initialization Statics
 *
 * These static variables are used to initialize the module.  They should
 * never be used to hold varying state, only to provide configuration.
 */
/*@{*/

/** Initial value for per-context data. */
static per_context_t c_per_context_initial = {
    NULL, /* sets */
    0     /* debug */
};

/**
 * Configuration map.
 *
 * The configuration map is a static variable that is provided to
 * IB_MODULE_INIT() to automatically connect fields of the per-context data
 * to configuration settings.  Settings can be set in configuration, e.g.,
 *
 * @code
 * Set set.debug 1
 * @endcode
 *
 * Configuration maps work through fields (see field.h) and thus require the
 * members they access to be field types.  Thus, per_context_t::debug is an
 * @ref ib_num_t instead of a @c bool.
 **/
static IB_CFGMAP_INIT_STRUCTURE(c_config_map) = {
    IB_CFGMAP_INIT_ENTRY(
        MODULE_NAME_STR ".debug",
        IB_FTYPE_NUM,
        per_context_t,
        debug
    ),
    IB_CFGMAP_INIT_LAST
};

/**
 * Directive map.
 *
 * The directive map is a static variable that is provided to IB_MODULE_INIT()
 * to automatically register directives.  It is also possible to register
 * directive during module initialization via ib_config_register_directive().
 * This latter approach is useful, e.g., if complex callback data is needed.
 *
 * The use of `(void *)1` below is used to indicate case insensitivity, i.e.,
 * case insensitive iff callback data is non-NULL.
 **/
static IB_DIRMAP_INIT_STRUCTURE(c_directive_map) = {
    IB_DIRMAP_INIT_LIST(
        "SetDefine",
        dir_define, NULL
    ),
    IB_DIRMAP_INIT_LIST(
        "SetDefineInsensitive",
        dir_define, (void *)1
    ),
    IB_DIRMAP_INIT_PARAM2(
        "SetDefineFromFile",
        dir_define_from_file, NULL
    ),
    IB_DIRMAP_INIT_PARAM2(
        "SetDefineInsensitiveFromFile",
        dir_define_from_file, (void *)1
    ),
    IB_DIRMAP_INIT_LAST
};

/*@}*/

/**
 * Module initialization.
 *
 * This macro sets up the standard interface that IronBee uses to load
 * modules.  At minimum, it requires the module name and initialization
 * function.  In this module, we also provide information about the
 * per-context data, configuration map, directive map, and a context open
 * handler.
 **/
IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,                /* Default metadata   */
    MODULE_NAME_STR,                          /* Module name        */
    IB_MODULE_CONFIG(&c_per_context_initial), /* Per context data.  */
    c_config_map,                             /* Configuration map  */
    c_directive_map,                          /* Directive map      */
    init,         NULL,                       /* On initialize      */
    NULL,         NULL,                       /* On finish          */
);

/* Finished with declarations.  Remainder of file is definitions. */

/* Helpers Implementation */

static
per_context_t *fetch_per_context(ib_context_t  *ctx)
{
    assert(ctx != NULL);

    ib_status_t    rc;
    per_context_t *per_context = NULL;
    ib_module_t   *module      = NULL;

    rc = ib_engine_module_get(
        ib_context_get_engine(ctx),
        MODULE_NAME_STR,
        &module
    );
    assert(rc == IB_OK);

    rc = ib_context_module_config(ctx, module, &per_context);
    assert(rc == IB_OK);

    return per_context;
}

static
ib_status_t define_set(
    ib_cfgparser_t       *cp,
    bool                  case_insensitive,
    const char           *directive_name,
    const char           *set_name,
    const ib_list_node_t *items
)
{
    assert(cp             != NULL);
    assert(directive_name != NULL);
    assert(set_name       != NULL);
    assert(items          != NULL);

    ib_status_t    rc;
    ib_context_t  *ctx         = NULL;
    per_context_t *per_context = NULL;
    ib_hash_t     *set         = NULL;
    ib_mm_t        mm;

    mm = ib_engine_mm_main_get(cp->ib);

    rc = ib_cfgparser_context_current(cp, &ctx);
    assert(rc  == IB_OK);
    assert(ctx != NULL);

    per_context = fetch_per_context(ctx);
    assert(per_context       != NULL);
    assert(per_context->sets != NULL);

    rc = ib_hash_get(per_context->sets, NULL, set_name);
    if (rc == IB_OK) {
        ib_cfg_log_error(
            cp,
            "%s tried to define an already existent set: %s",
            directive_name,
            set_name
        );
        return IB_EOTHER;
    }
    assert(rc == IB_ENOENT);

    if (case_insensitive) {
        rc = ib_hash_create_nocase(&set, mm);
    }
    else {
        rc = ib_hash_create(&set, mm);
    }
    assert(rc  == IB_OK);
    assert(set != NULL);

    for (
        const ib_list_node_t *n = items;
        n != NULL;
        n = ib_list_node_next_const(n)
    ) {
        const char *item = ib_list_node_data_const(n);
        rc = ib_hash_set(set, ib_mm_strdup(mm, item), (void *)1);
        assert(rc == IB_OK);
    }

    rc = ib_hash_set(per_context->sets, set_name, set);
    assert(rc == IB_OK);

    return IB_OK;
}

/* Callbacks Implementation */

static
ib_status_t init(
    ib_engine_t *ib,
    ib_module_t *m,
    void        *cbdata
)
{
    assert(ib != NULL);
    assert(m  != NULL);

    ib_status_t    rc;
    per_context_t *per_context = NULL;
    ib_mm_t        mm;

    /* Set up main context data. */
    per_context = fetch_per_context(ib_context_main(ib));
    assert(per_context       != NULL);
    assert(per_context->sets == NULL);

    mm = ib_engine_mm_main_get(ib);

    rc = ib_hash_create(&per_context->sets, mm);
    assert(rc                == IB_OK);
    assert(per_context->sets != NULL);

    /* Register context open callback to handle per context data copying. */
    ib_hook_context_register(
        ib,
        context_open_state,
        context_open,  NULL
    );

    /* Register operator */
    rc = ib_operator_create_and_register(
        NULL,
        ib,
        "set_member",
        IB_OP_CAPABILITY_ALLOW_NULL,
        operator_create,  NULL,
        NULL,             NULL,
        operator_execute, NULL
    );
    if (rc == IB_EINVAL) {
        ib_log_error(ib, "Operator set_member already exists.  Double load?");
        return IB_EOTHER;
    }

    return IB_OK;
}

static
ib_status_t dir_define(
    ib_cfgparser_t  *cp,
    const char      *name,
    const ib_list_t *params,
    void            *cbdata
)
{
    assert(cp     != NULL);
    assert(name   != NULL);
    assert(params != NULL);

    bool        case_insensitive           = (cbdata != NULL);
    const       ib_list_node_t *param_node = NULL;
    const char *set_name                   = NULL;

    if (ib_list_elements(params) < 2) {
        ib_cfg_log_error(cp, "%s requires 2 or more arguments.", name);
        return IB_EINVAL;
    }

    param_node = ib_list_first_const(params);
    assert(param_node != NULL);
    set_name   = ib_list_node_data_const(param_node);
    param_node = ib_list_node_next_const(param_node);

    /* Forward to define_set() */
    return define_set(
        cp,
        case_insensitive,
        name,
        set_name,
        param_node
    );
}

static
ib_status_t dir_define_from_file(
    ib_cfgparser_t *cp,
    const char     *name,
    const char     *set_name,
    const char     *path,
    void           *cbdata
)
{
    assert(cp       != NULL);
    assert(name     != NULL);
    assert(set_name != NULL);
    assert(path     != NULL);

    ib_status_t  rc;
    bool         case_insensitive = (cbdata != NULL);
    FILE        *fp               = NULL;
    char        *buffer           = NULL;
    size_t       buffer_size      = 0;
    ib_list_t   *items            = NULL;
    ib_mm_t      mm;

    mm = ib_engine_mm_main_get(cp->ib);

    fp = fopen(path, "r");
    if (fp == NULL) {
        ib_cfg_log_error(
            cp,
            "%s unable to open file %s",
            name,
            path
        );
        return IB_EINVAL;
    }

    rc = ib_list_create(&items, mm);
    assert(rc    == IB_OK);
    assert(items != NULL);

    for (;;) {
        char *buffer_copy;
        int   read = getline(&buffer, &buffer_size, fp);

        if (read == -1) {
            if (! feof(fp)) {
                ib_cfg_log_error(
                    cp,
                    "%s had error reading from file %s: %d",
                    name,
                    path,
                    errno
                );
                fclose(fp);
                return IB_EINVAL;
            }
            else {
                break;
            }
        }

        buffer_copy = ib_mm_memdup(mm, buffer, read);
        assert(buffer_copy != NULL);
        while (buffer_copy[read-1] == '\n' || buffer_copy[read-1] == '\r') {
            buffer_copy[read-1] = '\0';
            --read;
        }

        rc = ib_list_push(items, (void *)buffer_copy);
        assert(rc == IB_OK);
    }

    fclose(fp);

    /* Forward to define_set() */
    return define_set(
        cp,
        case_insensitive,
        name,
        set_name,
        ib_list_first_const(items)
    );
}

static
ib_status_t operator_create(
    ib_context_t *ctx,
    ib_mm_t       mm,
    const char   *set_name,
    void         *instance_data,
    void         *cbdata
)
{
    assert(ctx           != NULL);
    assert(set_name      != NULL);
    assert(instance_data != NULL);

    ib_status_t          rc;
    const per_context_t *per_context  = NULL;
    const ib_hash_t     *set          = NULL;
    per_operator_t      *per_operator = NULL;

    per_context = fetch_per_context(ctx);
    assert(per_context != NULL);

    rc = ib_hash_get(per_context->sets, &set, set_name);
    assert(rc == IB_OK);
    assert(set != NULL);

    per_operator = ib_mm_alloc(mm, sizeof(*per_operator));
    assert(per_operator != NULL);

    per_operator->debug    = (per_context->debug != 0);
    per_operator->set      = set;
    per_operator->set_name = ib_mm_strdup(mm, set_name);
    assert(per_operator->set_name != NULL);

    *(per_operator_t **)instance_data = per_operator;

    return IB_OK;
}

ib_status_t operator_execute(
    ib_tx_t *tx,
    const ib_field_t *field,
    ib_field_t *capture,
    ib_num_t *result,
    void *instance_data,
    void *cbdata
)
{
    assert(tx            != NULL);
    assert(instance_data != NULL);
    assert(result        != NULL);

    ib_status_t           rc;
    const per_operator_t *per_operator = NULL;
    const ib_bytestr_t   *input        = NULL;

    per_operator = instance_data;
    assert(per_operator->set != NULL);

    if (field == NULL) {
        *result = 0;
        return IB_OK;
    }

    rc = ib_field_value_type(
        field,
        ib_ftype_bytestr_out(&input),
        IB_FTYPE_BYTESTR
    );
    if (rc == IB_EINVAL) {
        ib_log_error_tx(tx,
            "Input to set_member %s is not a bytestring.",
            per_operator->set_name
        );
        return IB_EINVAL;
    }
    assert(rc == IB_OK);

    rc = ib_hash_get_ex(
        per_operator->set,
        NULL,
        (const char *)ib_bytestr_const_ptr(input),
        ib_bytestr_length(input)
    );
    if (rc == IB_ENOENT) {
        *result = 0;
    }
    else {
        assert(rc == IB_OK);
        *result = 1;
    }

    if (per_operator->debug) {
        ib_log_info_tx(tx,
            "set_member %s for %.*s = %s",
            per_operator->set_name,
            (int)ib_bytestr_length(input),
            ib_bytestr_const_ptr(input),
            (*result == 1 ? "yes" : "no")
        );
    }

    return IB_OK;
}

static
ib_status_t context_open(
    ib_engine_t  *ib,
    ib_context_t *ctx,
    ib_state_t    state,
    void         *cbdata
)
{
    assert(ib     != NULL);
    assert(ctx    != NULL);
    assert(state  == context_open_state);
    assert(cbdata == NULL);

    ib_status_t         rc;
    ib_mm_t             mm;
    per_context_t      *per_context = NULL;
    const ib_hash_t    *parent_sets = NULL;
    ib_mm_t             temp_mm;
    ib_hash_iterator_t *iterator    = NULL;

    per_context = fetch_per_context(ctx);
    assert(per_context != NULL);

    mm = ib_context_get_mm(ctx);

    parent_sets = per_context->sets;
    assert(parent_sets != NULL);

    rc = ib_hash_create(&per_context->sets, mm);
    assert(rc                == IB_OK);
    assert(per_context->sets != NULL);

    temp_mm = ib_engine_mm_temp_get(ib);

    iterator = ib_hash_iterator_create(temp_mm);
    assert(iterator != NULL);

    for (
        ib_hash_iterator_first(iterator, parent_sets);
        ! ib_hash_iterator_at_end(iterator);
        ib_hash_iterator_next(iterator)
    ) {
        const char      *key        = NULL;
        size_t           key_length = 0;
        const ib_hash_t *set        = NULL;

        ib_hash_iterator_fetch(&key, &key_length, &set, iterator);

        assert(key != NULL);
        assert(set != NULL);

        rc = ib_hash_set_ex(
            per_context->sets,
            key, key_length,
            (void *)set
        );
        assert(rc == IB_OK);
    }

    return IB_OK;
}
