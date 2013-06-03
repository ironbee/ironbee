
#line 1 "/home/nleroy/devel/ib/engine/config-parser.rl"
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
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>
#include <limits.h>
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
        ib_cfg_log_error(cp, "Appending past the end of our buffer.");
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
 * @return a buffer allocated from the temp_mp memory pool
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
 * Implementation of "Include" and "IncludeIfExists" parse directives.
 * param[in] cp Configuration parser.
 * param[in] mp Memory pool to use.
 * param[in] node The parse node containing the directive.
 *
 * @returns
 * - IB_OK on success.
 * - Any other code causes a general failure to be repoted but the
 *   parse continues.
 */
static ib_status_t include_parse_directive_impl(
    ib_cfgparser_t *cp,
    ib_mpool_t *tmp_mp,
    ib_cfgparser_node_t *node,
    bool if_exists
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

    /* A temporary local value to store the parser state in.
     * We allocate this from local_mp to avoid putting the very large 
     * buffer variable in fsm on the stack. */
    ib_cfgparser_fsm_t *fsm;

    ib_cfgparser_node_t *current_node;
    ib_mpool_t *local_mp = NULL;

    rc = ib_mpool_create(&local_mp, "local_mp", tmp_mp);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Failed to create local memory pool.");
        goto cleanup;
    }

    if (ib_list_elements(node->params) != 1) {
        ib_cfg_log_error(
            cp,
            "%s: %zu - Directive %s only takes 1 parameter not %zu.",
            node->file,
            node->line,
            node->directive,
            ib_list_elements(node->params));
        rc = IB_EINVAL;
        goto cleanup;
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
        ib_mpool_release(local_mp);
        rc = IB_ENOENT;
        goto cleanup;
    }

    real = ib_mpool_alloc(local_mp, PATH_MAX);
    if (real == NULL) {
        ib_cfg_log_error(
            cp,
            "Failed to allocate path buffer of size %d",
            PATH_MAX);
        rc = IB_EALLOC;
        goto cleanup;
    }
    real = realpath(incfile, real);
    if (real == NULL) {
        if (!if_exists) {
            ib_cfg_log_error(cp,
                             "Failed to find real path of included file "
                             "(using original \"%s\"): %s",
                             incfile, strerror(errno));
        }
        else {
            ib_cfg_log_warning(
                cp,
                "Failed to normalize path. Using raw include path: %s",
                incfile);
        }

        real = incfile;
    }
    else if (strcmp(real, incfile) != 0) {
        ib_cfg_log_info(
            cp,
            "Real path of included file \"%s\" is \"%s\"",
            incfile,
            real);
    }
    else {
        ib_cfg_log_info(
            cp,
            "Including file \"%s\"",
            incfile);
    }

    rc = detect_file_loop(cp, node);
    if (rc != IB_OK) {
        goto cleanup;
    }

    if (access(incfile, R_OK) != 0) {
        if (if_exists) {
            ib_cfg_log(
                cp, (errno == ENOENT) ? IB_LOG_DEBUG : IB_LOG_NOTICE,
                "Ignoring include file \"%s\": %s",
                incfile,
                strerror(errno));
            rc = IB_OK;
            goto cleanup;
        }

        ib_cfg_log_error(
            cp,
            "Cannot access included file \"%s\": %s",
            incfile,
            strerror(errno));
        rc = IB_ENOENT;
        goto cleanup;
    }

    statval = stat(incfile, &statbuf);
    if (statval != 0) {
        if (if_exists) {
            ib_cfg_log(
                cp, (errno == ENOENT) ? IB_LOG_DEBUG : IB_LOG_NOTICE,
                "Ignoring include file \"%s\": %s",
                incfile,
                strerror(errno));
            rc = IB_OK;
            goto cleanup;
        }

        ib_cfg_log_error(
            cp,
            "Failed to stat include file \"%s\": %s",
            incfile,
            strerror(errno));
        rc = IB_ENOENT;
        goto cleanup;
    }

    if (S_ISREG(statbuf.st_mode) == 0) {
        if (if_exists) {
            ib_cfg_log_info(
                cp,
                "Ignoring include file \"%s\": Not a regular file",
                incfile);
            rc = IB_OK;
            goto cleanup;
        }

        ib_cfg_log_error(
            cp,
	    "Included file \"%s\" is not a regular file",
            incfile);
        rc = IB_ENOENT;
        goto cleanup;
    }

    ib_cfg_log_debug(cp, "Including '%s'", incfile);

    /* Make the given node the current node for the file inclusion. */
    current_node = cp->curr;
    cp->curr = node;

    /* Allocate fsm in the heap as it contains a very large buffer. */
    fsm = ib_mpool_alloc(local_mp, sizeof(*fsm));
    if (fsm == NULL) {
        rc = IB_EALLOC;
        goto cleanup;
    }
    *fsm = cp->fsm;
    rc = ib_cfgparser_parse_private(cp, incfile, false);
    cp->fsm = *fsm;
    cp->curr = current_node;
    if (rc != IB_OK) {
        ib_cfg_log_error(
            cp,
            "Error parsing included file \"%s\": %s",
	    incfile,
            ib_status_to_string(rc));
        rc = IB_OK;
        goto cleanup;
    }

    ib_cfg_log_debug(cp, "Done processing include file \"%s\"", incfile);

cleanup:
    if (local_mp != NULL) {
        ib_mpool_release(local_mp);
    }
    return rc;
}

//! Proxy to include_parse_directive_impl with if_exists = true.
static ib_status_t include_if_exists_parse_directive(
    ib_cfgparser_t *cp,
    ib_mpool_t *tmp_mp,
    ib_cfgparser_node_t *node
) {
    return include_parse_directive_impl(cp, tmp_mp, node, true);
}

//! Proxy to include_parse_directive_impl with if_exists = false.
static ib_status_t include_parse_directive(
    ib_cfgparser_t *cp,
    ib_mpool_t *tmp_mp,
    ib_cfgparser_node_t *node
) {
    return include_parse_directive_impl(cp, tmp_mp, node, false);
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
    { "IncludeIfExists", include_if_exists_parse_directive },
    { "Include",         include_parse_directive           },
    { "LogLevel",        loglevel_parse_directive          },
    { NULL, NULL } /* Null termination. Do not remove. */
};


#line 712 "/home/nleroy/devel/ib/engine/config-parser.rl"



#line 475 "/home/nleroy/devel/ib/engine/config-parser.c"
static const char _ironbee_config_actions[] = {
	0, 1, 0, 1, 6, 1, 12, 1, 
	13, 1, 17, 1, 24, 1, 28, 1, 
	31, 1, 39, 1, 42, 1, 46, 1, 
	47, 1, 48, 1, 51, 1, 53, 2, 
	0, 36, 2, 0, 45, 2, 1, 20, 
	2, 1, 21, 2, 2, 27, 2, 2, 
	28, 2, 3, 33, 2, 3, 41, 2, 
	3, 50, 2, 4, 52, 2, 7, 6, 
	2, 8, 34, 2, 8, 35, 2, 9, 
	26, 2, 10, 43, 2, 10, 44, 2, 
	14, 6, 3, 0, 10, 43, 3, 0, 
	10, 44, 3, 1, 5, 11, 3, 3, 
	5, 19, 3, 3, 7, 6, 3, 3, 
	18, 1, 3, 3, 25, 2, 3, 3, 
	32, 8, 3, 3, 33, 8, 3, 3, 
	40, 10, 3, 3, 41, 10, 3, 4, 
	3, 49, 3, 4, 3, 50, 3, 7, 
	6, 3, 3, 10, 0, 44, 3, 14, 
	6, 3, 3, 14, 6, 16, 3, 14, 
	6, 23, 3, 14, 6, 30, 3, 14, 
	6, 38, 3, 14, 7, 6, 4, 1, 
	5, 11, 20, 4, 1, 5, 11, 21, 
	4, 3, 5, 19, 1, 4, 14, 3, 
	6, 16, 4, 14, 3, 6, 23, 4, 
	14, 3, 6, 30, 4, 14, 3, 6, 
	38, 4, 14, 3, 15, 6, 4, 14, 
	3, 22, 6, 4, 14, 3, 29, 6, 
	4, 14, 3, 37, 6, 4, 14, 7, 
	6, 3, 4, 14, 7, 6, 16, 4, 
	14, 7, 6, 23, 4, 14, 7, 6, 
	30, 4, 14, 7, 6, 38, 5, 14, 
	3, 7, 6, 16, 5, 14, 3, 7, 
	6, 30, 5, 14, 3, 7, 6, 38
	
};

static const unsigned char _ironbee_config_key_offsets[] = {
	0, 0, 0, 1, 3, 3, 5, 5, 
	7, 7, 9, 9, 11, 11, 13, 13, 
	15, 24, 31, 38, 39, 46, 55, 62, 
	69, 69, 76, 84, 91, 91, 98, 107, 
	114, 121, 128, 135, 144, 151, 158, 165
};

static const char _ironbee_config_trans_keys[] = {
	47, 10, 13, 34, 92, 10, 13, 34, 
	92, 10, 13, 10, 13, 10, 13, 9, 
	10, 13, 32, 34, 35, 60, 62, 92, 
	32, 34, 60, 62, 92, 9, 10, 9, 
	10, 32, 34, 60, 62, 92, 10, 9, 
	10, 32, 34, 60, 62, 92, 9, 10, 
	13, 32, 34, 35, 60, 62, 92, 32, 
	34, 60, 62, 92, 9, 10, 9, 10, 
	32, 34, 60, 62, 92, 9, 10, 32, 
	34, 60, 62, 92, 9, 10, 32, 34, 
	35, 60, 62, 92, 32, 34, 60, 62, 
	92, 9, 10, 9, 10, 32, 34, 60, 
	62, 92, 9, 10, 13, 32, 60, 62, 
	92, 34, 35, 32, 34, 60, 62, 92, 
	9, 10, 9, 10, 32, 34, 60, 62, 
	92, 32, 34, 60, 62, 92, 9, 10, 
	9, 10, 32, 34, 60, 62, 92, 9, 
	10, 13, 32, 60, 62, 92, 34, 35, 
	32, 34, 60, 62, 92, 9, 10, 9, 
	10, 32, 34, 60, 62, 92, 32, 34, 
	60, 62, 92, 9, 10, 9, 10, 32, 
	34, 60, 62, 92, 0
};

static const char _ironbee_config_single_lengths[] = {
	0, 0, 1, 2, 0, 2, 0, 2, 
	0, 2, 0, 2, 0, 2, 0, 2, 
	9, 5, 7, 1, 7, 9, 5, 7, 
	0, 7, 8, 5, 0, 7, 7, 5, 
	7, 5, 7, 7, 5, 7, 5, 7
};

static const char _ironbee_config_range_lengths[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 1, 0, 0, 0, 0, 1, 0, 
	0, 0, 0, 1, 0, 0, 1, 1, 
	0, 1, 0, 1, 1, 0, 1, 0
};

static const unsigned char _ironbee_config_index_offsets[] = {
	0, 0, 1, 3, 6, 7, 10, 11, 
	14, 15, 18, 19, 22, 23, 26, 27, 
	30, 40, 47, 55, 57, 65, 75, 82, 
	90, 91, 99, 108, 115, 116, 124, 133, 
	140, 148, 155, 163, 172, 179, 187, 194
};

static const char _ironbee_config_indicies[] = {
	1, 3, 2, 4, 5, 1, 7, 9, 
	10, 8, 8, 11, 12, 7, 14, 16, 
	17, 15, 15, 18, 19, 14, 21, 22, 
	23, 21, 25, 26, 27, 25, 29, 30, 
	31, 29, 33, 32, 34, 33, 35, 28, 
	36, 36, 36, 36, 37, 36, 1, 36, 
	38, 36, 36, 36, 36, 37, 1, 39, 
	32, 36, 40, 36, 36, 36, 36, 37, 
	1, 42, 43, 44, 42, 45, 33, 33, 
	33, 46, 41, 47, 47, 47, 47, 48, 
	47, 7, 50, 51, 50, 50, 50, 50, 
	48, 7, 50, 50, 52, 50, 50, 50, 
	50, 48, 7, 54, 33, 54, 55, 33, 
	33, 56, 57, 53, 58, 58, 58, 58, 
	59, 58, 14, 60, 60, 61, 60, 60, 
	60, 60, 59, 14, 63, 64, 65, 63, 
	66, 66, 67, 66, 62, 68, 68, 68, 
	68, 69, 68, 21, 68, 70, 68, 68, 
	68, 68, 69, 21, 71, 71, 71, 71, 
	69, 71, 21, 68, 72, 68, 68, 68, 
	68, 69, 21, 74, 75, 76, 74, 66, 
	77, 78, 66, 73, 80, 80, 80, 80, 
	81, 80, 25, 80, 83, 80, 80, 80, 
	80, 81, 25, 85, 85, 85, 85, 81, 
	85, 25, 80, 86, 80, 80, 80, 80, 
	81, 25, 0
};

static const char _ironbee_config_trans_targs[] = {
	16, 17, 16, 16, 17, 20, 21, 22, 
	5, 24, 6, 22, 25, 26, 27, 9, 
	28, 10, 27, 29, 30, 31, 33, 34, 
	35, 36, 38, 39, 17, 16, 16, 18, 
	19, 0, 2, 3, 16, 1, 16, 16, 
	16, 22, 21, 21, 23, 5, 7, 21, 
	4, 21, 21, 21, 21, 27, 26, 9, 
	26, 11, 26, 8, 26, 26, 31, 30, 
	30, 32, 0, 13, 30, 12, 30, 30, 
	30, 36, 35, 35, 37, 35, 15, 35, 
	35, 14, 35, 35, 35, 35, 35
};

static const short _ironbee_config_trans_actions[] = {
	29, 79, 21, 23, 142, 142, 171, 146, 
	3, 3, 3, 201, 181, 13, 150, 3, 
	3, 3, 206, 186, 31, 154, 211, 191, 
	34, 158, 216, 196, 162, 25, 55, 221, 
	0, 0, 0, 134, 58, 3, 130, 27, 
	126, 226, 9, 94, 246, 61, 98, 40, 
	3, 166, 37, 176, 102, 231, 11, 61, 
	70, 98, 46, 3, 43, 106, 236, 15, 
	49, 252, 1, 98, 67, 3, 114, 64, 
	110, 241, 17, 52, 258, 19, 98, 138, 
	76, 3, 86, 122, 82, 73, 118
};

static const short _ironbee_config_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	5, 0, 0, 0, 0, 5, 0, 0, 
	0, 0, 5, 0, 0, 0, 5, 0, 
	0, 0, 0, 5, 0, 0, 0, 0
};

static const short _ironbee_config_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	7, 0, 0, 0, 0, 7, 0, 0, 
	0, 0, 7, 0, 0, 0, 7, 0, 
	0, 0, 0, 7, 0, 0, 0, 0
};

static const short _ironbee_config_eof_actions[] = {
	0, 0, 0, 0, 0, 90, 90, 90, 
	0, 0, 0, 0, 0, 1, 0, 1, 
	0, 0, 0, 0, 0, 90, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 1, 0, 0, 0, 0
};

static const unsigned char _ironbee_config_eof_trans[] = {
	0, 1, 0, 0, 7, 0, 0, 0, 
	14, 0, 0, 0, 21, 0, 25, 0, 
	0, 37, 37, 40, 37, 0, 7, 50, 
	50, 50, 0, 59, 61, 61, 0, 69, 
	69, 72, 69, 0, 80, 83, 85, 83
};

static const int ironbee_config_start = 16;
static const int ironbee_config_first_final = 16;
static const int ironbee_config_error = 0;

static const int ironbee_config_en_parameters = 21;
static const int ironbee_config_en_block_parameters = 26;
static const int ironbee_config_en_newblock = 30;
static const int ironbee_config_en_endblock = 35;
static const int ironbee_config_en_main = 16;


#line 715 "/home/nleroy/devel/ib/engine/config-parser.rl"

ib_status_t ib_cfgparser_ragel_parse_chunk(
    ib_cfgparser_t *cp,
    const char *buf,
    const size_t blen,
    const int is_last_chunk)
{
    assert(cp != NULL);
    assert(cp->ib != NULL);

    ib_engine_t *ib_engine = cp->ib;

    /* Temporary memory pool. */
    ib_mpool_t *temp_mp = ib_engine_pool_temp_get(ib_engine);

    /* Configuration memory pool. */
    ib_mpool_t *config_mp = ib_engine_pool_config_get(ib_engine);

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
    ib_list_create(&plist, temp_mp);
    if (plist == NULL) {
        ib_cfg_log_error(cp, "Cannot allocate parameter list.");
        return IB_EALLOC;
    }

    /* Access all ragel state variables via structure. */
    
#line 765 "/home/nleroy/devel/ib/engine/config-parser.rl"
    
#line 766 "/home/nleroy/devel/ib/engine/config-parser.rl"
    
#line 767 "/home/nleroy/devel/ib/engine/config-parser.rl"
    
#line 768 "/home/nleroy/devel/ib/engine/config-parser.rl"

    
#line 730 "/home/nleroy/devel/ib/engine/config-parser.c"
	{
	 cp->fsm.cs = ironbee_config_start;
	 cp->fsm.top = 0;
	 cp->fsm.ts = 0;
	 cp->fsm.te = 0;
	 cp->fsm.act = 0;
	}

#line 770 "/home/nleroy/devel/ib/engine/config-parser.rl"
    
#line 741 "/home/nleroy/devel/ib/engine/config-parser.c"
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
#line 762 "/home/nleroy/devel/ib/engine/config-parser.c"
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
				_trans += (unsigned int)(_mid - _keys);
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
				_trans += (unsigned int)((_mid - _keys)>>1);
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
#line 470 "/home/nleroy/devel/ib/engine/config-parser.rl"
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
#line 480 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{
        pval = qstrdup(cp, config_mp);
        if (pval == NULL) {
            return IB_EALLOC;
        }
        ib_list_push(plist, pval);
    }
	break;
	case 2:
#line 487 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{
        pval = qstrdup(cp, config_mp);
        if (pval == NULL) {
            return IB_EALLOC;
        }
        ib_list_push(plist, pval);
    }
	break;
	case 3:
#line 495 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{
        cp->curr->line += 1;
    }
	break;
	case 4:
#line 500 "/home/nleroy/devel/ib/engine/config-parser.rl"
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
#line 508 "/home/nleroy/devel/ib/engine/config-parser.rl"
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
                rc = (parse_directive_table[i].fn)(cp, temp_mp, node);
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
#line 561 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{
        if (cpbuf_append(cp, *( fsm_vars.p)) != IB_OK) {
            return IB_EALLOC;
        }
    }
	break;
	case 7:
#line 567 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{
        cpbuf_clear(cp);
    }
	break;
	case 8:
#line 572 "/home/nleroy/devel/ib/engine/config-parser.rl"
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
#line 580 "/home/nleroy/devel/ib/engine/config-parser.rl"
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
#line 614 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{
        ib_cfgparser_pop_node(cp);
        blkname = NULL;
    }
	break;
	case 11:
#line 653 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{ ( fsm_vars.p)--; { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; goto _again;} }
	break;
	case 14:
#line 1 "NONE"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 15:
#line 645 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{ cp->fsm.act = 2;}
	break;
	case 16:
#line 653 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{ cp->fsm.act = 4;}
	break;
	case 17:
#line 644 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 18:
#line 645 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 19:
#line 647 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; goto _again;} }}
	break;
	case 20:
#line 653 "/home/nleroy/devel/ib/engine/config-parser.rl"
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
#line 658 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{ cp->fsm.act = 6;}
	break;
	case 23:
#line 661 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{ cp->fsm.act = 7;}
	break;
	case 24:
#line 657 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 25:
#line 658 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 26:
#line 663 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; goto _again;} }}
	break;
	case 27:
#line 661 "/home/nleroy/devel/ib/engine/config-parser.rl"
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
#line 669 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{ cp->fsm.act = 10;}
	break;
	case 30:
#line 676 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{ cp->fsm.act = 12;}
	break;
	case 31:
#line 667 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 32:
#line 669 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ ( fsm_vars.p)--; { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; goto _again;} }}
	break;
	case 33:
#line 671 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ ( fsm_vars.p)--; { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; goto _again;} }}
	break;
	case 34:
#line 669 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;{ ( fsm_vars.p)--; { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; goto _again;} }}
	break;
	case 35:
#line 676 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;{ { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 26; goto _again;} }}
	break;
	case 36:
#line 1 "NONE"
	{	switch(  cp->fsm.act ) {
	case 10:
	{{( fsm_vars.p) = (( cp->fsm.te))-1;} ( fsm_vars.p)--; { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; goto _again;} }
	break;
	case 12:
	{{( fsm_vars.p) = (( cp->fsm.te))-1;} { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 26; goto _again;} }
	break;
	}
	}
	break;
	case 37:
#line 682 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{ cp->fsm.act = 14;}
	break;
	case 38:
#line 689 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{ cp->fsm.act = 16;}
	break;
	case 39:
#line 680 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 40:
#line 682 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 41:
#line 684 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 42:
#line 691 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; goto _again;} }}
	break;
	case 43:
#line 682 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;}
	break;
	case 44:
#line 689 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;}
	break;
	case 45:
#line 1 "NONE"
	{	switch(  cp->fsm.act ) {
	default:
	{{( fsm_vars.p) = (( cp->fsm.te))-1;}}
	break;
	}
	}
	break;
	case 46:
#line 704 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ ( fsm_vars.p)--; { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 30; goto _again;}}}
	break;
	case 47:
#line 705 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{        { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 35; goto _again;}}}
	break;
	case 48:
#line 708 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 49:
#line 709 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 50:
#line 710 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 51:
#line 695 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;}
	break;
	case 52:
#line 701 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;{ { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 21; goto _again;} }}
	break;
	case 53:
#line 701 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{{( fsm_vars.p) = (( cp->fsm.te))-1;}{ { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 21; goto _again;} }}
	break;
#line 1187 "/home/nleroy/devel/ib/engine/config-parser.c"
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
#line 1200 "/home/nleroy/devel/ib/engine/config-parser.c"
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
#line 470 "/home/nleroy/devel/ib/engine/config-parser.rl"
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
#line 480 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{
        pval = qstrdup(cp, config_mp);
        if (pval == NULL) {
            return IB_EALLOC;
        }
        ib_list_push(plist, pval);
    }
	break;
	case 5:
#line 508 "/home/nleroy/devel/ib/engine/config-parser.rl"
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
                rc = (parse_directive_table[i].fn)(cp, temp_mp, node);
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
#line 653 "/home/nleroy/devel/ib/engine/config-parser.rl"
	{ ( fsm_vars.p)--; { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; goto _again;} }
	break;
#line 1299 "/home/nleroy/devel/ib/engine/config-parser.c"
		}
	}
	}

	_out: {}
	}

#line 771 "/home/nleroy/devel/ib/engine/config-parser.rl"

    /* Ensure that our block is always empty on last chunk. */
    if ( is_last_chunk && blkname != NULL ) {
        ib_cfg_log_error(cp, "Block name is not empty at end of config input");
        return IB_EINVAL;
    }

    return rc;
}
