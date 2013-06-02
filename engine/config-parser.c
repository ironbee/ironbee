
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
#include <math.h>
#include <errno.h>

#include <ironbee/engine.h>
#include <ironbee/util.h>
#include <ironbee/config.h>
#include <ironbee/mpool.h>
#include <ironbee/path.h>

#include "config-parser.h"
#include "config_private.h"

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
 * Using the given mp, strdup the buffer in @a cp consiering quotes.
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
    if ((*start == '"') && (*end == '"') && (start < end)) {
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

/**
 * A table entry mapping a parse directive string to a handler function.
 */
struct parse_directive_entry_t {
    const char *directive;   /**< The directive. Case insensitive. */
    parse_directive_fn_t fn; /**< The handler function. */
};
typedef struct parse_directive_entry_t parse_directive_entry_t;

/**
 * Ensure that the node's file:line has not been encountered before.
 *
 * @param[in] cp Configuration parser.
 * @param[in] tmp_mp Temporary memory pool. 
 * @param[in] node The current parse node.
 * @returns
 * - IB_OK if the directive represented by @a node is new (not a dup).
 * - IB_EINVAL if we detect that @a node's file and line have been seen before.
 * - IB_EALLOC on allocation errors.
 * - Other if there is an internal IronBee error.
 */
static ib_status_t detect_file_loop(
    ib_cfgparser_t *cp,
    ib_cfgparser_node_t *node
) {
    assert(cp != NULL);
    assert(cp->mp != NULL);
    assert(node != NULL);
    assert(node->file != NULL);

    for (ib_cfgparser_node_t *node2 = node->parent;
         node2 != NULL;
         node2 = node2->parent)
    {
        /* If a node is at the same file and line, it is clearly a duplciate. */
        if (node2->type == IB_CFGPARSER_NODE_PARSE_DIRECTIVE
        &&  node->line == node2->line
        &&  strcmp(node->file, node2->file) == 0)
        {
            ib_cfg_log_error(
                cp,
                "File include cycle found at %s:%zu.",
                node->file,
                node->line);

            for (ib_cfgparser_node_t *node3 = node->parent;
                 node3 != NULL;
                 node3 = node3->parent)
            {
                /* Skip nodes that are not parse directives, 
                 * such as the root node and file nodes. */
                if (node3->type == IB_CFGPARSER_NODE_PARSE_DIRECTIVE) {
                    ib_cfg_log_error(
                        cp,
                        "\t... %s included from %s:%zu.",
                        node3->directive,
                        node3->file,
                        node3->line);
                }
            }

            return IB_EINVAL;
        }
    }

    return IB_OK;
}

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
    ib_cfgparser_node_t *node
) {
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
    const char* pval;
    const ib_list_node_t *list_node;
    ib_cfgparser_fsm_t fsm;
    ib_cfgparser_node_t *current_node;
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

    rc = detect_file_loop(cp, node);
    if (freeme != NULL) {
        free(freeme);
        freeme = NULL;
    }
    if (rc != IB_OK) {
        return rc;
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

    /* Make the given node the current node for the file inclusion. */
    current_node = cp->curr;
    cp->curr = node;
    fsm = cp->fsm;
    rc = ib_cfgparser_parse_private(cp, incfile, false);
    cp->fsm = fsm;
    cp->curr = current_node;
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error parsing included file \"%s\": %s",
	                 incfile, ib_status_to_string(rc));
        return rc;
    }

    ib_cfg_log_debug(cp, "Done processing include file \"%s\"", incfile);
    return IB_OK;
}

static ib_status_t loglevel_parse_directive(
    ib_cfgparser_t *cp,
    ib_mpool_t* tmp_mp,
    ib_cfgparser_node_t *node
) {
    assert(cp != NULL);
    assert(node != NULL);
    assert(node->directive != NULL);
    assert(node->params != NULL);

    ib_cfg_log_debug(cp, "Applying new log level.");

    return ib_config_directive_process(cp, node->directive, node->params);
}

/**
 * Null-terminated table that maps parsing directives to handler functions.
 */
static parse_directive_entry_t parse_directive_table[] = {
    { "IncludeIfExists", include_parse_directive },
    { "Include",         include_parse_directive },
    { "LogLevel",        loglevel_parse_directive },
    { NULL, NULL } /* Null termination. Do not remove. */
};


#line 625 "../../ironbee/engine/config-parser.rl"



#line 392 "../../ironbee/engine/config-parser.c"
static const char _ironbee_config_actions[] = {
	0, 1, 0, 1, 3, 1, 6, 1, 
	12, 1, 13, 1, 17, 1, 24, 1, 
	28, 1, 31, 1, 39, 1, 45, 1, 
	46, 1, 47, 1, 50, 1, 52, 2, 
	0, 36, 2, 0, 43, 2, 0, 44, 
	2, 1, 20, 2, 1, 21, 2, 2, 
	27, 2, 2, 28, 2, 3, 33, 2, 
	3, 41, 2, 3, 49, 2, 4, 51, 
	2, 7, 6, 2, 8, 34, 2, 8, 
	35, 2, 9, 26, 2, 10, 3, 2, 
	10, 42, 2, 10, 44, 2, 14, 6, 
	3, 1, 5, 11, 3, 3, 5, 19, 
	3, 3, 7, 6, 3, 3, 18, 1, 
	3, 3, 25, 2, 3, 3, 32, 8, 
	3, 3, 33, 8, 3, 3, 40, 10, 
	3, 4, 3, 48, 3, 4, 3, 49, 
	3, 7, 6, 3, 3, 14, 6, 3, 
	3, 14, 6, 16, 3, 14, 6, 23, 
	3, 14, 6, 30, 3, 14, 6, 38, 
	3, 14, 7, 6, 4, 1, 5, 11, 
	20, 4, 1, 5, 11, 21, 4, 3, 
	5, 19, 1, 4, 14, 3, 6, 16, 
	4, 14, 3, 6, 23, 4, 14, 3, 
	6, 30, 4, 14, 3, 6, 38, 4, 
	14, 3, 15, 6, 4, 14, 3, 22, 
	6, 4, 14, 3, 29, 6, 4, 14, 
	3, 37, 6, 4, 14, 7, 6, 3, 
	4, 14, 7, 6, 16, 4, 14, 7, 
	6, 23, 4, 14, 7, 6, 30, 4, 
	14, 7, 6, 38, 5, 14, 3, 7, 
	6, 16, 5, 14, 3, 7, 6, 30, 
	5, 14, 7, 6, 38, 3
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
	29, 85, 21, 23, 132, 132, 161, 136, 
	5, 5, 5, 191, 171, 15, 140, 5, 
	5, 5, 196, 176, 31, 144, 201, 181, 
	37, 148, 1, 55, 3, 206, 186, 152, 
	25, 58, 211, 0, 0, 0, 128, 61, 
	5, 124, 27, 120, 216, 11, 92, 236, 
	64, 96, 43, 5, 156, 40, 166, 100, 
	221, 13, 64, 73, 96, 49, 5, 46, 
	104, 226, 17, 52, 242, 96, 70, 5, 
	112, 67, 108, 231, 19, 3, 248, 0, 
	96, 82, 5, 34, 79, 76, 116
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
	0, 0, 0, 0, 0, 88, 88, 88, 
	0, 0, 0, 0, 0, 1, 0, 1, 
	1, 1, 0, 0, 0, 0, 0, 88, 
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


#line 628 "../../ironbee/engine/config-parser.rl"

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
    
#line 676 "../../ironbee/engine/config-parser.rl"
    
#line 677 "../../ironbee/engine/config-parser.rl"
    
#line 678 "../../ironbee/engine/config-parser.rl"
    
#line 679 "../../ironbee/engine/config-parser.rl"

    
#line 651 "../../ironbee/engine/config-parser.c"
	{
	 cp->fsm.cs = ironbee_config_start;
	 cp->fsm.top = 0;
	 cp->fsm.ts = 0;
	 cp->fsm.te = 0;
	 cp->fsm.act = 0;
	}

#line 681 "../../ironbee/engine/config-parser.rl"
    
#line 662 "../../ironbee/engine/config-parser.c"
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
	case 13:
#line 1 "NONE"
	{ cp->fsm.ts = ( fsm_vars.p);}
	break;
#line 683 "../../ironbee/engine/config-parser.c"
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
#line 387 "../../ironbee/engine/config-parser.rl"
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
#line 397 "../../ironbee/engine/config-parser.rl"
	{
        pval = qstrdup(cp, mpcfg);
        if (pval == NULL) {
            return IB_EALLOC;
        }
        ib_list_push(plist, pval);
    }
	break;
	case 2:
#line 404 "../../ironbee/engine/config-parser.rl"
	{
        pval = qstrdup(cp, mpcfg);
        if (pval == NULL) {
            return IB_EALLOC;
        }
        ib_list_push(plist, pval);
    }
	break;
	case 3:
#line 412 "../../ironbee/engine/config-parser.rl"
	{
        cp->curr->line += 1;
    }
	break;
	case 4:
#line 417 "../../ironbee/engine/config-parser.rl"
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
#line 425 "../../ironbee/engine/config-parser.rl"
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
        node->parent = cp->curr;
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
                cpbuf_clear(cp);
                rc = (parse_directive_table[i].fn)(cp, mptmp, node);
                if (rc != IB_OK) {
                    ib_cfg_log_error(
                        cp,
                        "Parse directive %s failed.",
                        node->directive);
                }
                else {
                    ib_cfg_log_debug(
                        cp,
                        "Parse directive %s succeeded.",
                        node->directive);
                }
            }
        }
    }
	break;
	case 6:
#line 478 "../../ironbee/engine/config-parser.rl"
	{
        if (cpbuf_append(cp, *( fsm_vars.p)) != IB_OK) {
            return IB_EALLOC;
        }
    }
	break;
	case 7:
#line 484 "../../ironbee/engine/config-parser.rl"
	{
        cpbuf_clear(cp);
    }
	break;
	case 8:
#line 489 "../../ironbee/engine/config-parser.rl"
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
#line 497 "../../ironbee/engine/config-parser.rl"
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
            ib_cfg_log_debug(
                cp,
                "Adding param \"%s\" to SBLK1 %s (node = %p)",
                (const char *)ib_list_node_data(lst_node),
                node->directive,
                node);
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
#line 531 "../../ironbee/engine/config-parser.rl"
	{
        ib_cfgparser_pop_node(cp);
        blkname = NULL;
    }
	break;
	case 11:
#line 568 "../../ironbee/engine/config-parser.rl"
	{ { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; goto _again;} }
	break;
	case 14:
#line 1 "NONE"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 15:
#line 560 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.act = 2;}
	break;
	case 16:
#line 568 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.act = 4;}
	break;
	case 17:
#line 559 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 18:
#line 560 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 19:
#line 562 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; goto _again;} }}
	break;
	case 20:
#line 568 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;}
	break;
	case 21:
#line 1 "NONE"
	{	switch(  cp->fsm.act ) {
	default:
	{{( fsm_vars.p) = (( cp->fsm.te))-1;}}
	break;
	}
	}
	break;
	case 22:
#line 573 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.act = 6;}
	break;
	case 23:
#line 576 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.act = 7;}
	break;
	case 24:
#line 572 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 25:
#line 573 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 26:
#line 578 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; goto _again;} }}
	break;
	case 27:
#line 576 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;}
	break;
	case 28:
#line 1 "NONE"
	{	switch(  cp->fsm.act ) {
	default:
	{{( fsm_vars.p) = (( cp->fsm.te))-1;}}
	break;
	}
	}
	break;
	case 29:
#line 584 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.act = 10;}
	break;
	case 30:
#line 591 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.act = 12;}
	break;
	case 31:
#line 582 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 32:
#line 584 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; goto _again;} }}
	break;
	case 33:
#line 586 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; goto _again;} }}
	break;
	case 34:
#line 584 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;{ { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; goto _again;} }}
	break;
	case 35:
#line 591 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;{ { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 28; goto _again;} }}
	break;
	case 36:
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
	case 37:
#line 596 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.act = 14;}
	break;
	case 38:
#line 600 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.act = 15;}
	break;
	case 39:
#line 595 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 40:
#line 596 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 41:
#line 604 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; goto _again;} }}
	break;
	case 42:
#line 600 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;}
	break;
	case 43:
#line 602 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;{ { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; goto _again;} }}
	break;
	case 44:
#line 1 "NONE"
	{	switch(  cp->fsm.act ) {
	default:
	{{( fsm_vars.p) = (( cp->fsm.te))-1;}}
	break;
	}
	}
	break;
	case 45:
#line 617 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ ( fsm_vars.p)--; { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 32; goto _again;}}}
	break;
	case 46:
#line 618 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{        { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 37; goto _again;}}}
	break;
	case 47:
#line 621 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 48:
#line 622 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 49:
#line 623 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 50:
#line 608 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;}
	break;
	case 51:
#line 614 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;{ { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 23; goto _again;} }}
	break;
	case 52:
#line 614 "../../ironbee/engine/config-parser.rl"
	{{( fsm_vars.p) = (( cp->fsm.te))-1;}{ { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 23; goto _again;} }}
	break;
#line 1104 "../../ironbee/engine/config-parser.c"
		}
	}

_again:
	_acts = _ironbee_config_actions + _ironbee_config_to_state_actions[ cp->fsm.cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 12:
#line 1 "NONE"
	{ cp->fsm.ts = 0;}
	break;
#line 1117 "../../ironbee/engine/config-parser.c"
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
#line 387 "../../ironbee/engine/config-parser.rl"
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
#line 397 "../../ironbee/engine/config-parser.rl"
	{
        pval = qstrdup(cp, mpcfg);
        if (pval == NULL) {
            return IB_EALLOC;
        }
        ib_list_push(plist, pval);
    }
	break;
	case 5:
#line 425 "../../ironbee/engine/config-parser.rl"
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
        node->parent = cp->curr;
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
                cpbuf_clear(cp);
                rc = (parse_directive_table[i].fn)(cp, mptmp, node);
                if (rc != IB_OK) {
                    ib_cfg_log_error(
                        cp,
                        "Parse directive %s failed.",
                        node->directive);
                }
                else {
                    ib_cfg_log_debug(
                        cp,
                        "Parse directive %s succeeded.",
                        node->directive);
                }
            }
        }
    }
	break;
	case 11:
#line 568 "../../ironbee/engine/config-parser.rl"
	{ { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; goto _again;} }
	break;
#line 1216 "../../ironbee/engine/config-parser.c"
		}
	}
	}

	_out: {}
	}

#line 682 "../../ironbee/engine/config-parser.rl"

    /* Ensure that our block is always empty on last chunk. */
    if ( is_last_chunk && blkname != NULL ) {
        return IB_EINVAL;
    }

    return rc;
}
