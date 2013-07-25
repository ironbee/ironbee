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
 * @brief IronBee --- Module providing basic geoip services.
 */

#include <ironbee/cfgmap.h>
#include <ironbee/config.h>
#include <ironbee/context.h>
#include <ironbee/engine.h>
#include <ironbee/engine_state.h>
#include <ironbee/escape.h>
#include <ironbee/field.h>
#include <ironbee/module.h>
#include <ironbee/mpool.h>

#include <GeoIP.h>

#include <assert.h>
#include <math.h>
#include <strings.h>
#include <unistd.h>

/* Define the module name as well as a string version of it. */
#define MODULE_NAME        geoip
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

/**
 * Data used by each module instance, associated with an IronBee engine.
 */
typedef struct {
    GeoIP       *geoip_db;         /**< The GeoIP database */
} module_data_t;

/* Declare the public module symbol. */
IB_MODULE_DECLARE();

/**
 * Lookup the IP address in the GeoIP database
 *
 * @param[in] ib IronBee engine
 * @param[in] tx Transaction
 * @param[in] event Event
 * @param[in] data callback data (Module configuration)
 */
static ib_status_t geoip_lookup(
    ib_engine_t *ib,
    ib_tx_t *tx,
    ib_state_event_type_t event,
    void *data
)
{
    assert(ib != NULL);
    assert(event == handle_context_tx_event);
    assert(data != NULL);

    const char *ip = tx->er_ipstr;
    const module_data_t *mod_data = (const module_data_t *)data;

    if (ip == NULL) {
        ib_log_alert_tx(tx, "Trying to lookup NULL IP in GEOIP");
        return IB_EINVAL;
    }

#ifdef GEOIP_HAVE_VERSION
    /**
     * Some configurations exist as single characters and must be converted to
     * a string. This is simply a place to assemble that string before
     * it is passed into ip_data_add_nulstr.
     * This is only needed if we have support confidence items. WAM
     */
    char one_char_str[2] = { '\0', '\0' };
#endif /* GEOIP_HAVE_VERSION */

    ib_status_t rc;

    /* Declare and initialize the GeoIP property list.
     * Regardless of if we find a record or not, we want to create the list
     * artifact so that later modules know we ran and did [not] find a
     * record. */
    ib_field_t *geoip_lst = NULL;

    ib_field_t *tmp_field = NULL;

    /* Id of geo ip record to read. */
    int geoip_id;

    ib_log_debug_tx(tx, "GeoIP Lookup '%s'", ip);

    /* Build a new list. */
    rc = ib_data_add_list(tx->data, "GEOIP", &geoip_lst);

    /* NOTICE: Called before GeoIP_record_by_addr allocates a
     * GeoIPRecord. */
    if (rc != IB_OK) {
        ib_log_alert_tx(tx, "Unable to add GEOIP list to DPI.");
        return IB_EINVAL;
    }

    if (mod_data->geoip_db == NULL) {
        ib_log_alert_tx(tx,
                        "GeoIP database was never opened. Perhaps the "
                        "configuration file needs a GeoIPDatabaseFile "
                        "\"/usr/share/geoip/GeoLite.dat\" line?");
        return IB_EINVAL;
    }

    geoip_id = GeoIP_id_by_addr(mod_data->geoip_db, ip);

    if (geoip_id > 0) {
        const char *tmp_str;

        ib_log_debug_tx(tx, "GeoIP record found.");

        /* Add integers. */
        tmp_field = NULL;

        tmp_str = GeoIP_code_by_id(geoip_id);
        if (tmp_str)
        {
            ib_field_create(&tmp_field,
                            tx->mp,
                            IB_FIELD_NAME("country_code"),
                            IB_FTYPE_NULSTR,
                            ib_ftype_nulstr_in(tmp_str));
            ib_field_list_add(geoip_lst, tmp_field);
        }

        tmp_str = GeoIP_code3_by_id(geoip_id);
        if (tmp_str)
        {
            ib_field_create(&tmp_field,
                            tx->mp,
                            IB_FIELD_NAME("country_code3"),
                            IB_FTYPE_NULSTR,
                            ib_ftype_nulstr_in(tmp_str));
            ib_field_list_add(geoip_lst, tmp_field);
        }

        tmp_str = GeoIP_country_name_by_id(mod_data->geoip_db, geoip_id);
        if (tmp_str)
        {
            ib_field_create(&tmp_field,
                            tx->mp,
                            IB_FIELD_NAME("country_name"),
                            IB_FTYPE_NULSTR,
                            ib_ftype_nulstr_in(tmp_str));
            ib_field_list_add(geoip_lst, tmp_field);
        }

        tmp_str = GeoIP_continent_by_id(geoip_id);
        if (tmp_str)
        {
            ib_field_create(&tmp_field,
                            tx->mp,
                            IB_FIELD_NAME("continent_code"),
                            IB_FTYPE_NULSTR,
                            ib_ftype_nulstr_in(tmp_str));
            ib_field_list_add(geoip_lst, tmp_field);
        }
    }
    else
    {
        ib_log_debug_tx(tx, "No GeoIP record found.");
        ib_field_create(&tmp_field,
                        tx->mp,
                        IB_FIELD_NAME("country_code"),
                        IB_FTYPE_NULSTR,
                        ib_ftype_nulstr_in("O1"));
        ib_field_list_add(geoip_lst, tmp_field);
        ib_field_create(&tmp_field,
                        tx->mp,
                        IB_FIELD_NAME("country_code3"),
                        IB_FTYPE_NULSTR,
                        ib_ftype_nulstr_in("O01"));
        ib_field_list_add(geoip_lst, tmp_field);
        ib_field_create(&tmp_field,
                        tx->mp,
                        IB_FIELD_NAME("country_name"),
                        IB_FTYPE_NULSTR,
                        ib_ftype_nulstr_in("Other Country"));
        ib_field_list_add(geoip_lst, tmp_field);
        ib_field_create(&tmp_field,
                        tx->mp,
                        IB_FIELD_NAME("continent_code"),
                        IB_FTYPE_NULSTR,
                        ib_ftype_nulstr_in("O1"));
        ib_field_list_add(geoip_lst, tmp_field);
    }

    return IB_OK;
}


/**
 * Handle a GeoIPDatabaseFile directive.
 *
 * @param[in] cp Configuration parser
 * @param[in] name The directive name.
 * @param[in] p1 The directive parameter.
 * @param[in] cbdata User data (module configuration)
 */
static ib_status_t geoip_database_file_dir_param1(ib_cfgparser_t *cp,
                                                  const char *name,
                                                  const char *p1,
                                                  void *cbdata)
{
    assert(cp != NULL);
    assert(name != NULL);
    assert(p1 != NULL);
    assert(cbdata != NULL);

    ib_status_t rc;
    size_t p1_len = strlen(p1);
    size_t p1_unescaped_len;
    char *p1_unescaped = malloc(p1_len+1);
    module_data_t *mod_data = (module_data_t *)cbdata;

    if (p1_unescaped == NULL) {
        return IB_EALLOC;
    }

    rc = ib_util_unescape_string(p1_unescaped,
                                 &p1_unescaped_len,
                                 p1,
                                 p1_len,
                                 IB_UTIL_UNESCAPE_NULTERMINATE |
                                 IB_UTIL_UNESCAPE_NONULL);

    if (rc != IB_OK ) {
        const char *msg = ( rc == IB_EBADVAL )?
                        "GeoIP Database File \"%s\" contains nulls." :
                        "GeoIP Database File \"%s\" is an invalid string.";

        ib_cfg_log_debug(cp, msg, p1);
        free(p1_unescaped);
        return rc;
    }

    if (mod_data->geoip_db != NULL) {
        GeoIP_delete(mod_data->geoip_db);
        mod_data->geoip_db = NULL;
    }

    mod_data->geoip_db = GeoIP_open(p1_unescaped, GEOIP_MMAP_CACHE);

    if (mod_data->geoip_db == NULL) {
        int status = access(p1_unescaped, R_OK);
        if (status != 0) {
            ib_cfg_log_error(cp, "Unable to read GeoIP database file \"%s\"",
                             p1_unescaped);
            rc = IB_ENOENT;
        }
        else {
            ib_cfg_log_error(cp,
                             "Unknown error opening GeoIP database file \"%s\"",
                             p1_unescaped);
            rc = IB_EUNKNOWN;
        }
    }

    free(p1_unescaped);
    return rc;
}

static ib_dirmap_init_t geoip_directive_map[] = {

    /* Give the config parser a callback for the directive GeoIPDatabaseFile */
    IB_DIRMAP_INIT_PARAM1(
        "GeoIPDatabaseFile",
        geoip_database_file_dir_param1,
        NULL                            /* Filled in by the init function */
    ),

    /* signal the end of the list */
    IB_DIRMAP_INIT_LAST
};

/* Called when module is loaded. */
static ib_status_t geoip_init(ib_engine_t *ib, ib_module_t *m, void *cbdata)
{
    ib_status_t    rc;
    GeoIP         *geoip_db = NULL;
    module_data_t *mod_data;

    mod_data = ib_mpool_calloc(ib_engine_pool_main_get(ib),
                               sizeof(*mod_data), 1);
    if (mod_data == NULL) {
        return IB_EALLOC;
    }

    ib_log_debug(ib, "Initializing default GeoIP database...");
    geoip_db = GeoIP_new(GEOIP_MMAP_CACHE);
    if (geoip_db == NULL) {
        ib_log_debug(ib, "Failed to initialize GeoIP database.");
        return IB_EUNKNOWN;
    }

    ib_log_debug(ib, "Initializing GeoIP database complete.");
    ib_log_debug(ib, "Registering handler...");

    /* Store off pointer to our module data structure */
    mod_data->geoip_db = geoip_db;

    /* And point the generic module data at it */
    m->data = mod_data;

    rc = ib_hook_tx_register(ib,
                             handle_context_tx_event,
                             geoip_lookup,
                             mod_data);
    if (rc != IB_OK) {
        ib_log_debug(
            ib,
            "Failed to register tx hook: %s",
            ib_status_to_string(rc));
        return rc;
    }

    ib_log_debug(ib, "Done registering handler.");

    rc = ib_data_register_indexed(ib_engine_data_config_get(ib), "GEOIP");
    if (rc != IB_OK) {
        ib_log_warning(ib,
            "GeoIP failed to register \"GEOIP\" as indexed: %s",
            ib_status_to_string(rc)
        );
        /* Continue */
    }

    if (rc != IB_OK) {
        ib_log_debug(ib, "Failed to load GeoIP module.");
        return rc;
    }

    geoip_directive_map[0].cbdata_cb = mod_data;

    ib_log_debug(ib, "GeoIP module loaded.");
    return IB_OK;
}

/* Called when module is unloaded. */
static ib_status_t geoip_fini(ib_engine_t *ib, ib_module_t *m, void *cbdata)
{
    assert(ib != NULL);
    assert(m != NULL);
    assert(m->data != NULL);

    module_data_t *mod_data = (module_data_t *)m->data;

    if (mod_data->geoip_db != NULL) {
        GeoIP_delete(mod_data->geoip_db);
        mod_data->geoip_db = NULL;
    }
    ib_log_debug(ib, "GeoIP module unloaded.");
    return IB_OK;
}

/* Initialize the module structure. */
IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,           /* Default metadata */
    MODULE_NAME_STR,                     /* Module name */
    IB_MODULE_CONFIG_NULL,               /* Global config data */
    NULL,                                /* Configuration field map */
    geoip_directive_map,                 /* Config directive map */
    geoip_init,                          /* Initialize function */
    NULL,                                /* Callback data */
    geoip_fini,                          /* Finish function */
    NULL,                                /* Callback data */
);
