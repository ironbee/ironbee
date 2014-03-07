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
 * @brief IronBee --- Configuration
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/config.h>
#include "config_private.h"

#include "config-parser.h"
#include "engine_private.h"

#include <ironbee/context.h>
#include <ironbee/flags.h>
#include <ironbee/mm_mpool.h>
#include <ironbee/mm_mpool_lite.h>
#include <ironbee/strval.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#if defined(__cplusplus) && !defined(__STDC_FORMAT_MACROS)
/* C99 requires that inttypes.h only exposes PRI* macros
 * for C++ implementations if this is defined: */
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>
#include <libgen.h>

/* -- Internal -- */

typedef struct cfgp_dir_t cfgp_dir_t;
typedef struct cfgp_blk_t cfgp_blk_t;

struct cfgp_dir_t {
    char                     *name;     /**< Directive name */
    ib_list_t                *params;   /**< Directive parameters */
};

struct cfgp_blk_t {
    char                     *name;     /**< Block name */
    ib_list_t                *params;   /**< Block parameters */
    ib_list_t                *dirs;     /**< Block directives */
};

ib_status_t ib_cfgparser_node_create(ib_cfgparser_node_t **node,
                                     ib_cfgparser_t *cfgparser)
{
    assert(node != NULL);
    assert(cfgparser != NULL);
    assert(cfgparser->mp != NULL);

    ib_mm_t mm = cfgparser->mm;
    ib_status_t rc;

    ib_cfgparser_node_t *new_node = ib_mm_calloc(mm, sizeof(*new_node), 1);
    if (new_node == NULL) {
        return IB_EALLOC;
    }

    rc = ib_list_create(&(new_node->params), mm);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_list_create(&(new_node->children), mm);
    if (rc != IB_OK) {
        return rc;
    }

    *node = new_node;

    return IB_OK;
}

ib_status_t ib_cfgparser_create(ib_cfgparser_t **pcp, ib_engine_t *ib)
{
    assert(pcp != NULL);
    assert(ib != NULL);

    ib_mpool_t *mp;
    ib_mm_t mm;
    ib_status_t rc;
    ib_cfgparser_t *cp;

    *pcp = NULL;

    /* Create parser memory pool */
    rc = ib_mpool_create(&mp, "cfgparser", ib->mp);
    if (rc != IB_OK) {
        return IB_EALLOC;
    }
    mm = ib_mm_mpool(mp);

    /* Create the configuration parser object from the memory pool */
    cp = (ib_cfgparser_t *)ib_mm_calloc(mm, sizeof(*cp), 1);
    if (cp == NULL) {
        rc = IB_EALLOC;
        goto failed;
    }

    /* Store pointers to the engine and the memory pool */
    cp->ib = ib;
    cp->mp = mp;
    cp->mm = mm;

    /* Create the stack */
    rc = ib_list_create(&(cp->stack), cp->mm);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Create the parse tree root. */
    rc = ib_cfgparser_node_create(&(cp->root), cp);
    if (rc != IB_OK) {
        goto failed;
    }

    cp->root->type = IB_CFGPARSER_NODE_ROOT;
    cp->root->file = "[root]";
    cp->root->line = 1;
    cp->root->directive = "[root]";
    cp->curr = cp->root;

    /* Build a buffer for aggregating tokens into. */
    rc = ib_vector_create(
        &(cp->buffer),
        cp->mm,
        IB_VECTOR_NEVER_SHRINK
    );
    if (rc != IB_OK) {
        goto failed;
    }

    /* Initialize the Ragel state machine state. */
    rc = ib_cfgparser_ragel_init(cp);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Other fields are NULLed via calloc */
    *pcp = cp;
    return IB_OK;

failed:
    /* Make sure everything is cleaned up on failure */
    ib_mpool_destroy(mp);

    return rc;
}

void ib_cfgparser_pop_node(ib_cfgparser_t *cp)
{
    assert(cp != NULL);
    assert(cp->curr != NULL);

    if (cp->curr->parent != NULL) {
        cp->curr = cp->curr->parent;
    }
}

ib_status_t ib_cfgparser_push_node(ib_cfgparser_t *cp,
                                   ib_cfgparser_node_t *node)
{
    assert(cp != NULL);
    assert(cp->curr != NULL);
    assert(cp->curr->children != NULL);
    assert(node != NULL);

    ib_status_t rc;

    /* Set down-link. */
    rc = ib_list_push(cp->curr->children, node);
    if (rc != IB_OK) {
        return rc;
    }

    /* Set up-link. */
    node->parent = cp->curr;

    /* This node is now the current node. */
    cp->curr = node;

    return rc;
}

/// @todo Create a ib_cfgparser_parse_ex that can parse non-files (DBs, etc)


ib_status_t ib_cfgparser_parse_private(
    ib_cfgparser_t *cp,
    const char *file,
    bool eof_mask
) {
    assert(cp != NULL);
    assert(cp->ib != NULL);

    int ec             = 0;    /* Error code for sys calls. */
    int fd             = 0;    /* File to read. */
    const size_t bufsz = 8192; /* Buffer size. */
    size_t buflen      = 0;    /* Last char in buffer. */
    char *buf          = NULL; /* Buffer. */
    char *pathbuf;
    const char *save_cwd;      /* CWD, used to restore during cleanup  */
    ib_cfgparser_node_t *node; /* Parser node for this file. */
    ib_cfgparser_node_t *save_node = NULL; /* Previous current node. */

    ib_status_t rc = IB_OK;
    unsigned error_count = 0;
    ib_status_t error_rc = IB_OK;

    /* Local memory pool. This is released at the end of this function. */
    ib_mpool_lite_t *local_mp;
    ib_mm_t          local_mm;

    /* Store the current file and path in the save_ stack variables */
    save_cwd = cp->cur_cwd;

    /* Create a memory pool for allocations local to this function.
     * This is destroyed at the end of this function. */
    rc = ib_mpool_lite_create(&local_mp);
    if (rc != IB_OK) {
        return rc;
    }
    local_mm = ib_mm_mpool_lite(local_mp);

    /* Open the file to read. */
    fd = open(file, O_RDONLY);
    if (fd == -1) {
        ec = errno;
        ib_cfg_log_error(cp, "Error opening config file \"%s\": (%d) %s",
                         file, ec, strerror(ec));
        rc = IB_EINVAL;
        goto cleanup_fd;
    }

    /* Build a buffer to read the file into. */
    buf = (char *)ib_mm_alloc(local_mm, sizeof(*buf)*bufsz);
    if (buf == NULL) {
        rc = IB_EALLOC;
        goto cleanup_buf;
    }

    /* Build a parse node to represent the parsing work we are doing. */
    node = NULL;
    rc = ib_cfgparser_node_create(&node, cp);
    if (rc != IB_OK) {
        goto cleanup_create_node;
    }
    node->file = ib_mm_strdup(cp->mm, file);
    node->line = 1;
    node->type = IB_CFGPARSER_NODE_FILE;
    node->directive = "[file]";
    rc = ib_cfgparser_push_node(cp, node);
    if (rc != IB_OK) {
        goto cleanup_push_node;
    }
    save_node = cp->curr;

    /* Store the new file and path in the parser object */
    pathbuf = (char *)ib_mm_strdup(cp->mm, file);
    if (pathbuf == NULL) {
        rc = IB_EALLOC;
        goto cleanup;
    }
    cp->cur_cwd = dirname(pathbuf);

    /* Fill the buffer, parse each line. Conditionally read another line. */
    do {
        buflen = read(fd, buf, bufsz);

        if ( buflen == 0 ) { /* EOF */
            rc = ib_cfgparser_ragel_parse_chunk(
                cp, buf, buflen, true && eof_mask);
            if (rc != IB_OK) {
                ++error_count;
                error_rc = rc;
            }
            break;
        }
        else if ( buflen > 0 ) {
            rc = ib_cfgparser_ragel_parse_chunk(cp, buf, buflen, false);
            if (rc != IB_OK) {
                ++error_count;
                error_rc = rc;
            }
        }
        else {
            /* buflen < 0. This is an error. */
            ib_cfg_log_error(
                cp,
                "Error reading config file \"%s\": %s",
                file,
                strerror(errno));
            rc = IB_ETRUNC;
            goto cleanup;
        }
    } while (buflen > 0);


cleanup:

    ib_cfgparser_pop_node(cp);
    cp->curr = save_node;
cleanup_create_node:
cleanup_push_node:
cleanup_buf:

    close(fd);
cleanup_fd:
    ib_mpool_lite_destroy(local_mp);

    cp->cur_cwd = save_cwd;

    if ( (error_count == 0) && (rc == IB_OK) ) {
        return IB_OK;
    }
    else if (rc == IB_OK) {
        rc = error_rc;
    }
    ib_cfg_log_error(
        cp,
        "%u Error(s) parsing config file: %s",
        error_count,
        ib_status_to_string(rc));

    return rc;
}

ib_status_t ib_cfgparser_parse(ib_cfgparser_t *cp, const char *file)
{
    ib_status_t rc;

    rc = ib_cfgparser_parse_private(cp, file, true);

    /* Reset the parser. */
    cp->curr = cp->root;

    return rc;
}

ib_status_t ib_cfgparser_parse_buffer(
    ib_cfgparser_t *cp,
    const char *buffer,
    size_t length,
    bool more
) {
    assert(cp != NULL);
    assert(buffer != NULL);

    ib_status_t rc;

    if (length == 0) {
        return IB_OK;
    }

    rc = ib_cfgparser_ragel_parse_chunk(cp, buffer, length, (more ? 0 : 1) );

    if (!more) {
        /* Reset the parser. */
        cp->curr = cp->root;
    }

    return rc;
}

/* Forward declare because it is mutually recursive with
 * cfgparser_apply_node_children_helper. */
static ib_status_t cfgparser_apply_node_helper(
    ib_cfgparser_t *cp,
    ib_engine_t *ib,
    ib_cfgparser_node_t *node);

static ib_status_t cfgparser_apply_node_children_helper(
    ib_cfgparser_t *cp,
    ib_engine_t *ib,
    ib_cfgparser_node_t *node)
{
    ib_list_node_t *list_node;
    ib_status_t rc = IB_OK;

    IB_LIST_LOOP(node->children, list_node) {
        ib_cfgparser_node_t *child = ib_list_node_data(list_node);
        ib_status_t tmp_rc;

        tmp_rc = cfgparser_apply_node_helper(cp, ib, child);
        if (rc == IB_OK && tmp_rc != IB_OK) {
            rc = tmp_rc;
        }
    }

    return rc;
}

/**
 * Helper function to recursively apply the configuration nodes.
 *
 * This sets the @c curr member of @a cp to @a node.
 *
 * @returns
 * - IB_OK on success.
 * - Other on error. Failure is not fatal. If a recursive
 *   call fails, the first failure code is returned to the caller.
 */
static ib_status_t cfgparser_apply_node_helper(
    ib_cfgparser_t *cp,
    ib_engine_t *ib,
    ib_cfgparser_node_t *node)
{
    assert(cp != NULL);
    assert(ib != NULL);
    assert(node != NULL);

    ib_status_t rc = IB_OK;
    ib_status_t tmp_rc;
    ib_cfgparser_node_t *prev_curr;

    switch(node->type) {
        case IB_CFGPARSER_NODE_ROOT:
            tmp_rc = cfgparser_apply_node_children_helper(cp, ib, node);
            if (rc == IB_OK) {
                rc = tmp_rc;
            }
            break;
        case IB_CFGPARSER_NODE_PARSE_DIRECTIVE:
            tmp_rc = cfgparser_apply_node_children_helper(cp, ib, node);
            if (rc == IB_OK) {
                rc = tmp_rc;
            }
            break;
        case IB_CFGPARSER_NODE_DIRECTIVE:

            assert(
                (ib_list_elements(node->children) == 0) &&
                "Directives may not have children.");

            /* Store previous current node. */
            prev_curr = cp->curr;

            /* Set the current node before callbacks. */
            cp->curr = node;

            /* Callback to user directives. */
            tmp_rc = ib_config_directive_process(
                cp,
                node->directive,
                node->params);
            if (tmp_rc != IB_OK && rc == IB_OK) {
                rc = tmp_rc;
            }

            /* Restore current node. */
            cp->curr = prev_curr;

            break;
        case IB_CFGPARSER_NODE_BLOCK:
            /* Set the current node before callbacks. */
            cp->curr = node;

            /* Callback to user directives. */
            tmp_rc = ib_config_block_start(cp, node->directive, node->params);
            if (tmp_rc != IB_OK) {
                ib_cfg_log_error(cp,
                                 "Error starting block \"%s\": %s",
                                 node->directive, ib_status_to_string(tmp_rc));
                if (rc == IB_OK) {
                    rc = tmp_rc;
                }
            }

            tmp_rc = cfgparser_apply_node_children_helper(cp, ib, node);
            if (rc == IB_OK) {
                rc = tmp_rc;
            }

            /* Set the current node before callbacks. */
            cp->curr = node;

            /* Callback to user directives. */
            tmp_rc = ib_config_block_process(cp, node->directive);
            if (tmp_rc != IB_OK) {
                ib_cfg_log_error(cp,
                                 "Error processing block \"%s\": %s",
                                 node->directive,
                                 ib_status_to_string(tmp_rc));
                if (rc == IB_OK) {
                    rc = tmp_rc;
                }
            }
            break;

        case IB_CFGPARSER_NODE_FILE:
            ib_log_debug3(ib, "Applying file block \"%s\"", node->file);
            for (ib_cfgparser_node_t *tmp_node = node->parent;
                 tmp_node != NULL;
                 tmp_node = tmp_node->parent)
            {
                if (tmp_node->type == IB_CFGPARSER_NODE_ROOT) {
                    ib_log_debug3(
                        ib,
                        "    included from [root]:%zd",
                        tmp_node->line);
                }
                if (tmp_node->type == IB_CFGPARSER_NODE_FILE) {
                    ib_log_debug3(
                        ib,
                        "    included from %s:%zd",
                        tmp_node->file,
                        tmp_node->line);
                }
            }

            tmp_rc = cfgparser_apply_node_children_helper(cp, ib, node);
            if (rc == IB_OK) {
                rc = tmp_rc;
            }
            break;
    }

    return rc;
}

ib_status_t ib_cfgparser_apply(ib_cfgparser_t *cp, ib_engine_t *ib)
{
    assert(cp != NULL);
    assert(ib != NULL);
    assert(cp->curr != NULL);
    assert(cp->root != NULL);

    ib_status_t rc;

    rc = ib_cfgparser_apply_node(cp, cp->root, ib);

    return rc;
}

ib_status_t ib_cfgparser_apply_node(
    ib_cfgparser_t *cp,
    ib_cfgparser_node_t *tree,
    ib_engine_t *ib)
{
    assert(cp != NULL);
    assert(ib != NULL);
    assert(cp->curr != NULL);
    assert(cp->root != NULL);

    return cfgparser_apply_node_helper(cp, ib, tree);
}

static void cfgp_set_current(ib_cfgparser_t *cp, ib_context_t *ctx)
{
    cp->cur_ctx = ctx;
    return;
}

ib_status_t ib_cfgparser_context_push(ib_cfgparser_t *cp,
                                      ib_context_t *ctx)
{
    assert(cp != NULL);
    assert(ctx != NULL);
    ib_status_t rc;

    rc = ib_list_push(cp->stack, ctx);
    if (rc != IB_OK) {
        return rc;
    }
    cfgp_set_current(cp, ctx);

    return IB_OK;
}

ib_status_t ib_cfgparser_context_pop(ib_cfgparser_t *cp,
                                     ib_context_t **pctx,
                                     ib_context_t **pcctx)
{
    assert(cp != NULL);
    ib_context_t *ctx;
    ib_status_t rc;

    if (pctx != NULL) {
        *pctx = NULL;
    }

    /* Remove the last item. */
    rc = ib_list_pop(cp->stack, &ctx);
    if (rc != IB_OK) {
        return rc;
    }

    if (pctx != NULL) {
        *pctx = ctx;
    }

    /* The last in the list is now the current. */
    ctx = (ib_context_t *)ib_list_node_data(ib_list_last(cp->stack));
    cfgp_set_current(cp, ctx);

    if (pcctx != NULL) {
        *pcctx = ctx;
    }

    return IB_OK;
}

ib_status_t ib_cfgparser_context_current(const ib_cfgparser_t *cp,
                                         ib_context_t **pctx)
{
    assert(cp != NULL);
    assert(pctx != NULL);

    *pctx = cp->cur_ctx;

    return IB_OK;
}


const char *ib_cfgparser_curr_file(const ib_cfgparser_t *cp)
{
    assert(cp != NULL);
    assert(cp->curr != NULL);
    assert(cp->curr->file != NULL);

    return cp->curr->file;
}

size_t ib_cfgparser_curr_line(const ib_cfgparser_t *cp)
{
    assert(cp != NULL);
    assert(cp->curr != NULL);

    return cp->curr->line;
}


ib_status_t ib_cfgparser_destroy(ib_cfgparser_t *cp)
{
    assert(cp != NULL);

    ib_engine_pool_destroy(cp->ib, cp->mp);

    return IB_OK;
}

ib_status_t ib_config_register_directives(ib_engine_t *ib,
                                          const ib_dirmap_init_t *init)
{
    assert(ib != NULL);
    assert(init != NULL);
    const ib_dirmap_init_t *rec = init;
    ib_status_t rc;

    while ((rec != NULL) && (rec->name != NULL)) {

        rc = ib_hash_get(ib->dirmap, NULL, rec->name);
        if (rc == IB_OK) {
            ib_log_error(ib, "Redefining directive %s.", rec->name);
            return IB_EOTHER;
        }
        else if (rc != IB_ENOENT) {
            ib_log_error(
                ib,
                "Error checking for redefinition of directive %s: %s",
                rec->name,
                ib_status_to_string(rc));
            return rc;
        }

        rc = ib_hash_set(ib->dirmap, rec->name, (void *)rec);
        if (rc != IB_OK) {
            return rc;
        }

        ++rec;
    }

    return IB_OK;
}

ib_status_t ib_config_register_directive(
    ib_engine_t              *ib,
    const char               *name,
    ib_dirtype_t              type,
    ib_void_fn_t              fn_config,
    ib_config_cb_blkend_fn_t  fn_blkend,
    void                     *cbdata_config,
    void                     *cbdata_blkend,
    ib_strval_t              *valmap
)
{
    ib_dirmap_init_t *rec;
    ib_status_t rc;

    rec = (ib_dirmap_init_t *)ib_mm_alloc(
        ib_engine_mm_config_get(ib), sizeof(*rec)
    );
    if (rec == NULL) {
        return IB_EALLOC;
    }
    rec->name = name;
    rec->type = type;
    rec->cb._init = fn_config;
    rec->fn_blkend = fn_blkend;
    rec->cbdata_cb = cbdata_config;
    rec->cbdata_blkend = cbdata_blkend;
    rec->valmap = valmap;

    rc = ib_hash_get(ib->dirmap, NULL, rec->name);
    if (rc == IB_OK) {
        ib_log_error(ib, "Redefining directive %s.", rec->name);
        return IB_EOTHER;
    }
    else if (rc != IB_ENOENT) {
        return rc;
    }

    rc = ib_hash_set(ib->dirmap, rec->name, (void *)rec);

    return rc;
}

ib_status_t ib_config_registered_directives(
    ib_engine_t *ib,
    ib_list_t   *list
)
{
    assert(ib != NULL);
    assert(list != NULL);

    return ib_hash_get_all(ib->dirmap, list);
}

ib_status_t ib_config_registered_transformations(
    ib_engine_t *ib,
    ib_list_t   *list
)
{
    assert(ib != NULL);
    assert(list != NULL);

    return ib_hash_get_all(ib->tfns, list);
}


ib_status_t ib_config_registered_operators(
    ib_engine_t *ib,
    ib_list_t   *list
)
{
    assert(ib != NULL);
    assert(list != NULL);

    return ib_hash_get_all(ib->operators, list);
}

ib_status_t ib_config_registered_stream_operators(
    ib_engine_t *ib,
    ib_list_t   *list
)
{
    assert(ib != NULL);
    assert(list != NULL);

    return ib_hash_get_all(ib->stream_operators, list);
}

ib_status_t ib_config_registered_actions(
    ib_engine_t *ib,
    ib_list_t   *list
)
{
    assert(ib != NULL);
    assert(list != NULL);

    return ib_hash_get_all(ib->actions, list);
}

/**
 * Log a debug message followed by the directive and its parameters.
 *
 * Note that quoting, spacing and &lt;, &gt; decoration is removed.
 *
 * @param[in] cp Configuration parser.
 * @param[in] name Name of the directive.
 * @param[in] args The list of arguments.
 * @param[in] msg The message that prefixes the printed directive.
 */
static ib_status_t print_directive(ib_cfgparser_t *cp,
                                   const char *name,
                                   ib_list_t *args,
                                   const char *msg)
{
    /* clang analyzer has a false positive due to a poor understanding of
     * the value of params_len. */
#ifndef __clang_analyzer__
    assert(cp != NULL);
    assert(cp->ib != NULL);
    assert(name != NULL);
    assert(args != NULL);

    int params_len = 1; /* At least a \0 char. */
    char *params;
    const ib_list_node_t *node;

    IB_LIST_LOOP_CONST(args, node) {
        /* "; p=" + val */
        params_len += 4 + strlen((const char *)ib_list_node_data_const(node));
    }

    params = malloc(params_len);
    if (params == NULL) {
        return IB_EALLOC;
    }

    params[0] = '\0';

    IB_LIST_LOOP_CONST(args, node) {
        strcat(params, "; p=");
        strcat(params, (const char *)ib_list_node_data_const(node));
    }

    ib_cfg_log_debug(cp, "%sname=%s%s", msg, name, params);

    free(params);
#endif
    return IB_OK;
}

ib_status_t ib_config_directive_process(ib_cfgparser_t *cp,
                                        const char *name,
                                        ib_list_t *args)
{
    assert(cp != NULL);
    assert(cp->ib != NULL);
    assert(name != NULL);
    assert(args != NULL);

    ib_engine_t *ib = cp->ib;
    ib_dirmap_init_t *rec;
    size_t nargs = ib_list_elements(args);
    const char *p1;
    const char *p2;
    ib_status_t rc;

    if (ib_logger_level_get(ib_engine_logger_get(ib)) >= IB_LOG_DEBUG) {
        rc = print_directive(cp, name, args, "Processing directive: ");
        if (rc != IB_OK) {
            return rc;
        }
    }

    rc = ib_hash_get(ib->dirmap, &rec, name);
    if (rc == IB_ENOENT) {
        ib_cfg_log_error(cp, "Directive \"%s\" not defined.", name);
        return rc;
    }
    if (rc != IB_OK) {
        ib_cfg_log_error(
            cp,
            "Error fetching directive definition for \"%s\": %s",
            name,
            ib_status_to_string(rc));
        return rc;
    }

    switch (rec->type) {
        case IB_DIRTYPE_ONOFF:
            if (nargs != 1) {
                ib_cfg_log_error(cp,
                                 "Directive \"%s\" "
                                 "takes one parameter, not %zd",
                                 name, nargs);
                rc = IB_EINVAL;
                break;
            }
            p1 = (const char *)ib_list_node_data_const(
                ib_list_first_const(args));
            if (   (strcasecmp("on", p1) == 0)
                || (strcasecmp("yes", p1) == 0)
                || (strcasecmp("true", p1) == 0))
            {
                rc = rec->cb.fn_onoff(cp, name, 1, rec->cbdata_cb);
            }
            else {
                rc = rec->cb.fn_onoff(cp, name, 0, rec->cbdata_cb);
            }
            break;
        case IB_DIRTYPE_PARAM1:
            if (nargs != 1) {
                ib_cfg_log_error(cp,
                                 "Directive \"%s\" "
                                 "takes one parameter, not %zd",
                                 name, nargs);
                rc = IB_EINVAL;
                break;
            }
            p1 = (const char *)ib_list_node_data_const(
                ib_list_first_const(args));
            rc = rec->cb.fn_param1(cp, name, p1, rec->cbdata_cb);
            break;
        case IB_DIRTYPE_PARAM2:
            if (nargs != 2) {
                ib_cfg_log_error(cp,
                                 "Directive \"%s\" "
                                 "takes two parameters, not %zd",
                                 name, nargs);
                rc = IB_EINVAL;
                break;
            }
            p1 = (const char *)ib_list_node_data_const(
                ib_list_first_const(args));
            p2 = (const char *)ib_list_node_data_const(
                ib_list_last_const(args));
            rc = rec->cb.fn_param2(cp, name, p1, p2, rec->cbdata_cb);
            break;
        case IB_DIRTYPE_LIST:
            rc = rec->cb.fn_list(cp, name, args, rec->cbdata_cb);
            break;
        case IB_DIRTYPE_OPFLAGS:
        {
            ib_flags_t  flags = 0;
            ib_flags_t  mask = 0;
            const char *error;

            rc = ib_flags_strlist(rec->valmap, args, &flags, &mask, &error);
            if (rc != IB_OK) {
                ib_cfg_log_error(cp, "Invalid %s option: \"%s\"", name, error);
            }

            rc = rec->cb.fn_opflags(cp, name, flags, mask, rec->cbdata_cb);
            break;
        }
        case IB_DIRTYPE_SBLK1:
            if (nargs != 1) {
                ib_cfg_log_error(cp,
                                 "Block Directive \"%s\" "
                                 "takes one parameter, not %zd",
                                 name, nargs);
                rc = IB_EINVAL;
                break;
            }
            p1 = (const char *)ib_list_node_data_const(
                ib_list_first_const(args));
            rc = rec->cb.fn_sblk1(cp, name, p1, rec->cbdata_cb);
            break;
    }

    if (rc != IB_OK) {
        ib_cfg_log_error(
            cp,
            "Error processing directive \"%s\": %s",
            name,
            ib_status_to_string(rc));
    }

    return rc;
}

ib_status_t ib_config_block_start(ib_cfgparser_t *cp,
                                  const char *name,
                                  ib_list_t *args)
{
    assert(cp != NULL);
    assert(name != NULL);

    return ib_config_directive_process(cp, name, args);
}

ib_status_t ib_config_block_process(ib_cfgparser_t *cp,
                                    const char *name)
{
    assert(cp != NULL);
    assert(name != NULL);

    ib_engine_t *ib = cp->ib;
    ib_dirmap_init_t *rec;
    ib_status_t rc;

    rc = ib_hash_get(ib->dirmap, &rec, name);
    if (rc != IB_OK) {
        return rc;
    }

    rc = IB_OK;
    switch (rec->type) {
    case IB_DIRTYPE_SBLK1:
        if (rec->fn_blkend != NULL) {
            rc = rec->fn_blkend(cp, name, rec->cbdata_blkend);
        }
        break;
    default:
        rc = IB_EINVAL;
    }

    return rc;
}

ib_status_t ib_config_strval_pair_lookup(const char *str,
                                         const ib_strval_t *map,
                                         ib_num_t *pval)
{
    assert(str != NULL);
    assert(map != NULL);
    assert(pval != NULL);

    ib_status_t rc;
    uint64_t    value;

    rc = ib_strval_lookup(map, str, &value);
    *pval = value;

    return rc;
}


void ib_cfg_log_f(ib_cfgparser_t *cp, ib_logger_level_t level,
                  const char *file, int line,
                  const char *fmt, ...)
{
    assert(cp != NULL);
    assert(fmt != NULL);

    va_list ap;
    va_start(ap, fmt);

    ib_cfg_vlog(cp, level, file, line, fmt, ap);

    va_end(ap);

    return;
}

void ib_cfg_log_ex_f(const ib_engine_t *ib,
                     const char *cfgfile, unsigned int cfgline,
                     ib_logger_level_t level,
                     const char *file, int line,
                     const char *fmt, ...)
{
    assert(ib != NULL);
    assert(fmt != NULL);

    va_list ap;
    va_start(ap, fmt);

    ib_cfg_vlog_ex(ib, cfgfile, cfgline, level, file, line, fmt, ap);

    va_end(ap);

    return;
}

void ib_cfg_vlog_ex(const ib_engine_t *ib,
                    const char *cfgfile, unsigned int cfgline,
                    ib_logger_level_t level,
                    const char *file, int line,
                    const char *fmt, va_list ap)
{
    assert(ib != NULL);
    assert(fmt != NULL);

    static const size_t MAX_LNOBUF = 16;
    char lnobuf[MAX_LNOBUF+1];
    const char *which_fmt = NULL;

    static const char *c_prefix = "CONFIG";

    const size_t new_fmt_len =
        strlen(c_prefix) +
        strlen(fmt) +
        (cfgfile != NULL ? strlen(cfgfile) : 0) +
        30;
    char *new_fmt = (char *)malloc(new_fmt_len);

    if (new_fmt != NULL) {
        snprintf(new_fmt, new_fmt_len, "%s %s", c_prefix, fmt);

        if (cfgfile != NULL) {
            strcat(new_fmt, " @ ");
            strcat(new_fmt, cfgfile);
            strcat(new_fmt, ":");
            snprintf(lnobuf, MAX_LNOBUF, "%u", cfgline);
            strcat(new_fmt, lnobuf);
        }

        which_fmt = new_fmt;
    }
    else {
        which_fmt = fmt;
    }

    ib_log_vex_ex(ib, level, file, __func__, line, which_fmt, ap);

    if (new_fmt != NULL) {
        free(new_fmt);
    }

    return;
}

void ib_cfg_vlog(ib_cfgparser_t *cp, ib_logger_level_t level,
                 const char *file, int line,
                 const char *fmt, va_list ap)
{
    assert(cp != NULL);
    assert(fmt != NULL);

    ib_cfg_vlog_ex(cp->ib, cp->curr->file, cp->curr->line,
                   level, file, line, fmt, ap);

    return;
}

ib_status_t ib_cfg_parse_tfn(
    const char *str,
    const char **name,
    const char **name_end,
    const char **arg,
    const char **arg_end
)
{
    assert(str != NULL);
    assert(name != NULL);
    assert(name_end != NULL);
    assert(arg != NULL);
    assert(arg_end != NULL);

    size_t s;

    *name = str;

    s = strcspn(str, "(");
    if (s == 0 || str[s] != '(') {
        return IB_EINVAL;
    }
    else {
        *name_end = *name + s;
    }

    *arg = *name_end + 1;

    s = strcspn(*arg, ")");
    if ((*arg)[s] != ')') {
        return IB_EINVAL;
    }
    else {
        *arg_end = *arg + s;
    }

    return IB_OK;
}

/**
 * Return the character just past the var target string, @a str.
 *
 * The characters before this are part of the target.
 * The characters after this, if any, make up the transformations to
 * that target.
 *
 * @param[in] str The string to parse.
 *
 * @returns The character past end of the target portion of @a str.
 */
static const char * cfg_find_end_of_target(const char *str) {
    const char *cur = str;
    ib_status_t rc;
    const char *name;
    const char *name_end;
    const char *arg;
    const char *arg_end;

    /* Parse out the source component. */
    for (; ; ++cur) {

        /* On ':' we have the source part of the target.
         * Jump to the next parsing. */
        if (*cur == ':') {
            break;
        }

        if (*cur == '\0') {
            return cur;
        }

        /* Special check for '.'.
         * If '.' means that there is a transformation, we are done. */
        if (*cur == '.') {

            rc = ib_cfg_parse_tfn(cur+1, &name, &name_end, &arg, &arg_end);

            /* If we find a '.' that signals a transformation,
             * the target is done. Return. */
            if (rc == IB_OK) {
                return cur;
            }
        }
    }

    /* Hop over ':'. */
    ++cur;

    /* Scan over to the ending '/'. */
    if (*cur == '/') {
        for (cur = cur + 1; *cur != '/'; ++cur) {
            /* Skip over escaped strings in regexes. */
            if (*cur == '\\' && *(cur + 1) != '\0') {
                ++cur;
            }
        }
        /* Hop over ending '/'. */
        if (*cur == '/') {
            ++cur;
        }

        /* At the close of the filter, we are done. */
        return cur;
    }

    for (; ; ++cur) {
        if (*cur == '\0') {
            return cur;
        }

        if (*cur == '.') {
            rc = ib_cfg_parse_tfn(cur+1, &name, &name_end, &arg, &arg_end);
            /* If we find a '.' that signals a transformation,
             * the target is done. Return. */
            if (rc == IB_OK) {
                return cur;
            }
        }
    }
}

ib_status_t ib_cfg_parse_target_string(
    ib_mm_t      mm,
    const char  *str,
    const char **target,
    ib_list_t   *tfns
)
{
    assert(str != NULL);
    assert(target != NULL);
    assert(tfns != NULL);

    ib_status_t  rc;
    char        *dup_str;            /* Duplicate string */
    const char  *end_of_target;

    end_of_target = cfg_find_end_of_target(str);
    if (end_of_target == NULL) {
        /* Error finding the end of the target. */
        return IB_EINVAL;
    }
    /* If the end of the target is the end of the string, we are done. */
    else if (*end_of_target == '\0') {
        *target = str;
        return IB_OK;
    }

    /* If the end_of_target is not also the end of the string, then we
     * must modify str to parse out transformations. Make a copy of str
     * into dup_str and parse. */

    dup_str = ib_mm_strdup(mm, str);
    if (dup_str == NULL) {
        return IB_EALLOC;
    }

    /* Compute the offset and set to '\0'. */
    dup_str[end_of_target - str] = '\0';
    /* Set target to the newly-terminated target string. */
    *target = dup_str;

    /* Walk through the string and parse-out transformations. */
    for (const char *cur = dup_str + (end_of_target - str)+1; cur[0] != '\0'; ) {
        const char *name;
        const char *name_end;
        const char *arg;
        const char *arg_end;
        ib_field_t *tfn_field;               /* Transformation to push. */

        rc = ib_cfg_parse_tfn(cur, &name, &name_end, &arg, &arg_end);
        if (rc != IB_OK) {
            return rc;
        }

        *(char *)name_end = '\0';
        *(char *)arg_end = '\0';

       rc = ib_field_create_alias(
            &tfn_field,
            mm,
            name, (name_end - name),
            IB_FTYPE_NULSTR,
            ib_ftype_nulstr_in(arg));
        if (rc != IB_OK) {
            /* Failed to create transformation field. */
            return rc;
        }

        rc = ib_list_push(tfns, tfn_field);
        if (rc != IB_OK) {
            /* Failed to push transformation. */
            return rc;
        }

        /* Skip over trailing ')' and trailing '.'. */
        cur = arg_end+2;
    }

    /**
     * The field name is the start of the duplicate string, even after
     * it's been chopped up into pieces.
     */
    *target = dup_str;

    return IB_OK;
}
