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

#include <assert.h>
#include <math.h>
#include <strings.h>

#include <GeoIPCity.h>

#include <ironbee/cfgmap.h>
#include <ironbee/debug.h>
#include <ironbee/engine.h>
#include <ironbee/module.h>
#include <ironbee/provider.h>
#include <ironbee/util.h>
#include <ironbee/config.h>
#include <ironbee/field.h>

/* Define the module name as well as a string version of it. */
#define MODULE_NAME        geoip
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)


/* Declare the public module symbol. */
IB_MODULE_DECLARE();

/**
 * @internal
 * The GeoIP database.
 */
static GeoIP *geoip_db = NULL;

static ib_status_t geoip_lookup(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_tx_t *tx,
    void *data
)
{
    IB_FTRACE_INIT();

    const char *ip = tx->er_ipstr;

    if (ip == NULL) {
        ib_log_error(ib, 0, "Trying to lookup NULL IP in GEOIP");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
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

    ib_num_t longitude;
    ib_num_t latitude;

    GeoIPRecord *geoip_rec;

    ib_log_debug(ib, 4, "GeoIP Lookup '%s'", ip);

    /* Build a new list. */
    rc = ib_data_add_list(tx->dpi, "GEOIP", &geoip_lst);

    /* NOTICE: Called before GeoIP_record_by_addr allocates a
     * GeoIPRecord. */
    if (rc != IB_OK)
    {
        ib_log_error(ib, 0, "Unable to add GEOIP list to DPI.");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    if (geoip_db == NULL) {
        ib_log_error(ib, 0,
                     "GeoIP database was never opened. Perhaps the "
                     "configuration file needs a GeoIPDatabaseFile "
                     "\"/usr/share/geoip/GeoLiteCity.dat\" line?");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    geoip_rec = GeoIP_record_by_addr(geoip_db, ip);

    if (geoip_rec != NULL)
    {
        ib_log_debug(ib, 4, "GeoIP record found.");

        /* Append the floats latitude and longitude.
         * NOTE: Future work may add a float type to the Ironbee DPI. */

        latitude = lround(geoip_rec->latitude);
        ib_field_create(&tmp_field,
                        tx->mp,
                        IB_FIELD_NAME("latitude"),
                        IB_FTYPE_NUM,
                        ib_ftype_num_in(&latitude));
        ib_field_list_add(geoip_lst, tmp_field);

        longitude = lround(geoip_rec->longitude);
        ib_field_create(&tmp_field,
                        tx->mp,
                        IB_FIELD_NAME("longitude"),
                        IB_FTYPE_NUM,
                        ib_ftype_num_in(&longitude));
        ib_field_list_add(geoip_lst, tmp_field);

        /* Add integers. */
        tmp_field = NULL;

        /* Avoids type-punning errors on gcc 4.2.4 with holder values. */
        const ib_num_t charset = geoip_rec->charset;
        const ib_num_t area_code = geoip_rec->area_code;;

        ib_field_create(&tmp_field,
                        tx->mp,
                        IB_FIELD_NAME("area_code"),
                        IB_FTYPE_NUM,
                        ib_ftype_num_in(&area_code));
        ib_field_list_add(geoip_lst, tmp_field);
        ib_field_create(&tmp_field,
                        tx->mp,
                        IB_FIELD_NAME("charset"),
                        IB_FTYPE_NUM,
                        ib_ftype_num_in(&charset));
        ib_field_list_add(geoip_lst, tmp_field);

        /* Add strings. */
        if (geoip_rec->country_code!=NULL)
        {
            ib_field_create(&tmp_field,
                            tx->mp,
                            IB_FIELD_NAME("country_code"),
                            IB_FTYPE_NULSTR,
                            ib_ftype_nulstr_in(geoip_rec->country_code));
            ib_field_list_add(geoip_lst, tmp_field);
        }

        if (geoip_rec->country_code3!=NULL)
        {
            ib_field_create(&tmp_field,
                            tx->mp,
                            IB_FIELD_NAME("country_code3"),
                            IB_FTYPE_NULSTR,
                            ib_ftype_nulstr_in(geoip_rec->country_code3));
            ib_field_list_add(geoip_lst, tmp_field);
        }

        if (geoip_rec->country_name!=NULL)
        {
            ib_field_create(&tmp_field,
                            tx->mp,
                            IB_FIELD_NAME("country_name"),
                            IB_FTYPE_NULSTR,
                            ib_ftype_nulstr_in(geoip_rec->country_name));
            ib_field_list_add(geoip_lst, tmp_field);
        }

        if (geoip_rec->region!=NULL)
        {
            ib_field_create(&tmp_field,
                            tx->mp,
                            IB_FIELD_NAME("region"),
                            IB_FTYPE_NULSTR,
                            ib_ftype_nulstr_in(geoip_rec->region));
            ib_field_list_add(geoip_lst, tmp_field);
        }

        if (geoip_rec->city!=NULL)
        {
            ib_field_create(&tmp_field,
                            tx->mp,
                            IB_FIELD_NAME("city"),
                            IB_FTYPE_NULSTR,
                            ib_ftype_nulstr_in(geoip_rec->city));
            ib_field_list_add(geoip_lst, tmp_field);
        }

        if (geoip_rec->postal_code!=NULL)
        {
            ib_field_create(&tmp_field,
                            tx->mp,
                            IB_FIELD_NAME("postal_code"),
                            IB_FTYPE_NULSTR,
                            ib_ftype_nulstr_in(geoip_rec->postal_code));
            ib_field_list_add(geoip_lst, tmp_field);
        }

        if (geoip_rec->continent_code!=NULL)
        {
            ib_field_create(&tmp_field,
                            tx->mp,
                            IB_FIELD_NAME("continent_code"),
                            IB_FTYPE_NULSTR,
                            ib_ftype_nulstr_in(geoip_rec->continent_code));
            ib_field_list_add(geoip_lst, tmp_field);
        }
        /* If we have GeoIP_lib_version() we are using GeoIP > 1.4.6 which means we also support confidence items */
#ifdef GEOIP_HAVE_VERSION
        ib_field_create(&tmp_field,
                        tx->mp,
                        IB_FIELD_NAME("accuracy_radius"),
                        IB_FTYPE_NUM,
                        ib_ftype_num_in(&geoip_rec->accuracy_radius));
        ib_field_list_add(geoip_lst, tmp_field);

        ib_field_create(&tmp_field,
                        tx->mp,
                        IB_FIELD_NAME("metro_code"),
                        IB_FTYPE_NUM,
                        ib_ftype_num_in(&geoip_rec->metro_code));
        ib_field_list_add(geoip_lst, tmp_field);

        /* Wrap single character arguments into a 2-character string and add. */
        one_char_str[0] = geoip_rec->country_conf;
        ib_field_create(&tmp_field,
                        tx->mp,
                        IB_FIELD_NAME("country_conf"),
                        IB_FTYPE_NULSTR,
                        ib_ftype_nulstr_in(one_char_str));
        ib_field_list_add(geoip_lst, tmp_field);

        one_char_str[0] = geoip_rec->region_conf;
        ib_field_create(&tmp_field,
                        tx->mp,
                        IB_FIELD_NAME("region_conf"),
                        IB_FTYPE_NULSTR,
                        ib_ftype_nulstr_in(one_char_str));
        ib_field_list_add(geoip_lst, tmp_field);

        one_char_str[0] = geoip_rec->city_conf;
        ib_field_create(&tmp_field,
                        tx->mp,
                        IB_FIELD_NAME("city_conf"),
                        IB_FTYPE_NULSTR,
                        ib_ftype_nulstr_in(one_char_str));
        ib_field_list_add(geoip_lst, tmp_field);

        one_char_str[0] = geoip_rec->postal_conf;
        ib_field_create(&tmp_field,
                        tx->mp,
                        IB_FIELD_NAME("postal_conf"),
                        IB_FTYPE_NULSTR,
                        ib_ftype_nulstr_in(one_char_str));
        ib_field_list_add(geoip_lst, tmp_field);
#endif /* GEOIP_HAVE_VERSION */

        GeoIPRecord_delete(geoip_rec);
    }
    else
    {
        ib_log_debug(ib, 4, "No GeoIP record found.");
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t geoip_database_file_dir_param1(ib_cfgparser_t *cp,
                                                  const char *name,
                                                  const char *p1,
                                                  void *cbdata)
{
    IB_FTRACE_INIT();

    assert(cp!=NULL);
    assert(name!=NULL);
    assert(p1!=NULL);

    ib_status_t rc;
    size_t p1_len = strlen(p1);
    size_t p1_unescaped_len;
    char *p1_unescaped = malloc(p1_len+1);

    if ( p1_unescaped == NULL ) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    rc = ib_util_unescape_string(p1_unescaped,
                                 &p1_unescaped_len,
                                 p1,
                                 p1_len,
                                 IB_UTIL_UNESCAPE_NONULL);

    if (rc != IB_OK ) {
        const char *msg = ( rc == IB_EBADVAL )?
                        "GeoIP Database File \"%s\" contains nulls." :
                        "GeoIP Database File \"%s\" is an invalid string.";

        ib_log_debug(cp->ib, 3, msg, p1);
        free(p1_unescaped);
        IB_FTRACE_RET_STATUS(rc);
    }

    if (geoip_db != NULL)
    {
        GeoIP_delete(geoip_db);
        geoip_db = NULL;
    }

    IB_FTRACE_MSG("Initializing custom GeoIP database...");
    IB_FTRACE_MSG(p1_unescaped);

    geoip_db = GeoIP_open(p1_unescaped, GEOIP_MMAP_CACHE);

    free(p1_unescaped);

    if (geoip_db == NULL)
    {
        IB_FTRACE_MSG("Failed to initialize GeoIP database.");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

static IB_DIRMAP_INIT_STRUCTURE(geoip_directive_map) = {

    /* Give the config parser a callback for the directive GeoIPDatabaseFile */
    IB_DIRMAP_INIT_PARAM1(
        "GeoIPDatabaseFile",
        geoip_database_file_dir_param1,
        NULL
    ),

    /* signal the end of the list */
    IB_DIRMAP_INIT_LAST
};

/* Called when module is loaded. */
static ib_status_t geoip_init(ib_engine_t *ib, ib_module_t *m, void *cbdata)
{
    IB_FTRACE_INIT();

    ib_status_t rc;

    if (geoip_db == NULL)
    {
        ib_log_debug(ib, 4, "Initializing default GeoIP database...");
        geoip_db = GeoIP_new(GEOIP_MMAP_CACHE);
    }

    if (geoip_db == NULL)
    {
        ib_log_debug(ib, 4, "Failed to initialize GeoIP database.");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    ib_log_debug(ib, 4, "Initializing GeoIP database complete.");

    ib_log_debug(ib, 4, "Registering handler...");

    rc = ib_hook_tx_register(ib,
                             handle_context_tx_event,
                             geoip_lookup,
                             NULL);

    ib_log_debug(ib, 4, "Done registering handler.");

    if (rc != IB_OK)
    {
        ib_log_debug(ib, 4, "Failed to load GeoIP module.");
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_log_debug(ib, 4, "GeoIP module loaded.");
    IB_FTRACE_RET_STATUS(IB_OK);
}

/* Called when module is unloaded. */
static ib_status_t geoip_fini(ib_engine_t *ib, ib_module_t *m, void *cbdata)
{
    IB_FTRACE_INIT();
    if (geoip_db!=NULL)
    {
        GeoIP_delete(geoip_db);
    }
    ib_log_debug(ib, 4, "GeoIP module unloaded.");
    IB_FTRACE_RET_STATUS(IB_OK);
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
    NULL,                                /* Context open function */
    NULL,                                /* Callback data */
    NULL,                                /* Context close function */
    NULL,                                /* Callback data */
    NULL,                                /* Context destroy function */
    NULL                                 /* Callback data */
);

