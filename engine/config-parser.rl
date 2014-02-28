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
#include <ironbee/mm_mpool.h>
#include <ironbee/mm_mpool_lite.h>
#include <ironbee/mpool_lite.h>
#include <ironbee/path.h>

#include "config-parser.h"
#include "config_private.h"
#include "engine_private.h"

/* Caused by Ragel */
#ifdef __clang__
#pragma clang diagnostic ignored "-Wunreachable-code"
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Wunused-const-variable"
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
 * @param[in,out] mm Manager to copy out of.
 *
 * @return a buffer allocated from the temp_mm memory manager
 *         available in ib_cfgparser_ragel_parse_chunk. This buffer may be
 *         larger than the string stored in it if the length of the string is
 *         reduced by Javascript unescaping.
 */
static char *qstrdup(ib_cfgparser_t *cp, ib_mm_t mm)
{
    const char *start = (const char *)cp->buffer->data;
    const char *end = (const char *)(cp->buffer->data + cp->buffer->len - 1);
    size_t len = cp->buffer->len;

    /* Adjust for quoted value. */
    if ((*start == '"') && (*end == '"') && (start < end)) {
        start++;
        len -= 2;
    }

    return ib_mm_memdup_to_str(mm, start, len);
}

/**
 * Callback function to handel parsing of parser directives.
 */
typedef ib_status_t(*parse_directive_fn_t)(
    ib_cfgparser_t      *cp,
    ib_mm_t              temp_mm,
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
    ib_mm_t         mm,
    const char     *incfile,
    bool            if_exists
)
{
    char *real;

    real = ib_mm_alloc(mm, PATH_MAX);
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
    ib_cfgparser_t      *cp,
    ib_cfgparser_node_t *node,
    ib_mm_t              mm,
    const char          *incfile
)
{
    /* A temporary local value to store the parser state in.
     * We allocate this from mm to avoid putting the very large
     * buffer variable in fsm on the stack. */
    ib_cfgparser_fsm_t *fsm;
    ib_cfgparser_node_t *current_node;
    ib_status_t rc;

    /* Make the given node the current node for the file inclusion. */
    current_node = cp->curr;
    cp->curr = node;

    /* Allocate fsm in the heap as it contains a very large buffer. */
    fsm = ib_mm_alloc(mm, sizeof(*fsm));
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
 * @param[in] temp_mm Memory manager to use.
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
    ib_cfgparser_t      *cp,
    ib_mm_t              temp_mm,
    ib_cfgparser_node_t *node,
    bool                 if_exists
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

    ib_mm_t mm;
    ib_mpool_lite_t *local_mpl = NULL;
    rc = ib_mpool_lite_create(&local_mpl);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Failed to create local memory pool.");
        goto cleanup;
    }
    mm = ib_mm_mpool_lite(local_mpl);

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
    incfile = ib_util_relative_file(mm, node->file, pval);
    if (incfile == NULL) {
        ib_cfg_log_error(cp, "Error resolving included file \"%s\": %s",
                         node->file, strerror(errno));
        ib_mpool_lite_destroy(local_mpl);
        rc = IB_ENOENT;
        goto cleanup;
    }

    /* Log the realpath of incfile to aid in configuring IB accurately. */
    rc = include_parse_directive_impl_log_realpath(
        cp,
        mm,
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
    rc = include_parse_directive_impl_parse(cp, node, mm, incfile);
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
    if (local_mpl != NULL) {
        ib_mpool_lite_destroy(local_mpl);
    }

    /* IncludeIfExists never causes failure. */
    return (if_exists)? IB_OK : rc;
}

//! Proxy to include_parse_directive_impl with if_exists = true.
static ib_status_t include_if_exists_parse_directive(
    ib_cfgparser_t      *cp,
    ib_mm_t              temp_mm,
    ib_cfgparser_node_t *node
) {
    return include_parse_directive_impl(cp, temp_mm, node, true);
}

//! Proxy to include_parse_directive_impl with if_exists = false.
static ib_status_t include_parse_directive(
    ib_cfgparser_t      *cp,
    ib_mm_t              temp_mm,
    ib_cfgparser_node_t *node
) {
    return include_parse_directive_impl(cp, temp_mm, node, false);
}

static ib_status_t loglevel_parse_directive(
    ib_cfgparser_t      *cp,
    ib_mm_t              mm,
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

%%{
    machine ironbee_config;

    prepush {
        if (cp->fsm.top >= 1023) {
            ib_cfg_log_error(cp, "Recursion too deep during parse.");
            return IB_EOTHER;
        }
    }

    postpop {
    }

    action error_action {
        rc = IB_EOTHER;
        ib_cfg_log_error(
            cp,
            "parser error near %s:%zu.",
            cp->curr->file,
            cp->curr->line);
        ib_cfg_log_debug(cp, "Current buffer is [[[[%.*s]]]]", (int)blen, buf);
    }

    # Parameter
    action push_param {
        tmp_str = qstrdup(cp, config_mm);
        if (tmp_str == NULL) {
            return IB_EALLOC;
        }
        ib_list_push(cp->fsm.plist, tmp_str);
        tmp_str = NULL;
        cpbuf_clear(cp);
    }
    action push_blkparam {
        tmp_str = qstrdup(cp, config_mm);
        if (tmp_str == NULL) {
            return IB_EALLOC;
        }
        ib_list_push(cp->fsm.plist, tmp_str);
        tmp_str = NULL;
        cpbuf_clear(cp);
    }

    action newline {
        cp->curr->line += 1;
    }

    # Directives
    action start_dir {
        if (cp->buffer->len == 0) {
            ib_cfg_log_error(cp, "Directive name is 0 length.");
            return IB_EOTHER;
        }
        cp->fsm.directive =
            ib_mm_memdup_to_str(cp->mm, cp->buffer->data, cp->buffer->len);
        if (cp->fsm.directive == NULL) {
            return IB_EALLOC;
        }
        ib_list_clear(cp->fsm.plist);
        cpbuf_clear(cp);
    }
    action push_dir {
        ib_cfgparser_node_t *node = NULL;
        rc = ib_cfgparser_node_create(&node, cp);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp, "Cannot create node.");
            return rc;
        }
        node->directive = cp->fsm.directive;
        cp->fsm.directive = NULL;
        node->file = ib_mm_strdup(cp->mm, cp->curr->file);
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
                rc = (parse_directive_table[i].fn)(cp, temp_mm, node);
                if (rc != IB_OK) {
                    ib_cfg_log_error(
                        cp,
                        "Parse directive %s failed.",
                        node->directive);
                }
            }
        }
    }

    action cpbuf_append {
        if (cpbuf_append(cp, *fpc) != IB_OK) {
            return IB_EALLOC;
        }
    }

    # Blocks
    action start_block {
        if (cp->buffer->len == 0) {
            ib_cfg_log_error(cp, "Block name is 0 length.");
            return IB_EOTHER;
        }
        cp->fsm.blkname =
            ib_mm_memdup_to_str(cp->mm, cp->buffer->data, cp->buffer->len);
        if (cp->fsm.blkname == NULL) {
            return IB_EALLOC;
        }
        ib_list_clear(cp->fsm.plist);
        cpbuf_clear(cp);
    }
    action push_block {
        ib_cfgparser_node_t *node = NULL;
        rc = ib_cfgparser_node_create(&node, cp);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp, "Cannot create node.");
            return rc;
        }
        node->directive = cp->fsm.blkname;
        /* NOTE: We do not clear blkname now. */
        node->file = ib_mm_strdup(cp->mm, cp->curr->file);
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
    action pop_block {
        ib_cfgparser_pop_node(cp);
        cpbuf_clear(cp);
        cp->fsm.blkname = NULL;
    }

    # include file logic
    # Whitespace.
    WS = [ \t];
    # End of line.
    EOL = '\r'? '\n';
    # Continuation.
    CONT = '\\' EOL;

    # qchar is any escaped character except what might be a line ending
    qchar = '\\' (any - [\r\n]);
    qtoken = '"' ( qchar | ( any - ["\\] ) )* '"';
    token = (qchar | (any - (WS | '\r' | '\n' | [<>#"\\])))
            (qchar | (any - (WS | '\r' | '\n' | [<>"\\])))*;
    param = qtoken | token;

    # A comment is any string starting with # and not including a newline.
    comment = '#' (any - ('\r' | '\n'))*;

    # The parameters machine pull in parameter.
    parameters := |*
        WS;
        CONT  $newline
              # CONT and param share a starting char of '\'.
              # Clear the token buffer if we match a CONT.
              { cpbuf_clear(cp); };
        EOL   $newline
              @push_dir { fret; };
        param $cpbuf_append
              %push_param
              $/push_param
              $/push_dir
              $err{ fhold; fret; }
              $eof{ fhold; fret; };
    *|;

    block_parameters := |*
        WS;
        CONT  $newline;
        ">"   @push_block
              { fret; };
        param $cpbuf_append
              %push_blkparam
              $err{ fhold; fret; };
    *|;

    newblock := |*
        WS;
        CONT   $newline
               $!error_action { fhold; fret; };
        EOL    $newline
               $!error_action { fhold; fret; };
        token  $cpbuf_append
               %start_block
               $!error_action
               { fcall block_parameters; };
    *|;

    endblock := |*
        WS      $eof(error_action);
        CONT    $newline
                $eof(error_action);
	    EOL     $newline
                $eof(error_action);
        token   $cpbuf_append
                $!error_action
                %pop_block
                $eof(error_action);
        ">"     $eof(error_action)
                { fret; };
    *|;

    main := |*
        comment;

        #  A directive.
        token $cpbuf_append
              %start_dir
              { fcall parameters; };

        # Handle block configurations <Site... >.
        "<" (any - "/") { fhold; fcall newblock;};
        "</"            {        fcall endblock;};

        # Eat space.
        WS;
        CONT $newline;
        EOL  $newline;
        ">" {
                ib_cfg_log_error(
                    cp,
                    "Character \">\" encountered outside a block tag.");
                rc = IB_EOTHER;
                fbreak;
            };
    *|;
}%%

%% write data;

ib_status_t ib_cfgparser_ragel_init(ib_cfgparser_t *cp) {
    assert(cp != NULL);
    assert(cp->ib != NULL);
    assert(cp->mp != NULL);

    ib_status_t rc;

    /* Access all ragel state variables via structure. */
    %% access cp->fsm.;

    %% write init;

    rc = ib_list_create(&(cp->fsm.plist), ib_mm_mpool(cp->mp));
    if (rc != IB_OK) {
        return rc;
    }

    cp->fsm.directive = NULL;
    cp->fsm.blkname = NULL;

    rc = ib_vector_create(&(cp->fsm.ts_buffer), ib_mm_mpool(cp->mp), 0);
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
    const char     *buf,
    const size_t    blen,
    const int       is_last_chunk)
{
    assert(cp != NULL);
    assert(cp->ib != NULL);

    ib_engine_t *ib_engine = cp->ib;

    /* Temporary memory pool. */
    ib_mm_t temp_mm = ib_engine_mm_temp_get(ib_engine);

    /* Configuration memory pool. */
    ib_mm_t config_mm = ib_engine_mm_config_get(ib_engine);

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
    %% access cp->fsm.;
    %% variable p fsm_vars.p;
    %% variable pe fsm_vars.pe;
    %% variable eof fsm_vars.eof;

    %% write exec;

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
