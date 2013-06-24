
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
    ib_mpool_t* temp_mp,
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
 * Log the real path of the include file.
 *
 * Log the real path of the file we are going to be including.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC Allocation error.
 */
static ib_status_t include_parse_directive_impl_log_realpath(
    ib_cfgparser_t *cp,
    ib_mpool_t *local_mp,
    const char *incfile,
    bool if_exists
)
{
    char *real;

    real = ib_mpool_alloc(local_mp, PATH_MAX);
    if (real == NULL) {
        ib_cfg_log_error(
            cp,
            "Failed to allocate path buffer of size %d",
            PATH_MAX);
        return IB_EALLOC;
    }

    /* Convert to the real path. Failure is OK. We'll use the original.
     * NOTE: We do not error report here. Error reporting is handled 
     *       when we try to access the file. */
    real = realpath(incfile, real);
    if (real == NULL) {
        ib_cfg_log_info(cp, "Including file \"%s\"", incfile);
    }
    else if (strcmp(real, incfile) != 0) {
        ib_cfg_log_info(
            cp,
            "Including file \"%s\" using real path \"%s\"",
            incfile,
            real);
    }
    else {
        ib_cfg_log_info(cp, "Including file \"%s\"", incfile);
    }

    return IB_OK;
}

/**
 * Check the file for inclusion and report useful error messages on failure.
 * @param[in] cp Configuration parser.
 * @param[in] incfile File to include.
 * @param[in] if_exists Choose the error message and log level by 
 *            whether the file must exist or may be missing.
 * @returns
 * - IB_OK on success.
 * - IB_ENOENT on failure.
 */
static ib_status_t include_parse_directive_impl_chk_access(
    ib_cfgparser_t *cp,
    const char *incfile,
    bool if_exists
) {
    struct stat statbuf;
    int statval;

    /* Check if we can access the file. */
    if (access(incfile, R_OK) != 0) {
        if (if_exists) {
            ib_cfg_log(
                cp,
                (errno == ENOENT) ? IB_LOG_DEBUG : IB_LOG_NOTICE,
                "Ignoring include file \"%s\": %s",
                incfile,
                strerror(errno));
        }
        else {
            ib_cfg_log_error(
                cp,
                "Cannot access included file \"%s\": %s",
                incfile,
                strerror(errno));
        }

        return IB_ENOENT;
    }

    /* Stat the file to see if we can read it. */
    statval = stat(incfile, &statbuf);
    if (statval != 0) {
        if (if_exists) {
            ib_cfg_log(
                cp, (errno == ENOENT) ? IB_LOG_DEBUG : IB_LOG_NOTICE,
                "Ignoring include file \"%s\": %s",
                incfile,
                strerror(errno));
        }
        else {
            ib_cfg_log_error(
                cp,
                "Failed to stat include file \"%s\": %s",
                incfile,
                strerror(errno));
        }

        return IB_ENOENT;
    }

    /* Check if this is a regular file. */
    if (S_ISREG(statbuf.st_mode) == 0) {
        if (if_exists) {
            ib_cfg_log_info(
                cp,
                "Ignoring include file \"%s\": Not a regular file",
                incfile);
        }
        else {
            ib_cfg_log_error(
                cp,
	        "Included file \"%s\" is not a regular file",
                incfile);
        }

        return IB_ENOENT;
    }

    return IB_OK;
}

static ib_status_t include_parse_directive_impl_parse(
    ib_cfgparser_t *cp,
    ib_cfgparser_node_t *node,
    ib_mpool_t *local_mp,
    const char *incfile
)
{
    /* A temporary local value to store the parser state in.
     * We allocate this from local_mp to avoid putting the very large
     * buffer variable in fsm on the stack. */
    ib_cfgparser_fsm_t *fsm;
    ib_cfgparser_node_t *current_node;
    ib_status_t rc;

    /* Make the given node the current node for the file inclusion. */
    current_node = cp->curr;
    cp->curr = node;

    /* Allocate fsm in the heap as it contains a very large buffer. */
    fsm = ib_mpool_alloc(local_mp, sizeof(*fsm));
    if (fsm == NULL) {
        return IB_EALLOC;
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

    return rc;
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
    ib_mpool_t *temp_mp,
    ib_cfgparser_node_t *node,
    bool if_exists
) {
    assert(cp != NULL);
    assert(cp->mp != NULL);
    assert(node != NULL);
    assert(node->directive != NULL);
    assert(node->params != NULL);
    assert(node->file != NULL);

    ib_status_t rc;
    char *incfile;
    const char* pval;
    const ib_list_node_t *list_node;

    ib_mpool_t *local_mp = NULL;

    rc = ib_mpool_create(&local_mp, "local_mp", temp_mp);
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

    /* If parsing file "path/ironbee.conf" and we include "other.conf",
     * this will generate the incfile value "path/other.conf". */
    incfile = ib_util_relative_file(local_mp, node->file, pval);
    if (incfile == NULL) {
        ib_cfg_log_error(cp, "Failed to resolve included file \"%s\": %s",
                         node->file, strerror(errno));
        ib_mpool_release(local_mp);
        rc = IB_ENOENT;
        goto cleanup;
    }

    /* Log the realpath of incfile to aid in configuring IB accurately. */
    rc = include_parse_directive_impl_log_realpath(
        cp,
        local_mp,
        incfile,
        if_exists);
    if (rc != IB_OK) {
        goto cleanup;
    }

    /* Prevent include loops. */
    rc = detect_file_loop(cp, node);
    if (rc != IB_OK) {
        goto cleanup;
    }

    /* Check (and log) access problems in a helpful way for the user. */
    rc = include_parse_directive_impl_chk_access(cp, incfile, if_exists);
    if (rc != IB_OK) {
        goto cleanup;
    }

    /* Parse the include file. */
    rc = include_parse_directive_impl_parse(cp, node, local_mp, incfile);
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

    /* IncludeIfExists never causes failure. */
    return (if_exists)? IB_OK : rc;
}

//! Proxy to include_parse_directive_impl with if_exists = true.
static ib_status_t include_if_exists_parse_directive(
    ib_cfgparser_t *cp,
    ib_mpool_t *temp_mp,
    ib_cfgparser_node_t *node
) {
    return include_parse_directive_impl(cp, temp_mp, node, true);
}

//! Proxy to include_parse_directive_impl with if_exists = false.
static ib_status_t include_parse_directive(
    ib_cfgparser_t *cp,
    ib_mpool_t *temp_mp,
    ib_cfgparser_node_t *node
) {
    return include_parse_directive_impl(cp, temp_mp, node, false);
}

static ib_status_t loglevel_parse_directive(
    ib_cfgparser_t *cp,
    ib_mpool_t* temp_mp,
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


#line 815 "config-parser.rl"



#line 553 "config-parser.c"
static const char _ironbee_config_actions[] = {
	0, 1, 0, 1, 3, 1, 6, 1, 
	10, 1, 12, 1, 13, 1, 15, 1, 
	18, 1, 22, 1, 23, 1, 25, 1, 
	29, 1, 30, 1, 31, 1, 35, 1, 
	36, 1, 39, 1, 41, 1, 42, 1, 
	43, 1, 44, 1, 47, 1, 48, 1, 
	50, 2, 0, 35, 2, 0, 41, 2, 
	1, 21, 2, 2, 28, 2, 3, 6, 
	2, 3, 19, 2, 3, 26, 2, 3, 
	32, 2, 3, 33, 2, 3, 37, 2, 
	3, 38, 2, 3, 45, 2, 3, 46, 
	2, 4, 49, 2, 6, 3, 2, 7, 
	34, 2, 8, 27, 2, 9, 40, 2, 
	12, 29, 2, 12, 30, 2, 13, 14, 
	2, 16, 6, 3, 0, 9, 40, 3, 
	1, 5, 11, 3, 3, 5, 20, 3, 
	3, 19, 1, 3, 3, 26, 2, 3, 
	3, 32, 7, 3, 3, 37, 9, 3, 
	4, 3, 45, 3, 9, 0, 40, 3, 
	16, 3, 6, 3, 16, 6, 3, 3, 
	16, 6, 17, 3, 16, 6, 24, 4, 
	1, 5, 10, 11, 4, 1, 5, 11, 
	21, 5, 1, 5, 10, 11, 22, 5, 
	1, 5, 10, 11, 23
};

static const unsigned char _ironbee_config_key_offsets[] = {
	0, 0, 2, 3, 4, 6, 8, 9, 
	11, 13, 15, 19, 21, 23, 25, 27, 
	29, 33, 35, 37, 38, 40, 42, 43, 
	45, 54, 62, 64, 67, 69, 77, 86, 
	94, 96, 99, 99, 101, 103, 111, 120, 
	128, 130, 133, 133, 135, 137, 145, 154, 
	162, 164, 167, 175, 184, 192, 194, 197
};

static const char _ironbee_config_trans_keys[] = {
	10, 13, 10, 47, 10, 13, 10, 13, 
	10, 34, 92, 10, 13, 34, 92, 10, 
	13, 34, 92, 10, 13, 10, 13, 34, 
	92, 10, 13, 34, 92, 10, 13, 34, 
	92, 10, 13, 10, 13, 10, 10, 13, 
	10, 13, 10, 10, 13, 9, 10, 13, 
	32, 34, 35, 60, 62, 92, 13, 32, 
	34, 60, 62, 92, 9, 10, 10, 92, 
	10, 13, 92, 10, 13, 9, 10, 13, 
	32, 34, 60, 62, 92, 9, 10, 13, 
	32, 34, 35, 60, 62, 92, 13, 32, 
	34, 60, 62, 92, 9, 10, 10, 92, 
	10, 13, 92, 34, 92, 34, 92, 9, 
	10, 13, 32, 34, 60, 62, 92, 9, 
	10, 13, 32, 34, 35, 60, 62, 92, 
	13, 32, 34, 60, 62, 92, 9, 10, 
	10, 92, 10, 13, 92, 34, 92, 34, 
	92, 9, 10, 13, 32, 34, 60, 62, 
	92, 9, 10, 13, 32, 60, 62, 92, 
	34, 35, 13, 32, 34, 60, 62, 92, 
	9, 10, 10, 92, 10, 13, 92, 9, 
	10, 13, 32, 34, 60, 62, 92, 9, 
	10, 13, 32, 60, 62, 92, 34, 35, 
	13, 32, 34, 60, 62, 92, 9, 10, 
	10, 92, 10, 13, 92, 9, 10, 13, 
	32, 34, 60, 62, 92, 0
};

static const char _ironbee_config_single_lengths[] = {
	0, 2, 1, 1, 2, 2, 1, 2, 
	2, 2, 4, 2, 2, 2, 2, 2, 
	4, 2, 2, 1, 2, 2, 1, 2, 
	9, 6, 2, 3, 2, 8, 9, 6, 
	2, 3, 0, 2, 2, 8, 9, 6, 
	2, 3, 0, 2, 2, 8, 7, 6, 
	2, 3, 8, 7, 6, 2, 3, 8
};

static const char _ironbee_config_range_lengths[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 1, 0, 0, 0, 0, 0, 1, 
	0, 0, 0, 0, 0, 0, 0, 1, 
	0, 0, 0, 0, 0, 0, 1, 1, 
	0, 0, 0, 1, 1, 0, 0, 0
};

static const short _ironbee_config_index_offsets[] = {
	0, 0, 3, 5, 7, 10, 13, 15, 
	18, 21, 24, 29, 32, 35, 38, 41, 
	44, 49, 52, 55, 57, 60, 63, 65, 
	68, 78, 86, 89, 93, 96, 105, 115, 
	123, 126, 130, 131, 134, 137, 146, 156, 
	164, 167, 171, 172, 175, 178, 187, 196, 
	204, 207, 211, 220, 229, 237, 240, 244
};

static const char _ironbee_config_indicies[] = {
	0, 2, 1, 3, 4, 6, 5, 7, 
	8, 1, 11, 12, 10, 13, 4, 16, 
	17, 15, 19, 15, 18, 20, 21, 15, 
	15, 15, 22, 21, 18, 23, 24, 10, 
	27, 28, 26, 31, 32, 30, 34, 30, 
	33, 35, 36, 30, 30, 30, 37, 36, 
	33, 38, 39, 26, 42, 43, 41, 45, 
	44, 46, 47, 41, 50, 51, 49, 52, 
	4, 53, 54, 49, 55, 3, 56, 55, 
	4, 57, 58, 59, 60, 2, 61, 61, 
	61, 61, 61, 62, 61, 2, 61, 63, 
	2, 61, 2, 63, 1, 64, 64, 57, 
	61, 65, 61, 61, 61, 61, 61, 62, 
	2, 66, 13, 67, 66, 15, 68, 68, 
	68, 69, 12, 71, 71, 71, 71, 71, 
	72, 71, 12, 71, 73, 12, 71, 12, 
	73, 10, 71, 16, 17, 15, 20, 21, 
	15, 71, 74, 71, 71, 71, 71, 71, 
	72, 12, 75, 76, 76, 75, 30, 76, 
	76, 77, 78, 28, 79, 79, 79, 79, 
	79, 80, 79, 28, 79, 81, 28, 79, 
	28, 81, 26, 79, 31, 32, 30, 35, 
	36, 30, 79, 82, 79, 79, 79, 79, 
	79, 80, 28, 83, 45, 84, 83, 44, 
	44, 85, 44, 43, 86, 86, 86, 86, 
	86, 87, 86, 43, 86, 88, 43, 86, 
	43, 88, 41, 86, 89, 86, 86, 86, 
	86, 86, 87, 43, 90, 52, 91, 90, 
	44, 92, 93, 44, 51, 95, 95, 95, 
	95, 95, 96, 95, 51, 95, 97, 51, 
	95, 51, 97, 49, 95, 99, 95, 95, 
	95, 95, 95, 96, 51, 0
};

static const char _ironbee_config_trans_targs[] = {
	24, 26, 25, 24, 0, 24, 24, 24, 
	29, 30, 32, 30, 31, 30, 30, 7, 
	34, 8, 9, 30, 35, 10, 36, 30, 
	37, 38, 40, 38, 39, 38, 13, 42, 
	14, 15, 38, 43, 16, 44, 38, 45, 
	46, 48, 46, 47, 0, 46, 46, 50, 
	51, 53, 51, 52, 51, 51, 55, 24, 
	2, 28, 3, 24, 4, 24, 1, 27, 
	24, 24, 30, 6, 0, 11, 30, 30, 
	5, 33, 30, 38, 0, 38, 17, 38, 
	12, 41, 38, 46, 19, 20, 46, 18, 
	49, 46, 51, 22, 51, 23, 51, 51, 
	21, 54, 51, 51
};

static const unsigned char _ironbee_config_trans_actions[] = {
	47, 5, 112, 85, 0, 37, 39, 82, 
	155, 177, 5, 17, 112, 123, 183, 5, 
	5, 5, 5, 19, 159, 5, 159, 64, 
	151, 103, 5, 23, 112, 106, 5, 5, 
	5, 5, 25, 163, 5, 163, 67, 151, 
	49, 5, 29, 112, 1, 73, 70, 151, 
	52, 5, 35, 112, 79, 76, 151, 41, 
	3, 0, 0, 43, 91, 88, 5, 5, 
	45, 143, 15, 3, 7, 61, 172, 55, 
	5, 5, 127, 21, 9, 97, 61, 58, 
	5, 5, 131, 27, 3, 61, 94, 5, 
	5, 135, 31, 3, 33, 61, 147, 100, 
	5, 5, 115, 139
};

static const unsigned char _ironbee_config_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	11, 0, 0, 0, 0, 0, 109, 0, 
	0, 0, 0, 0, 0, 0, 109, 0, 
	0, 0, 0, 0, 0, 0, 11, 0, 
	0, 0, 0, 11, 0, 0, 0, 0
};

static const unsigned char _ironbee_config_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	13, 0, 0, 0, 0, 0, 13, 0, 
	0, 0, 0, 0, 0, 0, 13, 0, 
	0, 0, 0, 0, 0, 0, 13, 0, 
	0, 0, 0, 13, 0, 0, 0, 0
};

static const unsigned char _ironbee_config_eof_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 167, 0, 0, 0, 0, 
	0, 9, 0, 1, 1, 0, 1, 1, 
	0, 0, 0, 0, 0, 0, 119, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 1, 0, 0, 0, 0
};

static const short _ironbee_config_eof_trans[] = {
	0, 1, 0, 0, 0, 10, 0, 15, 
	15, 15, 15, 0, 26, 30, 30, 30, 
	30, 0, 41, 0, 0, 49, 0, 0, 
	0, 62, 62, 62, 65, 62, 0, 71, 
	71, 71, 71, 71, 71, 71, 0, 80, 
	80, 80, 80, 80, 80, 80, 0, 87, 
	87, 87, 87, 0, 95, 95, 95, 99
};

static const int ironbee_config_start = 24;
static const int ironbee_config_first_final = 24;
static const int ironbee_config_error = 0;

static const int ironbee_config_en_parameters = 30;
static const int ironbee_config_en_block_parameters = 38;
static const int ironbee_config_en_newblock = 46;
static const int ironbee_config_en_endblock = 51;
static const int ironbee_config_en_main = 24;


#line 818 "config-parser.rl"

ib_status_t ib_cfgparser_ragel_init(ib_cfgparser_t *cp) {
    assert(cp != NULL);
    assert(cp->ib != NULL);
    assert(cp->mp != NULL);

    ib_status_t rc;

    ib_cfg_log_debug(cp, "Initializing Ragel state machine.");

    /* Access all ragel state variables via structure. */
    
#line 830 "config-parser.rl"

    
#line 784 "config-parser.c"
	{
	 cp->fsm.cs = ironbee_config_start;
	 cp->fsm.top = 0;
	 cp->fsm.ts = 0;
	 cp->fsm.te = 0;
	 cp->fsm.act = 0;
	}

#line 832 "config-parser.rl"

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
    const size_t buffer_remaining = cp->fsm.te - cp->fsm.ts;

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
    
#line 972 "config-parser.rl"
    
#line 973 "config-parser.rl"
    
#line 974 "config-parser.rl"
    
#line 975 "config-parser.rl"

    
#line 943 "config-parser.c"
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
	case 15:
#line 1 "NONE"
	{ cp->fsm.ts = ( fsm_vars.p);}
	break;
#line 964 "config-parser.c"
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
#line 558 "config-parser.rl"
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
#line 569 "config-parser.rl"
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
#line 578 "config-parser.rl"
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
#line 588 "config-parser.rl"
	{
        cp->curr->line += 1;
    }
	break;
	case 4:
#line 593 "config-parser.rl"
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
#line 606 "config-parser.rl"
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
#line 659 "config-parser.rl"
	{
        if (cpbuf_append(cp, *( fsm_vars.p)) != IB_OK) {
            return IB_EALLOC;
        }
    }
	break;
	case 7:
#line 666 "config-parser.rl"
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
#line 679 "config-parser.rl"
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
#line 714 "config-parser.rl"
	{
        ib_cfgparser_pop_node(cp);
        cpbuf_clear(cp);
        cp->fsm.blkname = NULL;
    }
	break;
	case 10:
#line 751 "config-parser.rl"
	{ ( fsm_vars.p)--; { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; {
    }goto _again;} }
	break;
	case 11:
#line 752 "config-parser.rl"
	{ ( fsm_vars.p)--; { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; {
    }goto _again;} }
	break;
	case 12:
#line 762 "config-parser.rl"
	{ ( fsm_vars.p)--; { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; {
    }goto _again;} }
	break;
	case 16:
#line 1 "NONE"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 17:
#line 752 "config-parser.rl"
	{ cp->fsm.act = 4;}
	break;
	case 18:
#line 740 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 19:
#line 744 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ cpbuf_clear(cp); }}
	break;
	case 20:
#line 746 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; {
    }goto _again;} }}
	break;
	case 21:
#line 752 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;}
	break;
	case 22:
#line 752 "config-parser.rl"
	{{( fsm_vars.p) = (( cp->fsm.te))-1;}}
	break;
	case 23:
#line 1 "NONE"
	{	switch(  cp->fsm.act ) {
	case 0:
	{{ cp->fsm.cs = 0; goto _again;}}
	break;
	default:
	{{( fsm_vars.p) = (( cp->fsm.te))-1;}}
	break;
	}
	}
	break;
	case 24:
#line 762 "config-parser.rl"
	{ cp->fsm.act = 8;}
	break;
	case 25:
#line 756 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 26:
#line 757 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 27:
#line 759 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; {
    }goto _again;} }}
	break;
	case 28:
#line 762 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;}
	break;
	case 29:
#line 762 "config-parser.rl"
	{{( fsm_vars.p) = (( cp->fsm.te))-1;}}
	break;
	case 30:
#line 1 "NONE"
	{	switch(  cp->fsm.act ) {
	case 0:
	{{ cp->fsm.cs = 0; goto _again;}}
	break;
	default:
	{{( fsm_vars.p) = (( cp->fsm.te))-1;}}
	break;
	}
	}
	break;
	case 31:
#line 766 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 32:
#line 768 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ ( fsm_vars.p)--; { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; {
    }goto _again;} }}
	break;
	case 33:
#line 770 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ ( fsm_vars.p)--; { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; {
    }goto _again;} }}
	break;
	case 34:
#line 774 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;{ {
        if (cp->fsm.top >= 1023) {
            ib_cfg_log_debug(cp, "Recursion too deep during parse.");
            return IB_EOTHER;
        }
    { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 38; goto _again;}} }}
	break;
	case 35:
#line 774 "config-parser.rl"
	{{( fsm_vars.p) = (( cp->fsm.te))-1;}{ {
        if (cp->fsm.top >= 1023) {
            ib_cfg_log_debug(cp, "Recursion too deep during parse.");
            return IB_EOTHER;
        }
    { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 38; goto _again;}} }}
	break;
	case 36:
#line 778 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 37:
#line 780 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 38:
#line 782 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 39:
#line 788 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; {
    }goto _again;} }}
	break;
	case 40:
#line 786 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;}
	break;
	case 41:
#line 786 "config-parser.rl"
	{{( fsm_vars.p) = (( cp->fsm.te))-1;}}
	break;
	case 42:
#line 800 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ ( fsm_vars.p)--; {
        if (cp->fsm.top >= 1023) {
            ib_cfg_log_debug(cp, "Recursion too deep during parse.");
            return IB_EOTHER;
        }
    { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 46; goto _again;}}}}
	break;
	case 43:
#line 801 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{        {
        if (cp->fsm.top >= 1023) {
            ib_cfg_log_debug(cp, "Recursion too deep during parse.");
            return IB_EOTHER;
        }
    { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 51; goto _again;}}}}
	break;
	case 44:
#line 804 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 45:
#line 805 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 46:
#line 806 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 47:
#line 807 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{
                ib_cfg_log_error(
                    cp,
                    "Character \">\" encountered outside a block tag.");
                rc = IB_EOTHER;
                {( fsm_vars.p)++; goto _out; }
            }}
	break;
	case 48:
#line 792 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;}
	break;
	case 49:
#line 797 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;{ {
        if (cp->fsm.top >= 1023) {
            ib_cfg_log_debug(cp, "Recursion too deep during parse.");
            return IB_EOTHER;
        }
    { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 30; goto _again;}} }}
	break;
	case 50:
#line 797 "config-parser.rl"
	{{( fsm_vars.p) = (( cp->fsm.te))-1;}{ {
        if (cp->fsm.top >= 1023) {
            ib_cfg_log_debug(cp, "Recursion too deep during parse.");
            return IB_EOTHER;
        }
    { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 30; goto _again;}} }}
	break;
#line 1425 "config-parser.c"
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
	case 14:
#line 1 "NONE"
	{ cp->fsm.act = 0;}
	break;
#line 1442 "config-parser.c"
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
#line 558 "config-parser.rl"
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
#line 569 "config-parser.rl"
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
#line 606 "config-parser.rl"
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
#line 751 "config-parser.rl"
	{ ( fsm_vars.p)--; { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; {
    }goto _again;} }
	break;
	case 11:
#line 752 "config-parser.rl"
	{ ( fsm_vars.p)--; { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; {
    }goto _again;} }
	break;
	case 12:
#line 762 "config-parser.rl"
	{ ( fsm_vars.p)--; { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; {
    }goto _again;} }
	break;
#line 1555 "config-parser.c"
		}
	}
	}

	_out: {}
	}

#line 977 "config-parser.rl"

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
