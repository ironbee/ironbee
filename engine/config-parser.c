
#line 1 "../../ironbee/engine/config-parser.rl"
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
 * @brief IronBee --- Configuration File Parser
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>
#include <errno.h>

#include <ironbee/engine.h>
#include <ironbee/util.h>
#include <ironbee/config.h>
#include <ironbee/mpool.h>
#include <ironbee/path.h>

#include "config-parser.h"

/* Caused by Ragel */
#ifdef __clang__
#pragma clang diagnostic ignored "-Wunreachable-code"
#endif

/**
 * Variables used by the finite state machine per-call.
 * Values here do not have to persist across calls to
 * ib_cfgparser_ragel_parse_chunk().
 */
typedef struct {
    const char *p;     /**< Pointer to the chunk being parsed. */
    const char *pe;    /**< Pointer past the end of p (p+length(p)). */
    const char *eof;   /**< eof==p==pe on last chunk. NULL otherwise. */
} fsm_vars_t;

/**
 * Append @a c to the internal buffer of @a cp. 
 * @param[in] cp Configuration parser.
 * @param[in] c Char to append.
 * @returns
 * - IB_OK
 * - IB_EALLOC if there is no space left in the buffer.
 */
static ib_status_t cpbuf_append(ib_cfgparser_t *cp, char c)
{
    assert(cp != NULL);
    assert(cp->buffer != NULL);
    assert (cp->buffer_sz >= cp->buffer_len);

    if (cp->buffer_sz == cp->buffer_len) {
        return IB_EALLOC;
    }

    cp->buffer[cp->buffer_len] = c;
    ++(cp->buffer_len);

    return IB_OK;
}

/**
 * Clear the buffer in @a cp.
 * @param[in] cp Configuration parser.
 */
static void cpbuf_clear(ib_cfgparser_t *cp) {
    assert(cp != NULL);
    assert(cp->buffer != NULL);

    cp->buffer_len = 0;
    cp->buffer[0] = '\0';
}

/**
 * Using the given mp, strdup the buffer in @a cp.
 *
 * If the buffer starts and ends with double quotes, remove them.
 *
 * @param[in] cp The configuration parser
 * @param[in,out] mp Pool to copy out of.
 *
 * @return a buffer allocated from the tmpmp memory pool
 *         available in ib_cfgparser_ragel_parse_chunk. This buffer may be
 *         larger than the string stored in it if the length of the string is
 *         reduced by Javascript unescaping.
 */
static char* qstrdup(ib_cfgparser_t *cp, ib_mpool_t* mp)
{
    const char *start = cp->buffer;
    const char *end = cp->buffer + cp->buffer_len - 1;
    size_t len = cp->buffer_len;

    /* Adjust for quoted value. */
    if ((*start == '"') && (*end == '"') && (start < end))
    {
        start++;
        len -= 2;
    }

    return ib_mpool_memdup_to_str(mp, start, len);
}

/**
 * Callback function to handel parsing of parser directives.
 */
typedef ib_status_t(*parse_directive_fn_t)(
    ib_cfgparser_t *cp,
    ib_mpool_t* tmp_mp,
    ib_cfgparser_node_t *node);

struct parse_directive_entry_t {
    const char *directive;
    parse_directive_fn_t fn;
};
typedef struct parse_directive_entry_t parse_directive_entry_t;

/**
 * Process "Include" and "IncludeIfExists" parse directives.
 * param[in] cp Configuration parser.
 * param[in] mp Memory pool to use.
 * param[in] node The parse node containing the directive.
 *
 * @returns
 * - IB_OK on success.
 * - Any other code causes a general failure to be repoted but the
 *   parse continues.
 */
static ib_status_t include_parse_directive(
    ib_cfgparser_t *cp,
    ib_mpool_t* tmp_mp,
    ib_cfgparser_node_t *node)
{
    assert(cp != NULL);
    assert(cp->mp != NULL);
    assert(node != NULL);
    assert(node->directive != NULL);
    assert(node->params != NULL);
    assert(node->file != NULL);

    struct stat statbuf;
    ib_status_t rc;
    ib_mpool_t *mp = cp->mp;
    int statval;
    char *incfile;
    char *real;
    char *lookup;
    const char* pval;
    const ib_list_node_t *list_node;
    bool if_exists;
    /* Some utilities we use employ malloc (eg realpath()).
     * If a malloc'ed buffer is in use, we alias it here to be free'ed. */
    void *freeme = NULL; 

    if (ib_list_elements(node->params) != 1) {
        ib_cfg_log_error(
            cp,
            "%s: %zu - Directive %s only takes 1 parameter not %zu.",
            node->file,
            node->line,
            node->directive,
            ib_list_elements(node->params));
        return IB_EINVAL;
    }

    /* Grab the first parameter node. */
    list_node = ib_list_first_const(node->params);
    assert(list_node != NULL);

    /* Grab the parameter value. */
    pval = (const char*) ib_list_node_data_const(list_node);
    assert(pval != NULL);

    if_exists = strcasecmp("IncludeIfExists", node->directive) == 0;

    incfile = ib_util_relative_file(mp, node->file, pval);
    if (incfile == NULL) {
        ib_cfg_log_error(cp, "Failed to resolve included file \"%s\": %s",
                         node->file, strerror(errno));
        return IB_ENOENT;
    }

    real = realpath(incfile, NULL);
    if (real == NULL) {
        if (!if_exists) {
            ib_cfg_log_error(cp,
                             "Failed to find real path of included file "
                             "(using original \"%s\"): %s",
                             incfile, strerror(errno));
        }

        real = incfile;
    }
    else {
        if (strcmp(real, incfile) != 0) {
            ib_cfg_log_info(cp,
                            "Real path of included file \"%s\" is \"%s\"",
                            incfile, real);
        }
        freeme = real;
    }

    /* Look up the real file path in the hash */
    rc = ib_hash_get(cp->includes, &lookup, real);
    if (rc == IB_OK) {
        ib_cfg_log_warning(cp,
                           "Included file \"%s\" already included: skipping",
                           real);
        return IB_OK;
    }
    else if (rc != IB_ENOENT) {
        ib_cfg_log_error(cp, "Error looking up include file \"%s\": %s",
                         real, strerror(errno));
    }

    /* Put the real name in the hash */
    lookup = ib_mpool_strdup(mp, real);
    if (freeme != NULL) {
        free(freeme);
        freeme = NULL;
    }
    if (lookup != NULL) {
        rc = ib_hash_set(cp->includes, lookup, lookup);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp,
                             "Error adding include file to hash \"%s\": %s",
                             lookup, strerror(errno));
        }
    }

    if (access(incfile, R_OK) != 0) {
        if (if_exists) {
            ib_cfg_log(cp, (errno == ENOENT) ? IB_LOG_DEBUG : IB_LOG_NOTICE,
                       "Ignoring include file \"%s\": %s",
                       incfile, strerror(errno));
            return IB_OK;
        }

        ib_cfg_log_error(cp, "Cannot access included file \"%s\": %s",
                         incfile, strerror(errno));
        return IB_ENOENT;
    }

    statval = stat(incfile, &statbuf);
    if (statval != 0) {
        if (if_exists) {
            ib_cfg_log(cp, (errno == ENOENT) ? IB_LOG_DEBUG : IB_LOG_NOTICE,
                       "Ignoring include file \"%s\": %s",
                       incfile, strerror(errno));
            return IB_OK;
        }

        ib_cfg_log_error(cp,
                         "Failed to stat include file \"%s\": %s",
                         incfile, strerror(errno));
        return IB_ENOENT;
    }

    if (S_ISREG(statbuf.st_mode) == 0) {
        if (if_exists) {
            ib_cfg_log_info(cp,
                            "Ignoring include file \"%s\": Not a regular file",
                            incfile);
            return IB_OK;
        }

        ib_cfg_log_error(cp,
	                 "Included file \"%s\" is not a regular file", incfile);
        return IB_ENOENT;
    }

    ib_cfg_log_debug(cp, "Including '%s'", incfile);
    rc = ib_cfgparser_parse(cp, incfile);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error parsing included file \"%s\": %s",
	                 incfile, ib_status_to_string(rc));
        return rc;
    }

    ib_cfg_log_debug(cp, "Done processing include file \"%s\"", incfile);
    return IB_OK;
}

/**
 * Table of handlers for parsing directives.
 */
static parse_directive_entry_t parse_directive_table[] = {
    { "IncludeIfExists", include_parse_directive },
    { "Include",         include_parse_directive },
    { NULL, NULL } /* Null termination. Do not remove. */
};


#line 542 "../../ironbee/engine/config-parser.rl"



#line 324 "../../ironbee/engine/config-parser.c"
static const char _ironbee_config_actions[] = {
	0, 1, 0, 1, 3, 1, 6, 1, 
	11, 1, 12, 1, 16, 1, 23, 1, 
	27, 1, 30, 1, 38, 1, 44, 1, 
	45, 1, 46, 1, 49, 1, 51, 2, 
	0, 35, 2, 0, 42, 2, 0, 43, 
	2, 1, 5, 2, 1, 19, 2, 1, 
	20, 2, 2, 26, 2, 2, 27, 2, 
	3, 32, 2, 3, 40, 2, 3, 48, 
	2, 4, 50, 2, 7, 6, 2, 8, 
	33, 2, 8, 34, 2, 9, 25, 2, 
	10, 3, 2, 10, 41, 2, 10, 43, 
	2, 13, 6, 3, 1, 5, 19, 3, 
	1, 5, 20, 3, 3, 5, 18, 3, 
	3, 7, 6, 3, 3, 17, 1, 3, 
	3, 24, 2, 3, 3, 31, 8, 3, 
	3, 32, 8, 3, 3, 39, 10, 3, 
	4, 3, 47, 3, 4, 3, 48, 3, 
	7, 6, 3, 3, 13, 6, 3, 3, 
	13, 6, 15, 3, 13, 6, 22, 3, 
	13, 6, 29, 3, 13, 6, 37, 3, 
	13, 7, 6, 4, 3, 5, 18, 1, 
	4, 13, 3, 6, 15, 4, 13, 3, 
	6, 22, 4, 13, 3, 6, 29, 4, 
	13, 3, 6, 37, 4, 13, 3, 14, 
	6, 4, 13, 3, 21, 6, 4, 13, 
	3, 28, 6, 4, 13, 3, 36, 6, 
	4, 13, 7, 6, 3, 4, 13, 7, 
	6, 15, 4, 13, 7, 6, 22, 4, 
	13, 7, 6, 29, 4, 13, 7, 6, 
	37, 5, 13, 3, 7, 6, 15, 5, 
	13, 3, 7, 6, 29, 5, 13, 7, 
	6, 37, 3
};

static const unsigned char _ironbee_config_key_offsets[] = {
	0, 0, 0, 1, 3, 3, 5, 5, 
	7, 7, 9, 9, 11, 11, 13, 13, 
	15, 16, 18, 27, 34, 41, 42, 49, 
	58, 65, 72, 72, 79, 87, 94, 94, 
	101, 110, 117, 124, 131, 138, 147, 154, 
	154, 161
};

static const char _ironbee_config_trans_keys[] = {
	47, 10, 13, 34, 92, 10, 13, 34, 
	92, 10, 13, 10, 13, 10, 13, 10, 
	10, 13, 9, 10, 13, 32, 34, 35, 
	60, 62, 92, 32, 34, 60, 62, 92, 
	9, 10, 9, 10, 32, 34, 60, 62, 
	92, 10, 9, 10, 32, 34, 60, 62, 
	92, 9, 10, 13, 32, 34, 35, 60, 
	62, 92, 32, 34, 60, 62, 92, 9, 
	10, 9, 10, 32, 34, 60, 62, 92, 
	9, 10, 32, 34, 60, 62, 92, 9, 
	10, 32, 34, 35, 60, 62, 92, 32, 
	34, 60, 62, 92, 9, 10, 9, 10, 
	32, 34, 60, 62, 92, 9, 10, 13, 
	32, 60, 62, 92, 34, 35, 32, 34, 
	60, 62, 92, 9, 10, 9, 10, 32, 
	34, 60, 62, 92, 32, 34, 60, 62, 
	92, 9, 10, 9, 10, 32, 34, 60, 
	62, 92, 9, 10, 13, 32, 60, 62, 
	92, 34, 35, 32, 34, 60, 62, 92, 
	9, 10, 9, 10, 32, 34, 60, 62, 
	92, 9, 10, 32, 34, 60, 62, 92, 
	0
};

static const char _ironbee_config_single_lengths[] = {
	0, 0, 1, 2, 0, 2, 0, 2, 
	0, 2, 0, 2, 0, 2, 0, 2, 
	1, 2, 9, 5, 7, 1, 7, 9, 
	5, 7, 0, 7, 8, 5, 0, 7, 
	7, 5, 7, 5, 7, 7, 5, 0, 
	7, 7
};

static const char _ironbee_config_range_lengths[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 1, 0, 0, 0, 0, 
	1, 0, 0, 0, 0, 1, 0, 0, 
	1, 1, 0, 1, 0, 1, 1, 0, 
	0, 0
};

static const unsigned char _ironbee_config_index_offsets[] = {
	0, 0, 1, 3, 6, 7, 10, 11, 
	14, 15, 18, 19, 22, 23, 26, 27, 
	30, 32, 35, 45, 52, 60, 62, 70, 
	80, 87, 95, 96, 104, 113, 120, 121, 
	129, 138, 145, 153, 160, 168, 177, 184, 
	185, 193
};

static const char _ironbee_config_indicies[] = {
	1, 3, 2, 4, 5, 1, 7, 9, 
	10, 8, 8, 11, 12, 7, 14, 16, 
	17, 15, 15, 18, 19, 14, 21, 22, 
	23, 21, 25, 27, 28, 26, 27, 26, 
	29, 30, 25, 32, 33, 34, 32, 36, 
	35, 37, 36, 38, 31, 39, 39, 39, 
	39, 40, 39, 1, 39, 41, 39, 39, 
	39, 39, 40, 1, 42, 35, 39, 43, 
	39, 39, 39, 39, 40, 1, 45, 46, 
	47, 45, 48, 36, 36, 36, 49, 44, 
	50, 50, 50, 50, 51, 50, 7, 53, 
	54, 53, 53, 53, 53, 51, 7, 53, 
	53, 55, 53, 53, 53, 53, 51, 7, 
	57, 36, 57, 58, 36, 36, 59, 60, 
	56, 61, 61, 61, 61, 62, 61, 14, 
	63, 63, 64, 63, 63, 63, 63, 62, 
	14, 66, 67, 68, 66, 26, 26, 69, 
	26, 65, 70, 70, 70, 70, 71, 70, 
	21, 70, 72, 70, 70, 70, 70, 71, 
	21, 73, 73, 73, 73, 71, 73, 21, 
	70, 74, 70, 70, 70, 70, 71, 21, 
	76, 77, 78, 76, 26, 79, 80, 26, 
	75, 81, 81, 81, 81, 82, 81, 25, 
	83, 84, 85, 84, 84, 84, 84, 82, 
	25, 84, 86, 84, 84, 84, 84, 82, 
	25, 0
};

static const char _ironbee_config_trans_targs[] = {
	18, 19, 18, 18, 19, 22, 23, 24, 
	5, 26, 6, 24, 27, 28, 29, 9, 
	30, 10, 29, 31, 32, 33, 35, 36, 
	37, 38, 0, 37, 16, 38, 41, 19, 
	18, 18, 20, 21, 0, 2, 3, 18, 
	1, 18, 18, 18, 24, 23, 23, 25, 
	5, 7, 23, 4, 23, 23, 23, 23, 
	29, 28, 9, 28, 11, 28, 8, 28, 
	28, 33, 32, 32, 34, 13, 32, 12, 
	32, 32, 32, 38, 37, 39, 40, 15, 
	17, 37, 14, 37, 37, 39, 37
};

static const unsigned char _ironbee_config_trans_actions[] = {
	29, 88, 21, 23, 139, 139, 95, 143, 
	5, 5, 5, 188, 168, 15, 147, 5, 
	5, 5, 193, 173, 31, 151, 198, 178, 
	37, 155, 1, 58, 3, 203, 183, 159, 
	25, 61, 208, 0, 0, 0, 135, 64, 
	5, 131, 27, 127, 213, 11, 99, 233, 
	67, 103, 46, 5, 91, 43, 163, 107, 
	218, 13, 67, 76, 103, 52, 5, 49, 
	111, 223, 17, 55, 239, 103, 73, 5, 
	119, 70, 115, 228, 19, 3, 245, 0, 
	103, 85, 5, 34, 82, 79, 123
};

static const unsigned char _ironbee_config_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 7, 0, 0, 0, 0, 7, 
	0, 0, 0, 0, 7, 0, 0, 0, 
	7, 0, 0, 0, 0, 7, 0, 0, 
	0, 0
};

static const unsigned char _ironbee_config_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 9, 0, 0, 0, 0, 9, 
	0, 0, 0, 0, 9, 0, 0, 0, 
	9, 0, 0, 0, 0, 9, 0, 0, 
	0, 0
};

static const unsigned char _ironbee_config_eof_actions[] = {
	0, 0, 0, 0, 0, 40, 40, 40, 
	0, 0, 0, 0, 0, 1, 0, 1, 
	1, 1, 0, 0, 0, 0, 0, 40, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0
};

static const unsigned char _ironbee_config_eof_trans[] = {
	0, 1, 0, 0, 7, 0, 0, 0, 
	14, 0, 0, 0, 21, 0, 25, 0, 
	0, 0, 0, 40, 40, 43, 40, 0, 
	7, 53, 53, 53, 0, 62, 64, 64, 
	0, 71, 71, 74, 71, 0, 82, 84, 
	85, 85
};

static const int ironbee_config_start = 18;
static const int ironbee_config_first_final = 18;
static const int ironbee_config_error = 0;

static const int ironbee_config_en_parameters = 23;
static const int ironbee_config_en_block_parameters = 28;
static const int ironbee_config_en_newblock = 32;
static const int ironbee_config_en_endblock = 37;
static const int ironbee_config_en_main = 18;


#line 545 "../../ironbee/engine/config-parser.rl"

ib_status_t ib_cfgparser_ragel_parse_chunk(ib_cfgparser_t *cp,
                                           const char *buf,
                                           const size_t blen,
                                           const int is_last_chunk)
{
    assert(cp != NULL);
    assert(cp->ib != NULL);

    ib_engine_t *ib_engine = cp->ib;

    /* Temporary memory pool. */
    ib_mpool_t *mptmp = ib_engine_pool_temp_get(ib_engine);

    /* Configuration memory pool. */
    ib_mpool_t *mpcfg = ib_engine_pool_config_get(ib_engine);

    /* Error actions will update this. */
    ib_status_t rc = IB_OK;

    /* Directive name being parsed. */
    char *directive = NULL;

    /* Block name being parsed. */
    char *blkname = NULL;

    /* Parameter value being added to the plist. */
    char *pval = NULL;

    /* Temporary list for storing values before they are committed to the
     * configuration. */
    ib_list_t *plist;

    /* Create a finite state machine type. */
    fsm_vars_t fsm_vars;

    fsm_vars.p = buf;
    fsm_vars.pe = buf + blen;
    fsm_vars.eof = (is_last_chunk ? fsm_vars.pe : NULL);

    /* Create a temporary list for storing parameter values. */
    ib_list_create(&plist, mptmp);
    if (plist == NULL) {
        return IB_EALLOC;
    }

    /* Access all ragel state variables via structure. */
    
#line 593 "../../ironbee/engine/config-parser.rl"
    
#line 594 "../../ironbee/engine/config-parser.rl"
    
#line 595 "../../ironbee/engine/config-parser.rl"
    
#line 596 "../../ironbee/engine/config-parser.rl"

    
#line 583 "../../ironbee/engine/config-parser.c"
	{
	 cp->fsm.cs = ironbee_config_start;
	 cp->fsm.top = 0;
	 cp->fsm.ts = 0;
	 cp->fsm.te = 0;
	 cp->fsm.act = 0;
	}

#line 598 "../../ironbee/engine/config-parser.rl"
    
#line 594 "../../ironbee/engine/config-parser.c"
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const char *_keys;

	if ( ( fsm_vars.p) == ( fsm_vars.pe) )
		goto _test_eof;
	if (  cp->fsm.cs == 0 )
		goto _out;
_resume:
	_acts = _ironbee_config_actions + _ironbee_config_from_state_actions[ cp->fsm.cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 12:
#line 1 "NONE"
	{ cp->fsm.ts = ( fsm_vars.p);}
	break;
#line 615 "../../ironbee/engine/config-parser.c"
		}
	}

	_keys = _ironbee_config_trans_keys + _ironbee_config_key_offsets[ cp->fsm.cs];
	_trans = _ironbee_config_index_offsets[ cp->fsm.cs];

	_klen = _ironbee_config_single_lengths[ cp->fsm.cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + _klen - 1;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + ((_upper-_lower) >> 1);
			if ( (*( fsm_vars.p)) < *_mid )
				_upper = _mid - 1;
			else if ( (*( fsm_vars.p)) > *_mid )
				_lower = _mid + 1;
			else {
				_trans += (_mid - _keys);
				goto _match;
			}
		}
		_keys += _klen;
		_trans += _klen;
	}

	_klen = _ironbee_config_range_lengths[ cp->fsm.cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + (_klen<<1) - 2;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + (((_upper-_lower) >> 1) & ~1);
			if ( (*( fsm_vars.p)) < _mid[0] )
				_upper = _mid - 2;
			else if ( (*( fsm_vars.p)) > _mid[1] )
				_lower = _mid + 2;
			else {
				_trans += ((_mid - _keys)>>1);
				goto _match;
			}
		}
		_trans += _klen;
	}

_match:
	_trans = _ironbee_config_indicies[_trans];
_eof_trans:
	 cp->fsm.cs = _ironbee_config_trans_targs[_trans];

	if ( _ironbee_config_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _ironbee_config_actions + _ironbee_config_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 0:
#line 319 "../../ironbee/engine/config-parser.rl"
	{
        rc = IB_EOTHER;
        ib_cfg_log_error(
            cp,
            "parser error near %s:%zu.",
            cp->curr->file,
            cp->curr->line);
    }
	break;
	case 1:
#line 329 "../../ironbee/engine/config-parser.rl"
	{
        pval = qstrdup(cp, mpcfg);
        if (pval == NULL) {
            return IB_EALLOC;
        }
        ib_list_push(plist, pval);
    }
	break;
	case 2:
#line 336 "../../ironbee/engine/config-parser.rl"
	{
        pval = qstrdup(cp, mpcfg);
        if (pval == NULL) {
            return IB_EALLOC;
        }
        ib_list_push(plist, pval);
    }
	break;
	case 3:
#line 344 "../../ironbee/engine/config-parser.rl"
	{
        cp->curr->line += 1;
    }
	break;
	case 4:
#line 349 "../../ironbee/engine/config-parser.rl"
	{
        directive = ib_mpool_memdup_to_str(cp->mp, cp->buffer, cp->buffer_len);
        if (directive == NULL) {
            return IB_EALLOC;
        }
        ib_list_clear(plist);
        cpbuf_clear(cp);
    }
	break;
	case 5:
#line 357 "../../ironbee/engine/config-parser.rl"
	{
        ib_cfgparser_node_t *node = NULL;
        ib_cfgparser_node_create(&node, cp);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp, "Cannot create node.");
            return rc;
        }
        node->directive = directive;
        directive = NULL;
        node->file = ib_mpool_strdup(cp->mp, cp->curr->file);
        if (node->file == NULL) {
            return IB_EALLOC;
        }
        node->line = cp->curr->line;
        node->type = IB_CFGPARSER_NODE_DIRECTIVE;
        ib_list_node_t *lst_node;
        IB_LIST_LOOP(plist, lst_node) {
            ib_list_push(node->params, ib_list_node_data(lst_node));
        }
        ib_list_push(cp->curr->children, node);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp, "Out of memory.");
        }
        
        /* Handle parse directives using the parse_directive_table. */
        for (int i = 0; parse_directive_table[i].directive != NULL; ++i) {
            if (
                strcasecmp(
                    parse_directive_table[i].directive,
                    node->directive) == 0
            ) {
                /* Change the node type. This is an parse directive. */
                node->type = IB_CFGPARSER_NODE_PARSE_DIRECTIVE;
                /* Process directive. */
                rc = (parse_directive_table[i].fn)(cp, mptmp, node);
                if (rc != IB_OK) {
                    ib_cfg_log_error(
                        cp,
                        "Parse directive %s failed.",
                        node->directive);
                }
            }
        }
    }
	break;
	case 6:
#line 402 "../../ironbee/engine/config-parser.rl"
	{
        if (cpbuf_append(cp, *( fsm_vars.p)) != IB_OK) {
            return IB_EALLOC;
        }
    }
	break;
	case 7:
#line 408 "../../ironbee/engine/config-parser.rl"
	{
        cpbuf_clear(cp);
    }
	break;
	case 8:
#line 413 "../../ironbee/engine/config-parser.rl"
	{
        blkname = ib_mpool_memdup_to_str(cp->mp, cp->buffer, cp->buffer_len);
        if (blkname == NULL) {
            return IB_EALLOC;
        }
        ib_list_clear(plist);
        cpbuf_clear(cp);
    }
	break;
	case 9:
#line 421 "../../ironbee/engine/config-parser.rl"
	{
        ib_cfgparser_node_t *node = NULL;
        rc = ib_cfgparser_node_create(&node, cp);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp, "Cannot create node.");
            return rc;
        }
        node->directive = blkname;
        node->file = ib_mpool_strdup(cp->mp, cp->curr->file);
        if (node->file == NULL) {
            return IB_EALLOC;
        }
        node->line = cp->curr->line;
        node->type = IB_CFGPARSER_NODE_BLOCK;
        ib_list_node_t *lst_node;
        IB_LIST_LOOP(plist, lst_node) {
            rc = ib_list_push(node->params, ib_list_node_data(lst_node));
            if (rc != IB_OK) {
                ib_cfg_log_error(cp, "Cannot push directive.");
                return rc;
            }
        }
        rc = ib_cfgparser_push_node(cp, node);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp, "Cannot push node.");
            return rc;
        }
    }
	break;
	case 10:
#line 449 "../../ironbee/engine/config-parser.rl"
	{
        ib_cfgparser_pop_node(cp);
        blkname = NULL;
    }
	break;
	case 13:
#line 1 "NONE"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 14:
#line 478 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.act = 2;}
	break;
	case 15:
#line 485 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.act = 4;}
	break;
	case 16:
#line 477 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 17:
#line 478 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 18:
#line 480 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; goto _again;} }}
	break;
	case 19:
#line 485 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;}
	break;
	case 20:
#line 1 "NONE"
	{	switch(  cp->fsm.act ) {
	default:
	{{( fsm_vars.p) = (( cp->fsm.te))-1;}}
	break;
	}
	}
	break;
	case 21:
#line 490 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.act = 6;}
	break;
	case 22:
#line 493 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.act = 7;}
	break;
	case 23:
#line 489 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 24:
#line 490 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 25:
#line 495 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; goto _again;} }}
	break;
	case 26:
#line 493 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;}
	break;
	case 27:
#line 1 "NONE"
	{	switch(  cp->fsm.act ) {
	default:
	{{( fsm_vars.p) = (( cp->fsm.te))-1;}}
	break;
	}
	}
	break;
	case 28:
#line 501 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.act = 10;}
	break;
	case 29:
#line 508 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.act = 12;}
	break;
	case 30:
#line 499 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 31:
#line 501 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; goto _again;} }}
	break;
	case 32:
#line 503 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; goto _again;} }}
	break;
	case 33:
#line 501 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;{ { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; goto _again;} }}
	break;
	case 34:
#line 508 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;{ { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 28; goto _again;} }}
	break;
	case 35:
#line 1 "NONE"
	{	switch(  cp->fsm.act ) {
	case 10:
	{{( fsm_vars.p) = (( cp->fsm.te))-1;} { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; goto _again;} }
	break;
	case 12:
	{{( fsm_vars.p) = (( cp->fsm.te))-1;} { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 28; goto _again;} }
	break;
	}
	}
	break;
	case 36:
#line 513 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.act = 14;}
	break;
	case 37:
#line 517 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.act = 15;}
	break;
	case 38:
#line 512 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 39:
#line 513 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 40:
#line 521 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; goto _again;} }}
	break;
	case 41:
#line 517 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;}
	break;
	case 42:
#line 519 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;{ { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; goto _again;} }}
	break;
	case 43:
#line 1 "NONE"
	{	switch(  cp->fsm.act ) {
	default:
	{{( fsm_vars.p) = (( cp->fsm.te))-1;}}
	break;
	}
	}
	break;
	case 44:
#line 534 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ ( fsm_vars.p)--; { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 32; goto _again;}}}
	break;
	case 45:
#line 535 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{        { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 37; goto _again;}}}
	break;
	case 46:
#line 538 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 47:
#line 539 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 48:
#line 540 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 49:
#line 525 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;}
	break;
	case 50:
#line 531 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;{ { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 23; goto _again;} }}
	break;
	case 51:
#line 531 "../../ironbee/engine/config-parser.rl"
	{{( fsm_vars.p) = (( cp->fsm.te))-1;}{ { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 23; goto _again;} }}
	break;
#line 1018 "../../ironbee/engine/config-parser.c"
		}
	}

_again:
	_acts = _ironbee_config_actions + _ironbee_config_to_state_actions[ cp->fsm.cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 11:
#line 1 "NONE"
	{ cp->fsm.ts = 0;}
	break;
#line 1031 "../../ironbee/engine/config-parser.c"
		}
	}

	if (  cp->fsm.cs == 0 )
		goto _out;
	if ( ++( fsm_vars.p) != ( fsm_vars.pe) )
		goto _resume;
	_test_eof: {}
	if ( ( fsm_vars.p) == ( fsm_vars.eof) )
	{
	if ( _ironbee_config_eof_trans[ cp->fsm.cs] > 0 ) {
		_trans = _ironbee_config_eof_trans[ cp->fsm.cs] - 1;
		goto _eof_trans;
	}
	const char *__acts = _ironbee_config_actions + _ironbee_config_eof_actions[ cp->fsm.cs];
	unsigned int __nacts = (unsigned int) *__acts++;
	while ( __nacts-- > 0 ) {
		switch ( *__acts++ ) {
	case 0:
#line 319 "../../ironbee/engine/config-parser.rl"
	{
        rc = IB_EOTHER;
        ib_cfg_log_error(
            cp,
            "parser error near %s:%zu.",
            cp->curr->file,
            cp->curr->line);
    }
	break;
	case 1:
#line 329 "../../ironbee/engine/config-parser.rl"
	{
        pval = qstrdup(cp, mpcfg);
        if (pval == NULL) {
            return IB_EALLOC;
        }
        ib_list_push(plist, pval);
    }
	break;
	case 5:
#line 357 "../../ironbee/engine/config-parser.rl"
	{
        ib_cfgparser_node_t *node = NULL;
        ib_cfgparser_node_create(&node, cp);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp, "Cannot create node.");
            return rc;
        }
        node->directive = directive;
        directive = NULL;
        node->file = ib_mpool_strdup(cp->mp, cp->curr->file);
        if (node->file == NULL) {
            return IB_EALLOC;
        }
        node->line = cp->curr->line;
        node->type = IB_CFGPARSER_NODE_DIRECTIVE;
        ib_list_node_t *lst_node;
        IB_LIST_LOOP(plist, lst_node) {
            ib_list_push(node->params, ib_list_node_data(lst_node));
        }
        ib_list_push(cp->curr->children, node);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp, "Out of memory.");
        }
        
        /* Handle parse directives using the parse_directive_table. */
        for (int i = 0; parse_directive_table[i].directive != NULL; ++i) {
            if (
                strcasecmp(
                    parse_directive_table[i].directive,
                    node->directive) == 0
            ) {
                /* Change the node type. This is an parse directive. */
                node->type = IB_CFGPARSER_NODE_PARSE_DIRECTIVE;
                /* Process directive. */
                rc = (parse_directive_table[i].fn)(cp, mptmp, node);
                if (rc != IB_OK) {
                    ib_cfg_log_error(
                        cp,
                        "Parse directive %s failed.",
                        node->directive);
                }
            }
        }
    }
	break;
#line 1118 "../../ironbee/engine/config-parser.c"
		}
	}
	}

	_out: {}
	}

#line 599 "../../ironbee/engine/config-parser.rl"

    /* Ensure that our block is always empty on last chunk. */
    if ( is_last_chunk && blkname != NULL ) {
        return IB_EINVAL;
    }

    return rc;
}
