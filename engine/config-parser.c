
#line 1 "config-parser.rl"
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

    /* Protect against run-away token aggregation. 1k should be enough? */
    if (cp->buffer->len >= 1024) {
        ib_cfg_log_error(
            cp,
            "Token size limit exceeded. Will not grow past %zu characters.",
            cp->buffer->len);
        ib_cfg_log_trace(
            cp,
            "Current buffer is [[[[%.*s]]]]",
            (int) cp->buffer->len,
            (char *)cp->buffer->data);
        return IB_EALLOC;
    }

    return ib_vector_append(cp->buffer, &c, 1);
}

/**
 * Clear the buffer in @a cp.
 * @param[in] cp Configuration parser.
 */
static void cpbuf_clear(ib_cfgparser_t *cp) {
    assert(cp != NULL);
    assert(cp->buffer != NULL);
    ib_status_t rc;

    rc = ib_vector_truncate(cp->buffer, 0);
    if (rc != IB_OK) {
        ib_cfg_log_error(
            cp,
            "Failed to truncate token buffer: %s",
            ib_status_to_string(rc));
    }
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
static char *qstrdup(ib_cfgparser_t *cp, ib_mpool_t* mp)
{
    const char *start = (const char *)cp->buffer->data;
    const char *end = (const char *)(cp->buffer->data + cp->buffer->len - 1);
    size_t len = cp->buffer->len;

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

    /* Store current fsm. */
    *fsm = cp->fsm;

    /* Initialize new fsm in cp. */
    rc = ib_cfgparser_ragel_init(cp);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Could not initialize new parser.");
        return rc;
    }

    rc = ib_cfgparser_parse_private(cp, incfile, false);
    /* Restore fsm. */
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


#line 754 "config-parser.rl"



#line 496 "config-parser.c"
static const char _ironbee_config_actions[] = {
	0, 1, 0, 1, 3, 1, 6, 1, 
	10, 1, 12, 1, 13, 1, 14, 1, 
	18, 1, 25, 1, 32, 1, 40, 1, 
	43, 1, 47, 1, 48, 1, 49, 1, 
	52, 1, 53, 1, 55, 2, 0, 37, 
	2, 0, 46, 2, 1, 21, 2, 1, 
	22, 2, 2, 28, 2, 2, 29, 2, 
	3, 6, 2, 3, 34, 2, 3, 42, 
	2, 3, 51, 2, 4, 54, 2, 6, 
	3, 2, 7, 35, 2, 7, 36, 2, 
	8, 27, 2, 9, 44, 2, 9, 45, 
	2, 12, 29, 2, 15, 6, 3, 0, 
	9, 44, 3, 0, 9, 45, 3, 1, 
	5, 11, 3, 3, 5, 20, 3, 3, 
	19, 1, 3, 3, 26, 2, 3, 3, 
	33, 7, 3, 3, 41, 9, 3, 4, 
	3, 50, 3, 9, 0, 45, 3, 15, 
	6, 3, 3, 15, 6, 17, 3, 15, 
	6, 24, 3, 15, 6, 31, 3, 15, 
	6, 39, 4, 1, 5, 10, 11, 4, 
	1, 5, 11, 21, 4, 1, 5, 11, 
	22, 4, 15, 3, 6, 17, 4, 15, 
	3, 6, 24, 4, 15, 3, 6, 31, 
	4, 15, 3, 6, 39, 4, 15, 3, 
	16, 6, 4, 15, 3, 23, 6, 4, 
	15, 3, 30, 6, 4, 15, 3, 38, 
	6, 5, 1, 5, 10, 11, 22
};

static const unsigned char _ironbee_config_key_offsets[] = {
	0, 0, 0, 1, 2, 4, 4, 5, 
	7, 7, 9, 9, 11, 11, 13, 13, 
	14, 16, 16, 17, 19, 28, 36, 38, 
	46, 55, 63, 63, 71, 80, 88, 88, 
	96, 105, 113, 121, 129, 138, 146, 154
};

static const char _ironbee_config_trans_keys[] = {
	10, 47, 10, 13, 10, 34, 92, 10, 
	13, 34, 92, 10, 13, 10, 10, 13, 
	10, 10, 13, 9, 10, 13, 32, 34, 
	35, 60, 62, 92, 13, 32, 34, 60, 
	62, 92, 9, 10, 10, 13, 9, 10, 
	13, 32, 34, 60, 62, 92, 9, 10, 
	13, 32, 34, 35, 60, 62, 92, 13, 
	32, 34, 60, 62, 92, 9, 10, 9, 
	10, 13, 32, 34, 60, 62, 92, 9, 
	10, 13, 32, 34, 35, 60, 62, 92, 
	13, 32, 34, 60, 62, 92, 9, 10, 
	9, 10, 13, 32, 34, 60, 62, 92, 
	9, 10, 13, 32, 60, 62, 92, 34, 
	35, 13, 32, 34, 60, 62, 92, 9, 
	10, 13, 32, 34, 60, 62, 92, 9, 
	10, 9, 10, 13, 32, 34, 60, 62, 
	92, 9, 10, 13, 32, 60, 62, 92, 
	34, 35, 13, 32, 34, 60, 62, 92, 
	9, 10, 13, 32, 34, 60, 62, 92, 
	9, 10, 9, 10, 13, 32, 34, 60, 
	62, 92, 0
};

static const char _ironbee_config_single_lengths[] = {
	0, 0, 1, 1, 2, 0, 1, 2, 
	0, 2, 0, 2, 0, 2, 0, 1, 
	2, 0, 1, 2, 9, 6, 2, 8, 
	9, 6, 0, 8, 9, 6, 0, 8, 
	7, 6, 6, 8, 7, 6, 6, 8
};

static const char _ironbee_config_range_lengths[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 1, 0, 0, 
	0, 1, 0, 0, 0, 1, 0, 0, 
	1, 1, 1, 0, 1, 1, 1, 0
};

static const unsigned char _ironbee_config_index_offsets[] = {
	0, 0, 1, 3, 5, 8, 9, 11, 
	14, 15, 18, 19, 22, 23, 26, 27, 
	29, 32, 33, 35, 38, 48, 56, 59, 
	68, 78, 86, 87, 96, 106, 114, 115, 
	124, 133, 141, 149, 158, 167, 175, 183
};

static const char _ironbee_config_indicies[] = {
	1, 2, 3, 5, 4, 6, 7, 1, 
	9, 10, 3, 12, 13, 11, 11, 14, 
	15, 9, 17, 19, 20, 18, 18, 21, 
	22, 17, 24, 26, 25, 27, 28, 24, 
	30, 31, 3, 32, 33, 30, 34, 2, 
	35, 34, 3, 36, 37, 38, 39, 1, 
	40, 40, 40, 40, 40, 41, 40, 1, 
	42, 42, 36, 40, 43, 40, 40, 40, 
	40, 40, 41, 1, 44, 10, 45, 44, 
	11, 46, 46, 46, 47, 9, 49, 49, 
	49, 49, 49, 50, 49, 9, 52, 52, 
	53, 52, 52, 52, 52, 52, 50, 9, 
	54, 55, 55, 54, 18, 55, 55, 56, 
	57, 17, 58, 58, 58, 58, 58, 59, 
	58, 17, 60, 60, 61, 60, 60, 60, 
	60, 60, 59, 17, 62, 26, 63, 62, 
	25, 25, 64, 25, 24, 65, 65, 65, 
	65, 65, 66, 65, 24, 67, 67, 67, 
	67, 67, 66, 67, 24, 65, 68, 65, 
	65, 65, 65, 65, 66, 24, 69, 31, 
	70, 69, 25, 71, 72, 25, 30, 74, 
	74, 74, 74, 74, 75, 74, 30, 77, 
	77, 77, 77, 77, 75, 77, 30, 74, 
	79, 74, 74, 74, 74, 74, 75, 30, 
	0
};

static const char _ironbee_config_trans_targs[] = {
	20, 21, 20, 0, 20, 20, 21, 23, 
	24, 25, 24, 7, 26, 8, 25, 27, 
	28, 29, 11, 30, 12, 29, 31, 32, 
	33, 0, 32, 34, 35, 36, 37, 36, 
	38, 39, 20, 2, 22, 3, 20, 4, 
	20, 1, 20, 20, 24, 6, 0, 9, 
	24, 24, 5, 24, 24, 24, 28, 0, 
	28, 13, 28, 10, 28, 28, 32, 15, 
	16, 32, 14, 32, 32, 36, 18, 36, 
	19, 36, 36, 17, 36, 36, 36, 36
};

static const unsigned char _ironbee_config_trans_actions[] = {
	35, 91, 64, 0, 25, 27, 134, 134, 
	209, 138, 106, 5, 5, 5, 189, 169, 
	88, 142, 5, 5, 5, 194, 174, 37, 
	146, 1, 58, 199, 179, 40, 150, 61, 
	204, 184, 29, 3, 0, 0, 31, 70, 
	67, 5, 33, 126, 15, 3, 7, 55, 
	164, 46, 5, 159, 43, 110, 17, 9, 
	79, 55, 52, 5, 49, 114, 19, 3, 
	55, 76, 5, 73, 118, 21, 3, 23, 
	55, 130, 85, 5, 94, 82, 98, 122
};

static const unsigned char _ironbee_config_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 11, 0, 0, 0, 
	11, 0, 0, 0, 11, 0, 0, 0, 
	11, 0, 0, 0, 11, 0, 0, 0
};

static const unsigned char _ironbee_config_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 13, 0, 0, 0, 
	13, 0, 0, 0, 13, 0, 0, 0, 
	13, 0, 0, 0, 13, 0, 0, 0
};

static const unsigned char _ironbee_config_eof_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 154, 
	154, 154, 0, 9, 9, 9, 0, 1, 
	1, 0, 1, 1, 0, 0, 0, 0, 
	102, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 1, 0, 0, 0
};

static const unsigned char _ironbee_config_eof_trans[] = {
	0, 1, 0, 0, 0, 9, 0, 0, 
	0, 0, 17, 0, 0, 0, 24, 0, 
	0, 30, 0, 0, 0, 41, 43, 41, 
	0, 49, 52, 52, 0, 59, 61, 61, 
	0, 66, 68, 66, 0, 74, 77, 79
};

static const int ironbee_config_start = 20;
static const int ironbee_config_first_final = 20;
static const int ironbee_config_error = 0;

static const int ironbee_config_en_parameters = 24;
static const int ironbee_config_en_block_parameters = 28;
static const int ironbee_config_en_newblock = 32;
static const int ironbee_config_en_endblock = 36;
static const int ironbee_config_en_main = 20;


#line 757 "config-parser.rl"

ib_status_t ib_cfgparser_ragel_init(ib_cfgparser_t *cp) {
    assert(cp != NULL);
    assert(cp->ib != NULL);
    assert(cp->mp != NULL);

    ib_status_t rc;

    ib_cfg_log_debug(cp, "Initializing Ragel state machine.");

    /* Access all ragel state variables via structure. */
    
#line 769 "config-parser.rl"

    
#line 696 "config-parser.c"
	{
	 cp->fsm.cs = ironbee_config_start;
	 cp->fsm.top = 0;
	 cp->fsm.ts = 0;
	 cp->fsm.te = 0;
	 cp->fsm.act = 0;
	}

#line 771 "config-parser.rl"

    ib_cfg_log_info(cp, "Initializing IronBee parse values.");

    rc = ib_list_create(&(cp->fsm.plist), cp->mp);
    if (rc != IB_OK) {
        return rc;
    }

    cp->fsm.directive = NULL;
    cp->fsm.blkname = NULL;

    rc = ib_vector_create(&(cp->fsm.ts_buffer), cp->mp, 0);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

/**
 * If Ragel has a partial match we must preserve the buffer
 * until next time to continue the match.
 * This effects how the machine is restarted on
 * the next call to ib_cfgparser_ragel_parse_chunk.
 */
static ib_status_t cfgparser_partial_match_maintenance(
    ib_cfgparser_t *cp,
    const char *buf,
    const size_t blen
)
{
    ib_status_t rc;
    size_t buffer_remaining;
    
    if (buf + blen > cp->fsm.ts) {
        buffer_remaining = buf + blen - cp->fsm.ts;
    }
    else {
        buffer_remaining = cp->fsm.ts - buf + blen;
    }

    /* Distance that the ts and te pointers will be shifted when
     * copied into the vector. */
    ssize_t delta;

    rc = ib_vector_truncate(cp->fsm.ts_buffer, 0);
    if (rc != IB_OK) {
        return rc;
    }


    rc = ib_vector_append(cp->fsm.ts_buffer, cp->fsm.ts, buffer_remaining);
    if (rc != IB_OK) {
        return rc;
    }

    delta = (char *)cp->fsm.ts_buffer->data - cp->fsm.ts;

    /* Relocate ts and te into the buffer. */
    cp->fsm.te += delta;
    cp->fsm.ts += delta;

    return IB_OK;
}

/**
 * If Ragel has a partial match we must resume parsing in 
 * a special buffer we are maintaining.
 * @param[in] cp Configuration parser. The cp->fsm structure is updated.
 * @param[in] buf Buffer to append to cp->fsm.ts_buffer.
 * @param[in] blen Length of the buffer to append to cp->fsm.ts_buffer.
 * @param[out] fsm_vars The p field is updated to point to 
 *             the new location of buffer.
 */
static ib_status_t cfgparser_partial_match_resume(
    ib_cfgparser_t *cp,
    const char *buf,
    const size_t blen
)
{
    ib_status_t rc;

    /* Store the previous data pointer so we can detect if it moves. */
    void *orig_data = cp->fsm.ts_buffer->data;

    /* Append the input buffer. */
    rc = ib_vector_append(cp->fsm.ts_buffer, (uint8_t *)buf, blen);
    if (rc != IB_OK) {
        return rc;
    }

    if (orig_data != cp->fsm.ts_buffer->data) {
        /* Update ts and te */
        ssize_t delta = cp->fsm.ts_buffer->data - orig_data;

        cp->fsm.ts   += delta;
        cp->fsm.te   += delta;
    }

    return IB_OK;
}

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

    /* Create a finite state machine type. */
    fsm_vars_t fsm_vars = { buf, NULL, 0 };

    /* A temporary string. This is used to fetch values, check them
     * for NULL, and then pass them on to an owning collection (list). */
    char *tmp_str = NULL;

    /* Point the machine at the current buffer to parse.
     */
    if (cp->fsm.ts != NULL) {
        rc = cfgparser_partial_match_resume(cp, buf, blen);
        if (rc != IB_OK) {
            return rc;
        }

        /* Buffer start = prev buffer end. */
        fsm_vars.p = cp->fsm.te;
    }
    else {
        fsm_vars.p = buf;
    }
    fsm_vars.pe  = fsm_vars.p + blen;
    fsm_vars.eof = (is_last_chunk ? fsm_vars.pe : NULL);

    /* Access all ragel state variables via structure. */
    
#line 919 "config-parser.rl"
    
#line 920 "config-parser.rl"
    
#line 921 "config-parser.rl"
    
#line 922 "config-parser.rl"

    
#line 863 "config-parser.c"
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
	case 14:
#line 1 "NONE"
	{ cp->fsm.ts = ( fsm_vars.p);}
	break;
#line 884 "config-parser.c"
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
#line 501 "config-parser.rl"
	{
        rc = IB_EOTHER;
        ib_cfg_log_error(
            cp,
            "parser error near %s:%zu.",
            cp->curr->file,
            cp->curr->line);
        ib_cfg_log_debug(cp, "Current buffer is [[[[%.*s]]]]", (int)blen, buf);
    }
	break;
	case 1:
#line 512 "config-parser.rl"
	{
        tmp_str = qstrdup(cp, config_mp);
        if (tmp_str == NULL) {
            return IB_EALLOC;
        }
        ib_list_push(cp->fsm.plist, tmp_str);
        tmp_str = NULL;
        cpbuf_clear(cp);
    }
	break;
	case 2:
#line 521 "config-parser.rl"
	{
        tmp_str = qstrdup(cp, config_mp);
        if (tmp_str == NULL) {
            return IB_EALLOC;
        }
        ib_list_push(cp->fsm.plist, tmp_str);
        tmp_str = NULL;
        cpbuf_clear(cp);
    }
	break;
	case 3:
#line 531 "config-parser.rl"
	{
        cp->curr->line += 1;
    }
	break;
	case 4:
#line 536 "config-parser.rl"
	{
        if (cp->buffer->len == 0) {
            ib_cfg_log_error(cp, "Directive name is 0 length.");
            return IB_EOTHER;
        }
        cp->fsm.directive =
            ib_mpool_memdup_to_str(cp->mp, cp->buffer->data, cp->buffer->len);
        if (cp->fsm.directive == NULL) {
            return IB_EALLOC;
        }
        ib_list_clear(cp->fsm.plist);
        cpbuf_clear(cp);
    }
	break;
	case 5:
#line 549 "config-parser.rl"
	{
        ib_cfgparser_node_t *node = NULL;
        ib_cfgparser_node_create(&node, cp);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp, "Cannot create node.");
            return rc;
        }
        node->directive = cp->fsm.directive;
        cp->fsm.directive = NULL;
        node->file = ib_mpool_strdup(cp->mp, cp->curr->file);
        if (node->file == NULL) {
            return IB_EALLOC;
        }
        node->parent = cp->curr;
        node->line = cp->curr->line;
        node->type = IB_CFGPARSER_NODE_DIRECTIVE;
        ib_list_node_t *lst_node;
        IB_LIST_LOOP(cp->fsm.plist, lst_node) {
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
#line 602 "config-parser.rl"
	{
        if (cpbuf_append(cp, *( fsm_vars.p)) != IB_OK) {
            return IB_EALLOC;
        }
    }
	break;
	case 7:
#line 609 "config-parser.rl"
	{
        if (cp->buffer->len == 0) {
            ib_cfg_log_error(cp, "Block name is 0 length.");
            return IB_EOTHER;
        }
        cp->fsm.blkname =
            ib_mpool_memdup_to_str(cp->mp, cp->buffer->data, cp->buffer->len);
        if (cp->fsm.blkname == NULL) {
            return IB_EALLOC;
        }
        ib_list_clear(cp->fsm.plist);
        cpbuf_clear(cp);
    }
	break;
	case 8:
#line 622 "config-parser.rl"
	{
        ib_cfgparser_node_t *node = NULL;
        rc = ib_cfgparser_node_create(&node, cp);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp, "Cannot create node.");
            return rc;
        }
        node->directive = cp->fsm.blkname;
        /* NOTE: We do not clear blkname now. */
        node->file = ib_mpool_strdup(cp->mp, cp->curr->file);
        if (node->file == NULL) {
            return IB_EALLOC;
        }
        node->line = cp->curr->line;
        node->type = IB_CFGPARSER_NODE_BLOCK;
        ib_list_node_t *lst_node;
        IB_LIST_LOOP(cp->fsm.plist, lst_node) {
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
	case 9:
#line 657 "config-parser.rl"
	{
        ib_cfgparser_pop_node(cp);
        cpbuf_clear(cp);
        cp->fsm.blkname = NULL;
    }
	break;
	case 10:
#line 690 "config-parser.rl"
	{ ( fsm_vars.p)--; { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; {
    }goto _again;} }
	break;
	case 11:
#line 691 "config-parser.rl"
	{ ( fsm_vars.p)--; { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; {
    }goto _again;} }
	break;
	case 12:
#line 701 "config-parser.rl"
	{ ( fsm_vars.p)--; { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; {
    }goto _again;} }
	break;
	case 15:
#line 1 "NONE"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 16:
#line 683 "config-parser.rl"
	{ cp->fsm.act = 2;}
	break;
	case 17:
#line 691 "config-parser.rl"
	{ cp->fsm.act = 4;}
	break;
	case 18:
#line 682 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 19:
#line 683 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 20:
#line 685 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; {
    }goto _again;} }}
	break;
	case 21:
#line 691 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;}
	break;
	case 22:
#line 1 "NONE"
	{	switch(  cp->fsm.act ) {
	default:
	{{( fsm_vars.p) = (( cp->fsm.te))-1;}}
	break;
	}
	}
	break;
	case 23:
#line 696 "config-parser.rl"
	{ cp->fsm.act = 6;}
	break;
	case 24:
#line 701 "config-parser.rl"
	{ cp->fsm.act = 8;}
	break;
	case 25:
#line 695 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 26:
#line 696 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 27:
#line 698 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; {
    }goto _again;} }}
	break;
	case 28:
#line 701 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;}
	break;
	case 29:
#line 1 "NONE"
	{	switch(  cp->fsm.act ) {
	default:
	{{( fsm_vars.p) = (( cp->fsm.te))-1;}}
	break;
	}
	}
	break;
	case 30:
#line 707 "config-parser.rl"
	{ cp->fsm.act = 10;}
	break;
	case 31:
#line 713 "config-parser.rl"
	{ cp->fsm.act = 12;}
	break;
	case 32:
#line 705 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 33:
#line 707 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ ( fsm_vars.p)--; { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; {
    }goto _again;} }}
	break;
	case 34:
#line 709 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ ( fsm_vars.p)--; { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; {
    }goto _again;} }}
	break;
	case 35:
#line 707 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;{ ( fsm_vars.p)--; { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; {
    }goto _again;} }}
	break;
	case 36:
#line 713 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;{ {
        if (cp->fsm.top >= 1023) {
            ib_cfg_log_debug(cp, "Recursion too deep during parse.");
            return IB_EOTHER;
        }
    { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 28; goto _again;}} }}
	break;
	case 37:
#line 1 "NONE"
	{	switch(  cp->fsm.act ) {
	case 10:
	{{( fsm_vars.p) = (( cp->fsm.te))-1;} ( fsm_vars.p)--; { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; {
    }goto _again;} }
	break;
	case 12:
	{{( fsm_vars.p) = (( cp->fsm.te))-1;} {
        if (cp->fsm.top >= 1023) {
            ib_cfg_log_debug(cp, "Recursion too deep during parse.");
            return IB_EOTHER;
        }
    { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 28; goto _again;}} }
	break;
	}
	}
	break;
	case 38:
#line 719 "config-parser.rl"
	{ cp->fsm.act = 14;}
	break;
	case 39:
#line 725 "config-parser.rl"
	{ cp->fsm.act = 16;}
	break;
	case 40:
#line 717 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 41:
#line 719 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 42:
#line 721 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 43:
#line 727 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; {
    }goto _again;} }}
	break;
	case 44:
#line 719 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;}
	break;
	case 45:
#line 725 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;}
	break;
	case 46:
#line 1 "NONE"
	{	switch(  cp->fsm.act ) {
	default:
	{{( fsm_vars.p) = (( cp->fsm.te))-1;}}
	break;
	}
	}
	break;
	case 47:
#line 739 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ ( fsm_vars.p)--; {
        if (cp->fsm.top >= 1023) {
            ib_cfg_log_debug(cp, "Recursion too deep during parse.");
            return IB_EOTHER;
        }
    { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 32; goto _again;}}}}
	break;
	case 48:
#line 740 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{        {
        if (cp->fsm.top >= 1023) {
            ib_cfg_log_debug(cp, "Recursion too deep during parse.");
            return IB_EOTHER;
        }
    { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 36; goto _again;}}}}
	break;
	case 49:
#line 743 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 50:
#line 744 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 51:
#line 745 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 52:
#line 746 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{
                ib_cfg_log_error(
                    cp,
                    "Character \">\" encountered outside a block tag.");
                rc = IB_EOTHER;
                {( fsm_vars.p)++; goto _out; }
            }}
	break;
	case 53:
#line 731 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;}
	break;
	case 54:
#line 736 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;{ {
        if (cp->fsm.top >= 1023) {
            ib_cfg_log_debug(cp, "Recursion too deep during parse.");
            return IB_EOTHER;
        }
    { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 24; goto _again;}} }}
	break;
	case 55:
#line 736 "config-parser.rl"
	{{( fsm_vars.p) = (( cp->fsm.te))-1;}{ {
        if (cp->fsm.top >= 1023) {
            ib_cfg_log_debug(cp, "Recursion too deep during parse.");
            return IB_EOTHER;
        }
    { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 24; goto _again;}} }}
	break;
#line 1378 "config-parser.c"
		}
	}

_again:
	_acts = _ironbee_config_actions + _ironbee_config_to_state_actions[ cp->fsm.cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 13:
#line 1 "NONE"
	{ cp->fsm.ts = 0;}
	break;
#line 1391 "config-parser.c"
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
#line 501 "config-parser.rl"
	{
        rc = IB_EOTHER;
        ib_cfg_log_error(
            cp,
            "parser error near %s:%zu.",
            cp->curr->file,
            cp->curr->line);
        ib_cfg_log_debug(cp, "Current buffer is [[[[%.*s]]]]", (int)blen, buf);
    }
	break;
	case 1:
#line 512 "config-parser.rl"
	{
        tmp_str = qstrdup(cp, config_mp);
        if (tmp_str == NULL) {
            return IB_EALLOC;
        }
        ib_list_push(cp->fsm.plist, tmp_str);
        tmp_str = NULL;
        cpbuf_clear(cp);
    }
	break;
	case 5:
#line 549 "config-parser.rl"
	{
        ib_cfgparser_node_t *node = NULL;
        ib_cfgparser_node_create(&node, cp);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp, "Cannot create node.");
            return rc;
        }
        node->directive = cp->fsm.directive;
        cp->fsm.directive = NULL;
        node->file = ib_mpool_strdup(cp->mp, cp->curr->file);
        if (node->file == NULL) {
            return IB_EALLOC;
        }
        node->parent = cp->curr;
        node->line = cp->curr->line;
        node->type = IB_CFGPARSER_NODE_DIRECTIVE;
        ib_list_node_t *lst_node;
        IB_LIST_LOOP(cp->fsm.plist, lst_node) {
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
	case 10:
#line 690 "config-parser.rl"
	{ ( fsm_vars.p)--; { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; {
    }goto _again;} }
	break;
	case 11:
#line 691 "config-parser.rl"
	{ ( fsm_vars.p)--; { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; {
    }goto _again;} }
	break;
	case 12:
#line 701 "config-parser.rl"
	{ ( fsm_vars.p)--; { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; {
    }goto _again;} }
	break;
#line 1504 "config-parser.c"
		}
	}
	}

	_out: {}
	}

#line 924 "config-parser.rl"

    assert(tmp_str == NULL && "tmp_str must be cleared after every use");

    /* Buffer maintenance code. */
    if (cp->fsm.ts != NULL) {
        rc = cfgparser_partial_match_maintenance(cp, buf, blen);
        if (rc != IB_OK) {
            return rc;
        }
    }

    /* On the last chunk, sanity check things. */
    if (is_last_chunk) {
        if (cp->fsm.blkname != NULL) {
            ib_cfg_log_error(
                cp,
                "Unpushed block \"%s\" at end of config input",
                cp->fsm.blkname);
            return IB_EINVAL;
        }
        if (cp->fsm.directive != NULL) {
            ib_cfg_log_error(
                cp,
                "Unpushed directive \"%s\" end of config input",
                cp->fsm.directive);
            return IB_EINVAL;
        }
    }

    return rc;
}
