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

/**
 * @file
 * @brief IronBee - User Agent Extraction Module
 *
 * This module extracts the user agent information
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include <user_agent_private.h>
#include <stdint.h>
#include <sys/types.h>
#include <string.h>
#include <ctype.h>

/* Include files required to build this as a stand-alone program */
#ifdef USER_AGENT_MAIN
#  include <stdlib.h>
#  include <stddef.h>
#  include <errno.h>
#  include <stdio.h>
#endif

#include <ironbee/types.h>
#include <ironbee/engine.h>
#include <ironbee/util.h>
#include <ironbee/module.h>
#include <ironbee/debug.h>
#include <ironbee/hash.h>
#include <ironbee/bytestr.h>
#include <ironbee/mpool.h>

/* Max line buffer for the stand alone program */
#ifdef USER_AGENT_MAIN
#  define MAX_LINE_BUF (16*1024)
#endif

#ifndef USER_AGENT_MAIN
/* Define the module name as well as a string version of it. */
#define MODULE_NAME        remote_ip
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

/* Declare the public module symbol. */
IB_MODULE_DECLARE();
#endif

static const modua_match_ruleset_t *modua_rules = NULL;

/**
 * @internal
 * Skip spaces, return pointer to first non-space.
 *
 * Skips spaces in the passed in string.
 *
 * @param[in] str String to skip
 *
 * @returns Pointer to first non-space character in the string.
 */
static char *skip_space(char *str)
{
    while (*str == ' ') {
        ++str;
    }
    return (*str == '\0') ? NULL : str;
}

/**
 * @internal
 * Parse the user agent header.
 *
 * Attempt to tokenize the user agent string passed in, splitting up
 * the passed in string into component parts.
 *
 * @param[in,out] str User agent string to parse
 * @param[out] p_product Pointer to product string
 * @param[out] p_platform Pointer to platform string
 * @param[out] p_extra Pointer to "extra" string
 *
 * @returns Status code
 */
static ib_status_t modua_parse_uastring(char *str,
                                        char **p_product,
                                        char **p_platform,
                                        char **p_extra)
{
    IB_FTRACE_INIT(modua_parse_uastring);
    char *lp = NULL;            /* lp: Left parent */
    char *extra = NULL;

    /* Initialize these to known values */
    *p_platform = NULL;
    *p_extra = NULL;

    /* Skip any leading space */
    str = skip_space(str);

    /* Simple validation */
    if ( (str == NULL) || (isalnum(*str) == 0) ) {
        *p_product = NULL;
        *p_extra = str;
        IB_FTRACE_RET_STATUS( (str == NULL) ? IB_EUNKNOWN : IB_OK );
    }

    /* The product is the first field. */
    *p_product = str;

    /* Search for a left parent followed by a right paren */
    lp = strstr(str, " (");
    if (lp != NULL) {
        char *rp;

        /* Find the matching right paren (if it exists).
         *  First try ") ", then ")" to catch a paren at end of line. */
        rp = strstr(lp, ") ");
        if (rp == NULL) {
            rp = strchr(str, ')');
        }

        /* If no matching right paren, ignore the left paren */
        if (rp == NULL) {
            lp = NULL;
        }
        else {
            /* Terminate the string after the right paren */
            ++rp;
            if ( (*rp == ' ') || (*rp == ',') || (*rp == ';') ) {
                extra = rp;
                *extra++ = '\0';
            }
            else if (*rp != '\0') {
                lp = NULL;
            }
        }
    }

    /* No parens?  'Extra' starts after the first space */
    if (lp == NULL) {
        char *cur = strchr(str, ' ');
        if (cur != NULL) {
            *cur++ = '\0';
        }
        extra = cur;
    }

    /* Otherwise, clean up around the parens */
    else {
        char *cur = lp++;
        while( (cur > str) && (*cur == ' ') ) {
            *cur = '\0';
            --cur;
        }
    }

    /* Skip extra whitespace preceding the real extra */
    if (extra != NULL) {
        extra = skip_space(extra);
    }

    /* Done: Store the results in the passed in pointers.
     * Note: p_product is filled in above. */
    *p_platform = lp;
    *p_extra = extra;
    IB_FTRACE_RET_STATUS(IB_OK);
}

/* Macros used to return the correct value based on a value and an 
 * match value.
 * av: Actual value
 * mv: Match value
 */
#define RESULT_EQ(av,mv)  ( ((av) == (mv)) ? YES : NO )
#define RESULT_NEQ(av,mv) ( ((av) != (mv)) ? YES : NO )

/**
 * @internal
 * Match a field against the specified match rule.
 *
 * Attempts to match the field string (or NULL) against the field match rule.
 * NULL)
 *
 * @param[in] str User agent string to match
 * @param[in] rule Field match rule to match the string against
 *
 * @returns 1 if the string matches, 0 if not.
 */
static modua_matchresult_t modua_frule_match(const char *str,
                                             const modua_field_rule_t *rule)
{
    IB_FTRACE_INIT(modua_rule_match);

    /* First, handle the simple NULL string case */
    if (str == NULL) {
        return NO;
    }

    /* Match using the rule's match type */
    switch (rule->match_type) {
        case EXISTS:         /* Note: NULL/NO handled above */
            return YES;
        case MATCHES:
            return RESULT_EQ(strcmp(str, rule->string), 0);
        case STARTSWITH:
            return RESULT_EQ(strncmp(str, rule->string, rule->slen), 0);
        case CONTAINS:
            return RESULT_NEQ(strstr(str, rule->string), NULL);
        case ENDSWITH: {
            size_t slen = strlen(str);
            size_t offset;
            if (slen < rule->slen) {
                IB_FTRACE_RET_INT(0);
            }
            offset = (slen - rule->slen);
            IB_FTRACE_RET_INT(RESULT_EQ(strcmp(str+offset, rule->string), 0));
        }
        default :
            fprintf(stderr,
                    "modua_frule_match: invalid match type %d",
                    rule->match_type);
    }

    /* Should never get here! */
    IB_FTRACE_RET_INT(NO);
}

/**
 * @internal
 * Apply the user agent category rules.
 *
 * Walks through the internal static category rules, attempts to apply each of
 * them to the passed in agent info, and returns the category string of the
 * first rule that matches, or NULL if no rules match.
 *
 * @param[in] product UA product component
 * @param[in] platform UA platform component
 * @param[in] extra UA extra component
 *
 * @returns Category string or NULL
 */
static const char *modua_mrule_match(const char *fields[],
                                     const modua_match_rule_t *rule)
{
    IB_FTRACE_INIT(modua_mrule_match);
    const modua_field_rule_t *fr;
    unsigned ruleno;

    /* Walk through the rules; if any fail, return NULL */
    for (ruleno = 0, fr = rule->rules;
         ruleno < rule->num_rules;
         ++ruleno, ++fr) {

        /* Apply the rule */
        modua_matchresult_t result = 
            modua_frule_match(fields[fr->match_field], fr);

        /* If it doesn't match the expect results, we're done */
        if (result != fr->match_result) {
            IB_FTRACE_RET_CONSTSTR( (const char *)NULL );
        }
    }

    /* If we've applied all rules, and all have passed, return the category
     * string name */
    IB_FTRACE_RET_CONSTSTR( rule->category );
}

/**
 * @internal
 * Apply the user agent category rules.
 *
 * Walks through the internal static category rules, attempts to apply each of
 * them to the passed in agent info, and returns the category string of the
 * first rule that matches, or NULL if no rules match.
 *
 * @param[in] product UA product component
 * @param[in] platform UA platform component
 * @param[in] extra UA extra component
 *
 * @returns Category string or NULL
 */
static const char *modua_match_cat_rules(const char *product,
                                         const char *platform,
                                         const char *extra)
{
    IB_FTRACE_INIT(modua_match_cat_rules);
    const char *fields[3] = { product, platform, extra };
    const modua_match_rule_t *rule;
    unsigned ruleno;

    /* Walk through the rules; the first to match "wins" */
    for (ruleno = 0, rule = modua_rules->rules;
         ruleno < modua_rules->num_rules;
         ++ruleno, ++rule ) {
        const char *result;

        /* Apply the field rules */
        result = modua_mrule_match(fields, rule);

        /* If the entire rule set matches, return the category string */
        if (result != (const char *)NULL) {
            IB_FTRACE_RET_CONSTSTR( result );
        }
    }

    /* If we've applied all rules, and have had success, return NULL */
    IB_FTRACE_RET_CONSTSTR( (const char *)NULL );
}

#ifndef USER_AGENT_MAIN
/**
 * @internal
 * Parse the user agent header, splitting into component fields.
 *
 * Attempt to tokenize the user agent string passed in, storing the
 * result in the DPI associated with the transaction.
 *
 * @param[in] ib IronBee object
 * @param[in,out] tx Transaction object
 * @param[in] data Callback data (not used)
 *
 * @returns Status code
 */
static ib_status_t modua_store_field(ib_engine_t *ib,
                                     ib_mpool_t *mp,
                                     ib_field_t *agent_list,
                                     const char *name,
                                     const char *value)
{
    IB_FTRACE_INIT(modua_store_field);
    ib_field_t *tmp_field = NULL;
    ib_status_t rc = IB_OK;

    /* No value?  Do nothing */
    if (value == NULL) {
        ib_log_debug(ib, 9, "No %s field in user agent", name);
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Create the field */
    rc = ib_field_create(&tmp_field, mp, name, IB_FTYPE_NULSTR, &value);
    if (rc != IB_OK) {
        ib_log_error(ib, 0,
                     "Error creating user agent %s field: %d", name, rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Add the field to the list */
    rc = ib_field_list_add(agent_list, tmp_field);
    if (rc != IB_OK) {
        ib_log_error(ib, 0,
                     "Error adding user agent %s field: %d", name, rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_log_debug(ib, 9, "Stored user agent %s '%s'", name, value);

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Parse the user agent header, splitting into component fields.
 *
 * Attempt to tokenize the user agent string passed in, storing the
 * result in the DPI associated with the transaction.
 *
 * @param[in] ib IronBee object
 * @param[in,out] tx Transaction object
 * @param[in] data Callback data (not used)
 *
 * @returns Status code
 */
static ib_status_t modua_agent_fields(ib_engine_t *ib,
                                      ib_tx_t *tx,
                                      char *agent)
{
    IB_FTRACE_INIT(modua_handle_tx);
    ib_field_t *agent_list = NULL;
    char *product = NULL;
    char *platform = NULL;
    char *extra = NULL;
    const char *category = NULL;
    ib_status_t rc;

    /* Parse the user agent string */
    rc = modua_parse_uastring(agent, &product, &platform, &extra);
    if (rc != IB_OK) {
        ib_log_debug(ib, 4, "Failed to parse User Agent string '%s'", agent);
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Categorize the parsed string */
    category = modua_match_cat_rules(product, platform, extra);

    /* Build a new list. */
    rc = ib_data_add_list(tx->dpi, "User-Agent", &agent_list);
    if (rc != IB_OK)
    {
        ib_log_error(ib, 0, "Unable to add UserAgent list to DPI.");
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Handle product */
    rc = modua_store_field(ib, tx->mp, agent_list, "product", product);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Platform */
    rc = modua_store_field(ib, tx->mp, agent_list, "platform", platform);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Extra */
    rc = modua_store_field(ib, tx->mp, agent_list, "extra", extra);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Extra */
    rc = modua_store_field(ib, tx->mp, agent_list, "category", category);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Done */
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Handle request_header events.
 *
 * Extract the "request_headers" field (a list) from the transactions's
 * data provider instance, then loop through the list, looking for the
 * "User-Agent"  field.  If found, the value is parsed and used to update the
 * connection object fields.
 *
 * @param[in] ib IronBee object
 * @param[in,out] tx Transaction object
 * @param[in] data Callback data (not used)
 *
 * @returns Status code
 */
static ib_status_t modua_handle_req_headers(ib_engine_t *ib,
                                            ib_tx_t *tx,
                                            void *data)
{
    IB_FTRACE_INIT(modua_handle_req_headers);
    ib_conn_t *conn = tx->conn;
    ib_field_t *req = NULL;
    ib_status_t rc = IB_OK;
    ib_list_t *lst = NULL;
    ib_list_node_t *node = NULL;

    /* Extract the request headers field from the provider instance */
    rc = ib_data_get(tx->dpi, "request_headers", &req);
    if ( (req == NULL) || (rc != IB_OK) ) {
        ib_log_debug(ib, 4,
                     "request_headers_event: "
                     "No request headers provided" );
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* The field value *should* be a list, extract it as such */
    lst = ib_field_value_list(req);
    if (lst == NULL) {
        ib_log_debug(ib, 4,
                     "request_headers_event: "
                     "Field list missing / incorrect type" );
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* Loop through the list; we're looking for User-Agent */
    IB_LIST_LOOP(lst, node) {
        ib_field_t *field = (ib_field_t *)ib_list_node_data(node);
        ib_bytestr_t *bs;
        unsigned len;
        char *buf;

        /* Check the field name
         * Note: field->name is not always a null ('\0') terminated string */
        if (strncmp(field->name, "User-Agent", field->nlen) != 0) {
            continue;
        }

        /* Found it: copy the data into a newly allocated string buffer */
        bs = ib_field_value_bytestr(field);
        len = ib_bytestr_length(bs);

        /* Allocate the memory */
        buf = (char *)ib_mpool_calloc(conn->mp, 1, len+1);
        if (buf == NULL) {
            ib_log_error( ib, 4,
                          "Failed to allocate %d bytes for local address",
                          len+1 );
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }

        /* Copy the string out */
        memcpy(buf, ib_bytestr_ptr(bs), len);
        buf[len] = '\0';
        ib_log_debug(ib, 4, "user agent => '%s'", buf);

        /* Finally, split it up & store the components */
        rc = modua_agent_fields(ib, tx, buf);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Done */
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Called to initialize the user agent module (when the module is loaded).
 *
 * Registers a handler for the request_headers_event event.
 *
 * @param[in,out] ib IronBee object
 * @param[in] m Module object
 *
 * @returns Status code
 */
static ib_status_t modua_init(ib_engine_t *ib, ib_module_t *m)
{
    IB_FTRACE_INIT(modua_context_init);
    ib_status_t rc;
    unsigned    failed_rule_num;

    /* Register our callback */
    rc = ib_hook_register(ib, request_headers_event,
                          (ib_void_fn_t)modua_handle_req_headers,
                          (void*)request_headers_event);
    if (rc != IB_OK) {
        ib_log_error(ib, 4, "Hook register returned %d", rc);
    }

    /* Initializations */
    rc = modua_rules_init( &failed_rule_num );
    if (rc != IB_OK) {
        ib_log_error(ib, 4,
                     "User agent rule initialization failed on rule #%d: %d",
                     failed_rule_num, rc);
    }

    /* Get the rules */
    modua_rules = modua_rules_get( );
    if (modua_rules == NULL) {
        ib_log_error(ib, 4, "Failed to get user agent rule list: %d", rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Called to close the user agent module (when the module is unloaded).
 *
 * Does nothing
 *
 * @param[in,out] ib IronBee object (unused)
 * @param[in] m Module object (unused)
 *
 * @returns Status code
 */
static ib_status_t modua_finish(ib_engine_t *ib, ib_module_t *m)
{
    IB_FTRACE_INIT(modua_finish);
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Called when the module's context is initialized. 
 *
 * Does nothing
 *
 * @param[in,out] ib IronBee object (unused)
 * @param[in] m Module object (unused)
 * @param[in] ctx Configuration context (unused)
 *
 * @returns Status code
 */
static ib_status_t modua_context_init(ib_engine_t *ib,
                                      ib_module_t *m,
                                      ib_context_t *ctx)
{
    IB_FTRACE_INIT(modua_context_init);
    IB_FTRACE_RET_STATUS(IB_OK);
}

IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,      /* Default metadata */
    "user agent",                   /* Module name */
    NULL,                           /* Global config data */
    0,                              /* Global config data length*/
    NULL,                           /* Module config map */
    NULL,                           /* Module directive map */

    modua_init,                     /* Initialize function */
    modua_finish,                   /* Finish function */
    modua_context_init              /* Context init function */
);
#else

/**
 * @brief
 * Main to run a simple test of the user agent logic.
 *
 * Reads user agent strings from a file, and invokes the internal
 * modua_parse_uastring() function to parse each string, then prints the
 * resulting data.
 *
 * @param[in] argc Argument count
 * @param[in] argv Argument list
 *
 * @returns Status code
 */
int main(int argc, const char *argv[])
{
    char         buf[MAX_LINE_BUF];
    char        *p;
    FILE        *fp;
    ib_status_t  rc;
    unsigned     failed_rule_num;


    /* Rule Initializations */
    rc = modua_rules_init( &failed_rule_num );
    if (rc != IB_OK) {
        fprintf(stderr,
                "User agent rule initialization failed on rule #%d: %d",
                failed_rule_num, rc);
    }

    /* Get the rules */
    modua_rules = modua_rules_get( );
    if (modua_rules == NULL) {
        fprintf(stderr, "Failed to get user agent rule list: %d", rc);
    }

    /* Parse command line. */
    if (argc != 2) {
        fprintf(stderr, "usage: user_agent <file>\n");
        exit(1);
    }

    /* Attempt to open the user agent file */
    fp = fopen(argv[1], "r");
    if (fp == NULL) {
        fprintf(stderr, "Failed to open file %s: %s\n",
                argv[1], strerror(errno));
        exit(1);
    }

    /* Read each line of the file, try to parse it as a user agent string. */
    while ( (p = fgets(buf, sizeof(buf), fp)) != NULL) {
        char       *product;
        char       *platform;
        char       *extra;
        const char *category;

        /* Strip off the trailing whitespace */
        char       *end = buf+strlen(buf)-1;
        while( (end > buf) && (isspace(*end) != 0) ) {
            --end;
        }
        *end = '\0';
        
        printf( "%s:\n", buf );

        /* Parse it */
        modua_parse_uastring(buf, &product, &platform, &extra);
        category = modua_match_cat_rules(product, platform, extra);

        /* Print the results */
        if (product != NULL) {
            printf( "  PRODUCT  = '%s'\n", product );
        }
        if (platform != NULL) {
            printf( "  PLATFORM = '%s'\n", platform );
        }
        if (extra != NULL) {
            printf( "  EXTRA    = '%s'\n", extra );
        }
        if (category != NULL) {
            printf( "  CATEGORY = '%s'\n", category );
        }
    }

    /* Done */
    fclose(fp);
    return 0;
}

#endif
