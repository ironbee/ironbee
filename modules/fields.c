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

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <assert.h>

#include <ironbee/core.h>
#include <ironbee/cfgmap.h>
#include <ironbee/debug.h>
#include <ironbee/engine.h>
#include <ironbee/module.h>
#include <ironbee/util.h>
#include <ironbee/mpool.h>
#include <ironbee/bytestr.h>
#include <ironbee/list.h>
#include <ironbee/field.h>
#include <ironbee/config.h>


/* Define the module name as well as a string version of it. */
#define MODULE_NAME        fields
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

/* Declare the public module symbol. */
IB_MODULE_DECLARE();

/**
 * static data
 */
ib_list_t *g_field_list = NULL;  /**< Global list of all our fields */
const char *g_type_names [ ] =
{
    "GENERIC",
    "NUM",
    "UNUM",
    "NULSTR",
    "BYTESTR",
    "LIST",
};


/**
 * @param[in] cp Configuration parser
 * @param[in] mp Memory pool to use for allocations
 * @param[in] str String to parse as a type name
 * @param[out] type Field type
 * @param[out] list_type If @a type is list, the type of the elements in
 *                       the list, if specified, otherwise IB_FTYPE_GENERIC
 *
 * @return Status code
 */
static ib_status_t parse_type(ib_cfgparser_t *cp,
                              ib_mpool_t *mp,
                              const char *str,
                              ib_ftype_t *type,
                              ib_ftype_t *element_type)
{
    IB_FTRACE_INIT();

    /* Parse the type name */
    if (strcasecmp(str, "NUM") == 0) {
        *type = (ib_ftype_t)IB_FTYPE_NUM;
    }
    else if (strcasecmp(str, "UNUM") == 0) {
        *type = (ib_ftype_t)IB_FTYPE_UNUM;
    }
    else if (strcasecmp(str, "NULSTR") == 0) {
        *type = (ib_ftype_t)IB_FTYPE_NULSTR;
    }
    else if (strcasecmp(str, "BYTESTR") == 0) {
        *type = (ib_ftype_t)IB_FTYPE_BYTESTR;
    }
    else if (strcasecmp(str, "LIST") == 0) {
        *type = (ib_ftype_t)IB_FTYPE_LIST;
        if (element_type != NULL) {
            *element_type = (ib_ftype_t)IB_FTYPE_GENERIC;
        }
    }
    else if (strncasecmp(str, "LIST:", 5) == 0) {
        *type = (ib_ftype_t)IB_FTYPE_LIST;
        if (element_type != NULL) {
            ib_status_t rc;
            rc = parse_type(cp, mp, str+5, element_type, NULL);
            if (rc != IB_OK) {
                ib_log_error(cp->ib, "Invalid type '%s'", str);
            }
        }
    }
    else {
        ib_log_error(cp->ib, "Invalid type '%s'", str);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    ib_log_debug2(cp->ib, "Parsed type '%s' -> %d", str, (int)(*type) );

    /* Done */
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @param[in] cp Configuration parser
 * @param[in] mp Memory pool to use for allocations
 * @param[in] str String to parse as a type name
 * @param[in] type Field type
 * @param[in] name Field name
 * @param[out] pfield Field to create
 *
 * @return Status code
 */
static ib_status_t parse_value(ib_cfgparser_t *cp,
                               ib_mpool_t *mp,
                               const char *str,
                               ib_ftype_t type,
                               const char *name,
                               ib_field_t **pfield)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    /* Parse the type name */
    if (type == IB_FTYPE_NUM) {
        ib_num_t val = (ib_num_t) strtol(str, NULL, 0);
        rc = ib_field_create(pfield,
                             mp,
                             IB_FIELD_NAME(name),
                             type,
                             ib_ftype_num_in(&val));
    }
    else if (type == IB_FTYPE_UNUM) {
        ib_unum_t val = (ib_unum_t) strtol(str, NULL, 0);
        rc = ib_field_create(pfield,
                             mp,
                             IB_FIELD_NAME(name),
                             type,
                             ib_ftype_unum_in(&val));
    }
    else if (type == IB_FTYPE_NULSTR) {
        rc = ib_field_create(pfield,
                             mp,
                             IB_FIELD_NAME(name),
                             type,
                             ib_ftype_nulstr_in(str));
    }
    else if (type == IB_FTYPE_BYTESTR) {
        ib_bytestr_t *bs;
        rc = ib_bytestr_dup_nulstr(&bs, mp, str);
        if (rc != IB_OK) {
            ib_log_error(cp->ib,
                         "Failed to create bytestr for '%s': %d", str, rc);
            IB_FTRACE_RET_STATUS(rc);
        }
        rc = ib_field_create(pfield,
                             mp,
                             IB_FIELD_NAME(name),
                             type,
                             ib_ftype_bytestr_in(bs));
    }
    else {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Done */
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @brief Parse a FieldTx directive.
 *
 * @details Register a FieldTx directive to the engine.
 * @param[in] cp Configuration parser
 * @param[in] directive The directive name.
 * @param[in] vars The list of variables passed to @c name.
 * @param[in] cbdata User data. Unused.
 */
static ib_status_t fields_tx_params(ib_cfgparser_t *cp,
                                    const char *directive,
                                    const ib_list_t *vars,
                                    void *cbdata)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    ib_mpool_t *mp = ib_engine_pool_main_get(cp->ib);
    const ib_list_node_t *name_node;
    const ib_list_node_t *type_node;
    const ib_list_node_t *value_node;
    ib_field_t *field = NULL;
    const char *name_str;
    const char *type_str;
    ib_ftype_t type_num;
    ib_ftype_t element_type;
    ib_num_t element_num;

    if (cbdata != NULL) {
        IB_FTRACE_MSG("Callback data is not null.");
    }


    /* Get the field name string */
    name_node = ib_list_first_const(vars);
    if ( (name_node == NULL) || (name_node->data == NULL) ) {
        ib_log_error(cp->ib, "No name specified for field");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    name_str = (const char *)(name_node->data);

    /* Get type name string */
    type_node = ib_list_node_next_const(name_node);
    if ( (type_node == NULL) || (type_node->data == NULL) ) {
        ib_log_error(cp->ib, "No type specified for field");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    type_str = (const char *)(type_node->data);

    /* Parse the type name */
    rc = parse_type(cp, mp, type_str, &type_num, &element_type);
    if (rc != IB_OK) {
        ib_log_error(cp->ib,
                     "Error parsing type string '%s': %d", type_str, rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Find the next value node */
    value_node = ib_list_node_next_const(type_node);

    /* Parse the value(s) */
    if (type_num == IB_FTYPE_LIST) {

        /* Check for errors */
        if (element_type == IB_FTYPE_LIST) {
            if (value_node != NULL) {
                ib_log_error(cp->ib, "Value(s) not for LIST:LIST field");
                IB_FTRACE_RET_STATUS(IB_EINVAL);
            }
        }
        else if (element_type == IB_FTYPE_GENERIC) {
            if (value_node != NULL) {
                ib_log_error(cp->ib, "Values but no type for LIST field");
                IB_FTRACE_RET_STATUS(IB_EINVAL);
            }
        }
        else {
            if (value_node == NULL) {
                ib_log_error(cp->ib, "LIST type specified, but not values");
                IB_FTRACE_RET_STATUS(IB_EINVAL);
            }
        }

        /* Create the field */
        rc = ib_field_create(&field,
                             mp,
                             IB_FIELD_NAME(name_str),
                             type_num,
                             NULL);
        if (rc != IB_OK) {
            ib_log_error(cp->ib, "Error creating field: %d", rc);
            IB_FTRACE_RET_STATUS(rc);
        }

        ib_log_debug(cp->ib,
                     "Field %s: type %s / %s",
                     name_str,
                     g_type_names[type_num],
                     g_type_names[element_type]);

        /* Parse the values */
        element_num = 1;
        while( (value_node != NULL) && (value_node->data != NULL) ) {
            ib_field_t *vfield;
            char buf[32];

            /* Create a field name: index */
            snprintf(buf, sizeof(buf), "%ld", (long)element_num );
            ++element_num;

            /* Parse the value and create a field to contain it */
            rc = parse_value(cp, mp, value_node->data,
                             element_type, buf, &vfield);
            if (rc != IB_OK) {
                ib_log_error(cp->ib,
                             "Error parse value '%s' type %s: %d",
                             value_node->data, g_type_names[element_type], rc);
                IB_FTRACE_RET_STATUS(rc);
            }

            /* Add the field to the list */
            rc = ib_field_list_add(field, vfield);
            if (rc != IB_OK) {
                ib_log_error(cp->ib, "Error pushing value on list: %d", rc);
                IB_FTRACE_RET_STATUS(rc);
            }

            /* Next value */
            value_node = ib_list_node_next_const(value_node);
        }
    }
    else if ( (value_node != NULL) && (value_node->data != NULL) ) {
        /* Parse the value and create a field to contain it */
        rc = parse_value(cp, mp, value_node->data, type_num, name_str, &field);
        if (rc != IB_OK) {
            ib_log_error(cp->ib, "Error parse value '%s': %d", rc);
            IB_FTRACE_RET_STATUS(rc);
        }
    }
    else {
        ib_log_error(cp->ib, "No value specified for field %s", name_str);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Add the field to the list */
    rc = ib_list_push(g_field_list, field);
    if (rc != IB_OK) {
        ib_log_error(cp->ib, "Error pushing value on list: %d", rc);
        IB_FTRACE_RET_STATUS(rc);
    }
    ib_log_debug(cp->ib,
                 "Created field %p '%s' of type %d '%s'",
                 (void *)field, name_str, (int)type_num, type_str);

    /* Done */
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Handle request_header events to add fields.
 *
 * Adds fields to the transaction DPI.
 *
 * @param[in] ib IronBee object
 * @param[in] event Event type
 * @param[in,out] tx Transaction object
 * @param[in] data Callback data (not used)
 *
 * @returns Status code
 */
static ib_status_t fields_tx_headers(ib_engine_t *ib,
                                     ib_tx_t *tx,
                                     ib_state_event_type_t event,
                                     void *data)
{
    IB_FTRACE_INIT();

    assert(event == request_headers_event);

    ib_list_node_t *node;
    ib_status_t rc = IB_OK;

    /* Loop through the list */
    IB_LIST_LOOP(g_field_list, node) {
        const ib_field_t *field = (ib_field_t *)ib_list_node_data(node);
        ib_field_t *newf;

        if (field->type == IB_FTYPE_BYTESTR) {
            const ib_bytestr_t *bs;
            rc = ib_field_value(field, ib_ftype_bytestr_out(&bs));
            if (rc != IB_OK) {
                ib_log_error_tx(tx, "Failed to retrieve field value: %d", rc);
                continue;
            }
            ib_log_debug_tx(tx, "Adding bytestr %p (f=%p) = '%.*s'",
                            (void *)bs, (void *)field,
                            (int)ib_bytestr_size(bs),
                            (const char *)ib_bytestr_const_ptr(bs));
        }

        rc = ib_field_copy(&newf, tx->mp, field->name, field->nlen, field);
        if (rc != IB_OK) {
            ib_log_debug_tx(tx, "Failed to copy field: %d", rc);
            continue;
        }
        rc = ib_data_add(tx->dpi, newf);
        if (rc != IB_OK) {
            ib_log_error_tx(tx, "Failed to add field %.*s to TX DPI",
                            field->nlen, field->name);
        }
        ib_log_debug_tx(tx, "Added field %.*s (type %s)",
                        field->nlen, field->name, g_type_names[field->type]);
    }

    IB_FTRACE_RET_STATUS(rc);
}


static IB_DIRMAP_INIT_STRUCTURE(fields_directive_map) = {

    /* Give the config parser a callback for the TxField directive */
    IB_DIRMAP_INIT_LIST(
        "FieldTx",
        fields_tx_params,
        NULL
    ),

    /* signal the end of the list */
    IB_DIRMAP_INIT_LAST
};

static ib_status_t fields_init(ib_engine_t *ib, ib_module_t *m, void *cbdata)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    ib_mpool_t *mp;

    ib_log_debug(ib, "Initializing fields module.");

    /* Get a pointer to the config memory pool */
    mp = ib_engine_pool_config_get(ib);
    if (mp == NULL) {
        ib_log_error(ib, "Error getting memory pool");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* Create the list */
    rc = ib_list_create(&g_field_list, mp);
    if (rc != IB_OK) {
        ib_log_error(ib, "Error creating global field list: %d", rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the TX headers callback */
    rc = ib_hook_tx_register(ib,
                             request_headers_event,
                             fields_tx_headers,
                             NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Hook register returned %d", rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t fields_fini(ib_engine_t *ib, ib_module_t *m, void *cbdata)
{
    IB_FTRACE_INIT();
    ib_log_debug(ib, "Fields module unloading.");

    IB_FTRACE_RET_STATUS(IB_OK);
}

/* Initialize the module structure. */
IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,           /* Default metadata */
    MODULE_NAME_STR,                     /* Module name */
    IB_MODULE_CONFIG_NULL,               /* Global config data */
    NULL,                                /* Configuration field map */
    fields_directive_map,                /* Config directive map */
    fields_init,                         /* Initialize function */
    NULL,                                /* Callback data */
    fields_fini,                         /* Finish function */
    NULL,                                /* Callback data */
    NULL,                                /* Context open function */
    NULL,                                /* Callback data */
    NULL,                                /* Context close function */
    NULL,                                /* Callback data */
    NULL,                                /* Context destroy function */
    NULL                                 /* Callback data */
);
