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
    GeoIP           *geoip_db;   /**< The GeoIP database */
    ib_var_source_t *geoip_source; /**< Var source for GEO */
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
    assert(tx != NULL);
    assert(tx->mp != NULL);
    assert(event == handle_context_tx_event);
    assert(data != NULL);

    const char          *ip = tx->remote_ipstr;
    const module_data_t *mod_data = (const module_data_t *)data;
    ib_mpool_t          *mp = tx->mp;

    if (ip == NULL) {
        ib_log_notice_tx(tx, "GeoIP: Trying to lookup NULL IP");
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

    ib_log_debug_tx(tx, "GeoIP: Lookup \"%s\"", ip);

    /* Build a new list. */
    rc = ib_var_source_initialize(
        mod_data->geoip_source,
        &geoip_lst,
        tx->var_store,
        IB_FTYPE_LIST
    );

    /* NOTICE: Called before GeoIP_record_by_addr allocates a
     * GeoIPRecord. */
    if (rc != IB_OK) {
        ib_log_error_tx(tx, "GeoIP: Failed to add GEOIP var.");
        return IB_EINVAL;
    }

    if (mod_data->geoip_db == NULL) {
        ib_log_error_tx(tx,
                        "GeoIP: Database was never opened. Perhaps the "
                        "configuration file needs a GeoIPDatabaseFile "
                        "\"/usr/share/geoip/GeoLite.dat\" line?");
        return IB_EINVAL;
    }

    geoip_id = GeoIP_id_by_addr(mod_data->geoip_db, ip);

    if (geoip_id > 0) {
        const char *tmp_str;
        ib_bytestr_t *tmp_bs;

        ib_log_debug_tx(tx, "GeoIP: Record found.");

        /* Add integers. */
        tmp_field = NULL;

        tmp_str = GeoIP_code_by_id(geoip_id);
        if (tmp_str)
        {
            rc = ib_bytestr_dup_nulstr(&tmp_bs, mp, tmp_str);
            if (rc != IB_OK) {
                ib_log_error_tx(tx, "GeoIP: Failed to dup country_code %s", tmp_str);
                return rc;
            }
            ib_field_create(&tmp_field,
                            mp,
                            IB_S2SL("country_code"),
                            IB_FTYPE_BYTESTR,
                            ib_ftype_bytestr_in(tmp_bs));
            ib_field_list_add(geoip_lst, tmp_field);
        }

        tmp_str = GeoIP_code3_by_id(geoip_id);
        if (tmp_str)
        {
            rc = ib_bytestr_dup_nulstr(&tmp_bs, mp, tmp_str);
            if (rc != IB_OK) {
                ib_log_error_tx(tx, "GeoIP: Failed to dup country_code3 %s", tmp_str);
                return rc;
            }
            ib_field_create(&tmp_field,
                            mp,
                            IB_S2SL("country_code3"),
                            IB_FTYPE_BYTESTR,
                            ib_ftype_bytestr_in(tmp_bs));
            ib_field_list_add(geoip_lst, tmp_field);
        }

        tmp_str = GeoIP_country_name_by_id(mod_data->geoip_db, geoip_id);
        if (tmp_str)
        {
            rc = ib_bytestr_dup_nulstr(&tmp_bs, mp, tmp_str);
            if (rc != IB_OK) {
                ib_log_error_tx(tx, "GeoIP: Failed to dup country_name %s", tmp_str);
                return rc;
            }
            ib_field_create(&tmp_field,
                            mp,
                            IB_S2SL("country_name"),
                            IB_FTYPE_BYTESTR,
                            ib_ftype_bytestr_in(tmp_bs));
            ib_field_list_add(geoip_lst, tmp_field);
        }

        tmp_str = GeoIP_continent_by_id(geoip_id);
        if (tmp_str)
        {
            rc = ib_bytestr_dup_nulstr(&tmp_bs, mp, tmp_str);
            if (rc != IB_OK) {
                ib_log_error_tx(tx, "GeoIP: Failed to dup continent_code %s", tmp_str);
                return rc;
            }
            ib_field_create(&tmp_field,
                            mp,
                            IB_S2SL("continent_code"),
                            IB_FTYPE_BYTESTR,
                            ib_ftype_bytestr_in(tmp_bs));
            ib_field_list_add(geoip_lst, tmp_field);
        }
    }
    else
    {
        ib_bytestr_t *tmp_bs;
        ib_log_debug_tx(tx, "GeoIP: No record found.");

        rc = ib_bytestr_alias_nulstr(&tmp_bs, mp, "01");
        if (rc != IB_OK) {
            ib_log_error_tx(tx, "GeoIP: Failed to dup string 01");
            return rc;
        }
        ib_field_create(&tmp_field,
                        mp,
                        IB_S2SL("country_code"),
                        IB_FTYPE_BYTESTR,
                        ib_ftype_bytestr_in(tmp_bs));
        ib_field_list_add(geoip_lst, tmp_field);

        rc = ib_bytestr_alias_nulstr(&tmp_bs, mp, "001");
        if (rc != IB_OK) {
            ib_log_error_tx(tx, "GeoIP: Failed to dup string 001");
            return rc;
        }
        ib_field_create(&tmp_field,
                        mp,
                        IB_S2SL("country_code3"),
                        IB_FTYPE_BYTESTR,
                        ib_ftype_bytestr_in(tmp_bs));
        ib_field_list_add(geoip_lst, tmp_field);

        rc = ib_bytestr_alias_nulstr(&tmp_bs, mp, "01");
        if (rc != IB_OK) {
            ib_log_error_tx(tx, "GeoIP: Failed to dup string Other Country");
            return rc;
        }
        ib_field_create(&tmp_field,
                        mp,
                        IB_S2SL("country_name"),
                        IB_FTYPE_BYTESTR,
                        ib_ftype_bytestr_in(tmp_bs));
        ib_field_list_add(geoip_lst, tmp_field);

        rc = ib_bytestr_alias_nulstr(&tmp_bs, mp, "01");
        if (rc != IB_OK) {
            ib_log_error_tx(tx, "GeoIP: Failed to dup string 01");
            return rc;
        }
        ib_field_create(&tmp_field,
                        mp,
                        IB_S2SL("continent_code"),
                        IB_FTYPE_BYTESTR,
                        ib_ftype_bytestr_in(tmp_bs));
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
                                 p1_len);

    if (rc != IB_OK ) {
        if (rc == IB_EBADVAL) {
            ib_cfg_log_debug(cp, "GeoIP: Database File \"%s\" contains nulls.", p1);
        }
        else {
            ib_cfg_log_debug(cp, "GeoIP: Database File \"%s\" is an invalid string.", p1);
        }
        free(p1_unescaped);
        return rc;
    }

    assert(p1_unescaped_len <= p1_len);

    /* Null-terminate the result. */
    p1_unescaped[p1_unescaped_len] = '\0';

    if (mod_data->geoip_db != NULL) {
        GeoIP_delete(mod_data->geoip_db);
        mod_data->geoip_db = NULL;
    }

    mod_data->geoip_db = GeoIP_open(p1_unescaped, GEOIP_MMAP_CACHE);

    if (mod_data->geoip_db == NULL) {
        int status = access(p1_unescaped, R_OK);
        if (status != 0) {
            ib_cfg_log_error(cp, "GeoIP: Unable to read database file \"%s\"",
                             p1_unescaped);
            rc = IB_ENOENT;
        }
        else {
            ib_cfg_log_error(cp,
                             "GeoIP: Unknown error opening database file \"%s\"",
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

    mod_data = ib_mpool_calloc(ib_engine_mm_main_get(ib),
                               sizeof(*mod_data), 1);
    if (mod_data == NULL) {
        return IB_EALLOC;
    }

    ib_log_debug(ib, "GeoIP: Initializing default database...");
    geoip_db = GeoIP_new(GEOIP_MMAP_CACHE);
    if (geoip_db == NULL) {
        ib_log_error(ib, "GeoIP: Failed to initialize database.");
        return IB_EUNKNOWN;
    }

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
            "GeoIP: Error registering tx hook: %s",
            ib_status_to_string(rc));
        return rc;
    }

    rc = ib_var_source_register(
        &(mod_data->geoip_source),
        ib_engine_var_config_get(ib),
        IB_S2SL("GEOIP"),
        IB_PHASE_NONE, IB_PHASE_NONE
    );
    if (rc != IB_OK) {
        ib_log_warning(ib,
            "GeoIP: Error registering \"GEOIP\" var: %s",
            ib_status_to_string(rc)
        );
        /* Continue */
    }

    if (rc != IB_OK) {
        ib_log_error(ib, "GeoIP: Failed to load module.");
        return rc;
    }

    geoip_directive_map[0].cbdata_cb = mod_data;

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
