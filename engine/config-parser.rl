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
static char *qstrdup(ib_cfgparser_t *cp, ib_mpool_t* mp)
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

%%{
    machine ironbee_config;

    action error_action {
        rc = IB_EOTHER;
        ib_cfg_log_error(
            cp,
            "parser error near %s:%zu.",
            cp->curr->file,
            cp->curr->line);
    }

    # Parameter
    action push_param {
        cp->fsm.pval = qstrdup(cp, config_mp);
        if (cp->fsm.pval == NULL) {
            return IB_EALLOC;
        }
        ib_list_push(cp->fsm.plist, cp->fsm.pval);
        cpbuf_clear(cp);
    }
    action push_blkparam {
        cp->fsm.pval = qstrdup(cp, config_mp);
        if (cp->fsm.pval == NULL) {
            return IB_EALLOC;
        }
        ib_list_push(cp->fsm.plist, cp->fsm.pval);
        cpbuf_clear(cp);
    }

    action newline {
        cp->curr->line += 1;
    }

    # Directives
    action start_dir {
        cp->fsm.directive =
            ib_mpool_memdup_to_str(cp->mp, cp->buffer, cp->buffer_len);
        if (cp->fsm.directive == NULL) {
            return IB_EALLOC;
        }
        ib_list_clear(cp->fsm.plist);
        cpbuf_clear(cp);
    }
    action push_dir {
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

    action cpbuf_append {
        if (cpbuf_append(cp, *fpc) != IB_OK) {
            return IB_EALLOC;
        }
    }

    # Blocks
    action start_block {
        cp->fsm.blkname =
            ib_mpool_memdup_to_str(cp->mp, cp->buffer, cp->buffer_len);
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
    # Non-breaking space. Space that does not terminate a statement.
    NBSP = ( WS | '\\' EOL );


    qchar = '\\' any;
    qtoken = '"' ( qchar | ( any - ["\\] ) )* '"';
    token = (qchar | (any - (WS | EOL | [<>#"\\])))
            (qchar | (any - (WS | EOL | [<>"\\])))*;
    param = qtoken | token;
    keyval = token '=' param;
    iparam = ( '"' (any - (EOL | '"'))+ '"' ) | (any - (WS | EOL) )+;

    # A comment is any string starting with # and not including a newline.
    # Notice that we do *not* make allowances for NBSP used in line comments.
    comment = '#' (any -- EOL)*;

    # The parameters machine pull in parameter.
    parameters := |*
        WS;
        CONT  $newline;
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

    ib_cfg_log_debug(cp, "Initializing Ragel state machine.");

    /* Access all ragel state variables via structure. */
    %% access cp->fsm.;

    %% write init;

    ib_cfg_log_info(cp, "Initializing IronBee parse values.");

    rc = ib_list_create(&(cp->fsm.plist), cp->mp);
    if (rc != IB_OK) {
        return rc;
    }

    cp->fsm.directive = NULL;
    cp->fsm.blkname = NULL;
    cp->fsm.pval = NULL;

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
    fsm_vars_t fsm_vars;

    fsm_vars.p = buf;
    fsm_vars.pe = buf + blen;
    fsm_vars.eof = (is_last_chunk ? fsm_vars.pe : NULL);

    /* Access all ragel state variables via structure. */
    %% access cp->fsm.;
    %% variable p fsm_vars.p;
    %% variable pe fsm_vars.pe;
    %% variable eof fsm_vars.eof;

    %% write exec;

    /* On the last chunk, sanity check things. */
    if (is_last_chunk) {
        if (cp->fsm.blkname != NULL) {
            ib_cfg_log_error(
                cp,
                "Block name is not empty at end of config input");
            return IB_EINVAL;
        }
    }

    return rc;
}
