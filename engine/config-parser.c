
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
    if (cp->buffer->len >= 8192) {
        ib_cfg_log_error(
            cp,
            "Token size limit exceeded. Will not grow past %zu characters.",
            cp->buffer->len);
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
            "Error truncating token buffer: %s",
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
                "Error accessing included file \"%s\": %s",
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
                "Error stating include file \"%s\": %s",
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
        ib_cfg_log_error(cp, "Failed to initialize new parser.");
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
 *
 * @param[in] cp Configuration parser.
 * @param[in] temp_mp Memory pool to use.
 * @param[in] node The parse node containing the directive.
 * @param[in] if_exists Choose the error message and log level by
 *            whether the file must exist or may be missing.
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
        ib_cfg_log_error(cp, "Error resolving included file \"%s\": %s",
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

    ib_cfg_log_debug(cp, "Finished processing include file \"%s\"", incfile);

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


#line 800 "config-parser.rl"



#line 550 "config-parser.c"
static const char _ironbee_config_actions[] = {
	0, 1, 0, 1, 3, 1, 6, 1, 
	10, 1, 12, 1, 13, 1, 14, 1, 
	16, 1, 20, 1, 21, 1, 25, 1, 
	26, 1, 30, 1, 31, 1, 34, 1, 
	36, 1, 37, 1, 38, 1, 39, 1, 
	42, 1, 43, 1, 45, 2, 0, 30, 
	2, 0, 36, 2, 1, 19, 2, 2, 
	24, 2, 3, 6, 2, 3, 17, 2, 
	3, 22, 2, 3, 27, 2, 3, 28, 
	2, 3, 32, 2, 3, 33, 2, 3, 
	40, 2, 3, 41, 2, 4, 44, 2, 
	6, 3, 2, 7, 29, 2, 8, 23, 
	2, 9, 35, 2, 12, 25, 2, 15, 
	6, 3, 1, 5, 11, 3, 3, 5, 
	18, 3, 9, 0, 35, 4, 1, 5, 
	10, 11, 4, 1, 5, 11, 19, 5, 
	1, 5, 10, 11, 20
};

static const char _ironbee_config_key_offsets[] = {
	0, 0, 2, 3, 4, 6, 7, 9, 
	10, 12, 14, 16, 17, 19, 21, 23, 
	25, 26, 28, 29, 31, 32, 34, 35, 
	37, 38, 47, 55, 57, 66, 74, 74, 
	83, 91, 91, 100, 108, 117
};

static const char _ironbee_config_trans_keys[] = {
	10, 13, 10, 47, 10, 13, 10, 10, 
	13, 10, 34, 92, 10, 13, 10, 13, 
	10, 10, 13, 34, 92, 10, 13, 10, 
	13, 10, 10, 13, 10, 10, 13, 10, 
	10, 13, 10, 10, 13, 10, 9, 10, 
	13, 32, 34, 35, 60, 62, 92, 13, 
	32, 34, 60, 62, 92, 9, 10, 10, 
	13, 9, 10, 13, 32, 34, 35, 60, 
	62, 92, 13, 32, 34, 60, 62, 92, 
	9, 10, 9, 10, 13, 32, 34, 35, 
	60, 62, 92, 13, 32, 34, 60, 62, 
	92, 9, 10, 9, 10, 13, 32, 60, 
	62, 92, 34, 35, 13, 32, 34, 60, 
	62, 92, 9, 10, 9, 10, 13, 32, 
	60, 62, 92, 34, 35, 13, 32, 34, 
	60, 62, 92, 9, 10, 0
};

static const char _ironbee_config_single_lengths[] = {
	0, 2, 1, 1, 2, 1, 2, 1, 
	2, 2, 2, 1, 2, 2, 2, 2, 
	1, 2, 1, 2, 1, 2, 1, 2, 
	1, 9, 6, 2, 9, 6, 0, 9, 
	6, 0, 7, 6, 7, 6
};

static const char _ironbee_config_range_lengths[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 1, 0, 0, 1, 0, 0, 
	1, 0, 1, 1, 1, 1
};

static const unsigned char _ironbee_config_index_offsets[] = {
	0, 0, 3, 5, 7, 10, 12, 15, 
	17, 20, 23, 26, 28, 31, 34, 37, 
	40, 42, 45, 47, 50, 52, 55, 57, 
	60, 62, 72, 80, 83, 93, 101, 102, 
	112, 120, 121, 130, 138, 147
};

static const char _ironbee_config_indicies[] = {
	0, 0, 1, 2, 3, 5, 4, 6, 
	7, 1, 6, 3, 10, 10, 9, 11, 
	3, 13, 14, 12, 15, 15, 12, 16, 
	17, 9, 16, 3, 20, 20, 19, 22, 
	23, 21, 24, 24, 21, 25, 26, 19, 
	25, 3, 29, 29, 28, 31, 30, 32, 
	33, 28, 32, 30, 36, 36, 35, 37, 
	3, 38, 39, 35, 38, 3, 40, 2, 
	41, 40, 3, 42, 43, 44, 45, 1, 
	46, 46, 46, 46, 46, 47, 46, 1, 
	48, 48, 42, 49, 11, 50, 49, 12, 
	15, 15, 15, 51, 9, 53, 53, 53, 
	53, 53, 54, 53, 9, 53, 55, 24, 
	24, 55, 21, 24, 24, 56, 57, 19, 
	58, 58, 58, 58, 58, 59, 58, 19, 
	58, 60, 31, 61, 60, 30, 30, 62, 
	30, 28, 63, 63, 63, 63, 63, 64, 
	63, 28, 65, 37, 66, 65, 30, 67, 
	68, 30, 35, 70, 70, 70, 70, 70, 
	71, 70, 35, 0
};

static const char _ironbee_config_trans_targs[] = {
	25, 26, 25, 0, 25, 25, 25, 5, 
	28, 29, 28, 28, 8, 30, 9, 0, 
	28, 11, 31, 32, 31, 13, 33, 14, 
	0, 31, 16, 34, 35, 34, 0, 34, 
	34, 20, 36, 37, 36, 36, 36, 24, 
	25, 2, 27, 3, 25, 4, 25, 1, 
	25, 28, 7, 10, 28, 28, 6, 31, 
	31, 15, 31, 12, 34, 18, 19, 34, 
	17, 36, 22, 36, 23, 36, 36, 21
};

static const char _ironbee_config_trans_actions[] = {
	43, 102, 81, 0, 33, 35, 78, 3, 
	127, 102, 17, 109, 5, 5, 5, 7, 
	60, 3, 99, 102, 21, 5, 5, 5, 
	9, 63, 3, 45, 102, 25, 1, 69, 
	66, 3, 48, 102, 31, 75, 72, 3, 
	37, 3, 0, 0, 39, 87, 84, 5, 
	41, 15, 3, 57, 122, 51, 5, 19, 
	93, 57, 54, 5, 23, 3, 57, 90, 
	5, 27, 3, 29, 57, 113, 96, 5
};

static const char _ironbee_config_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 11, 0, 0, 11, 0, 0, 11, 
	0, 0, 11, 0, 11, 0
};

static const char _ironbee_config_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 13, 0, 0, 13, 0, 0, 13, 
	0, 0, 13, 0, 13, 0
};

static const char _ironbee_config_eof_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	117, 117, 117, 0, 0, 9, 9, 9, 
	0, 0, 1, 1, 1, 0, 1, 1, 
	1, 0, 0, 0, 105, 0, 0, 0, 
	0, 0, 0, 0, 1, 0
};

static const unsigned char _ironbee_config_eof_trans[] = {
	0, 1, 0, 0, 0, 0, 9, 0, 
	0, 0, 0, 0, 19, 0, 0, 0, 
	0, 28, 0, 0, 0, 35, 0, 0, 
	0, 0, 47, 49, 0, 53, 53, 0, 
	59, 59, 0, 64, 0, 70
};

static const int ironbee_config_start = 25;
static const int ironbee_config_first_final = 25;
static const int ironbee_config_error = 0;

static const int ironbee_config_en_parameters = 28;
static const int ironbee_config_en_block_parameters = 31;
static const int ironbee_config_en_newblock = 34;
static const int ironbee_config_en_endblock = 36;
static const int ironbee_config_en_main = 25;


#line 803 "config-parser.rl"

ib_status_t ib_cfgparser_ragel_init(ib_cfgparser_t *cp) {
    assert(cp != NULL);
    assert(cp->ib != NULL);
    assert(cp->mp != NULL);

    ib_status_t rc;

    /* Access all ragel state variables via structure. */
    
#line 813 "config-parser.rl"

    
#line 726 "config-parser.c"
	{
	 cp->fsm.cs = ironbee_config_start;
	 cp->fsm.top = 0;
	 cp->fsm.ts = 0;
	 cp->fsm.te = 0;
	 cp->fsm.act = 0;
	}

#line 815 "config-parser.rl"

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
 *
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
    
#line 952 "config-parser.rl"
    
#line 953 "config-parser.rl"
    
#line 954 "config-parser.rl"
    
#line 955 "config-parser.rl"

    
#line 882 "config-parser.c"
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
#line 903 "config-parser.c"
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
#line 555 "config-parser.rl"
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
#line 566 "config-parser.rl"
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
#line 575 "config-parser.rl"
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
#line 585 "config-parser.rl"
	{
        cp->curr->line += 1;
    }
	break;
	case 4:
#line 590 "config-parser.rl"
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
#line 603 "config-parser.rl"
	{
        ib_cfgparser_node_t *node = NULL;
        rc = ib_cfgparser_node_create(&node, cp);
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
        rc = ib_list_push(cp->curr->children, node);
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
            }
        }
    }
	break;
	case 6:
#line 650 "config-parser.rl"
	{
        if (cpbuf_append(cp, *( fsm_vars.p)) != IB_OK) {
            return IB_EALLOC;
        }
    }
	break;
	case 7:
#line 657 "config-parser.rl"
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
#line 670 "config-parser.rl"
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
#line 699 "config-parser.rl"
	{
        ib_cfgparser_pop_node(cp);
        cpbuf_clear(cp);
        cp->fsm.blkname = NULL;
    }
	break;
	case 10:
#line 736 "config-parser.rl"
	{ ( fsm_vars.p)--; { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; {
    }goto _again;} }
	break;
	case 11:
#line 737 "config-parser.rl"
	{ ( fsm_vars.p)--; { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; {
    }goto _again;} }
	break;
	case 12:
#line 747 "config-parser.rl"
	{ ( fsm_vars.p)--; { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; {
    }goto _again;} }
	break;
	case 15:
#line 1 "NONE"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 16:
#line 725 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 17:
#line 729 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ cpbuf_clear(cp); }}
	break;
	case 18:
#line 731 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; {
    }goto _again;} }}
	break;
	case 19:
#line 737 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;}
	break;
	case 20:
#line 737 "config-parser.rl"
	{{( fsm_vars.p) = (( cp->fsm.te))-1;}}
	break;
	case 21:
#line 741 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 22:
#line 742 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 23:
#line 744 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; {
    }goto _again;} }}
	break;
	case 24:
#line 747 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;}
	break;
	case 25:
#line 747 "config-parser.rl"
	{{( fsm_vars.p) = (( cp->fsm.te))-1;}}
	break;
	case 26:
#line 751 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 27:
#line 753 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ ( fsm_vars.p)--; { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; {
    }goto _again;} }}
	break;
	case 28:
#line 755 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ ( fsm_vars.p)--; { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; {
    }goto _again;} }}
	break;
	case 29:
#line 759 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;{ {
        if (cp->fsm.top >= 1023) {
            ib_cfg_log_error(cp, "Recursion too deep during parse.");
            return IB_EOTHER;
        }
    { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 31; goto _again;}} }}
	break;
	case 30:
#line 759 "config-parser.rl"
	{{( fsm_vars.p) = (( cp->fsm.te))-1;}{ {
        if (cp->fsm.top >= 1023) {
            ib_cfg_log_error(cp, "Recursion too deep during parse.");
            return IB_EOTHER;
        }
    { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 31; goto _again;}} }}
	break;
	case 31:
#line 763 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 32:
#line 765 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 33:
#line 767 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 34:
#line 773 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; {
    }goto _again;} }}
	break;
	case 35:
#line 771 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;}
	break;
	case 36:
#line 771 "config-parser.rl"
	{{( fsm_vars.p) = (( cp->fsm.te))-1;}}
	break;
	case 37:
#line 785 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ ( fsm_vars.p)--; {
        if (cp->fsm.top >= 1023) {
            ib_cfg_log_error(cp, "Recursion too deep during parse.");
            return IB_EOTHER;
        }
    { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 34; goto _again;}}}}
	break;
	case 38:
#line 786 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{        {
        if (cp->fsm.top >= 1023) {
            ib_cfg_log_error(cp, "Recursion too deep during parse.");
            return IB_EOTHER;
        }
    { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 36; goto _again;}}}}
	break;
	case 39:
#line 789 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 40:
#line 790 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 41:
#line 791 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 42:
#line 792 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{
                ib_cfg_log_error(
                    cp,
                    "Character \">\" encountered outside a block tag.");
                rc = IB_EOTHER;
                {( fsm_vars.p)++; goto _out; }
            }}
	break;
	case 43:
#line 777 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;}
	break;
	case 44:
#line 782 "config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;{ {
        if (cp->fsm.top >= 1023) {
            ib_cfg_log_error(cp, "Recursion too deep during parse.");
            return IB_EOTHER;
        }
    { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 28; goto _again;}} }}
	break;
	case 45:
#line 782 "config-parser.rl"
	{{( fsm_vars.p) = (( cp->fsm.te))-1;}{ {
        if (cp->fsm.top >= 1023) {
            ib_cfg_log_error(cp, "Recursion too deep during parse.");
            return IB_EOTHER;
        }
    { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 28; goto _again;}} }}
	break;
#line 1320 "config-parser.c"
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
#line 1333 "config-parser.c"
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
#line 555 "config-parser.rl"
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
#line 566 "config-parser.rl"
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
#line 603 "config-parser.rl"
	{
        ib_cfgparser_node_t *node = NULL;
        rc = ib_cfgparser_node_create(&node, cp);
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
        rc = ib_list_push(cp->curr->children, node);
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
            }
        }
    }
	break;
	case 10:
#line 736 "config-parser.rl"
	{ ( fsm_vars.p)--; { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; {
    }goto _again;} }
	break;
	case 11:
#line 737 "config-parser.rl"
	{ ( fsm_vars.p)--; { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; {
    }goto _again;} }
	break;
	case 12:
#line 747 "config-parser.rl"
	{ ( fsm_vars.p)--; { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; {
    }goto _again;} }
	break;
#line 1440 "config-parser.c"
		}
	}
	}

	_out: {}
	}

#line 957 "config-parser.rl"

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
