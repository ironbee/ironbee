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
 * @brief IronBee - Configuration
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>


#include <ironbee/engine.h>
#include <ironbee/util.h>
#include <ironbee/config.h>

#include "config-parser.h"
#include "ironbee_private.h"

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

static void cfgp_dump(ib_cfgparser_t *cp)
{
    /// @todo Implement
    ib_log_debug(cp->ib, 4, "Config: %p (TODO)", cp);
}


/* -- Configuration Parser Routines -- */

ib_status_t ib_cfgparser_create(ib_cfgparser_t **pcp,
                                ib_engine_t *ib)
{
    IB_FTRACE_INIT(ib_cfgparser_create);
    ib_mpool_t *pool;
    ib_status_t rc;

    /* Create parser memory pool */
    rc = ib_mpool_create(&pool, ib->mp);
    if (rc != IB_OK) {
        rc = IB_EALLOC;
        goto failed;
    }

    /* Create the main structure in the memory pool */
    *pcp = (ib_cfgparser_t *)ib_mpool_calloc(pool, 1, sizeof(**pcp));
    if (*pcp == NULL) {
        rc = IB_EALLOC;
        goto failed;
    }
    (*pcp)->ib = ib;
    (*pcp)->mp = pool;

    /* Create the stack */
    rc = ib_list_create(&((*pcp)->stack), pool);
    if (rc != IB_OK) {
        goto failed;
    }
    (*pcp)->cur_ctx = ib_context_main(ib);
    ib_list_push((*pcp)->stack, (*pcp)->cur_ctx);

    /* Create the block tracking list */
    rc = ib_list_create(&((*pcp)->block), pool);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Other fields are NULLed via calloc */

    ib_log_debug(ib, 4, "Stack: ctx=%p site=%p(%s) loc=%p",
                 (*pcp)->cur_ctx,
                 (*pcp)->cur_site, (*pcp)->cur_site?(*pcp)->cur_site->name:"NONE",
                 (*pcp)->cur_loc);

    IB_FTRACE_RET_STATUS(rc);

failed:
    /* Make sure everything is cleaned up on failure */
    if (pool != NULL) {
        ib_mpool_destroy(pool);
    }
    *pcp = NULL;

    IB_FTRACE_RET_STATUS(rc);
}

/// @todo Create a ib_cfgparser_parse_ex that can parse non-files (DBs, etc)

ib_status_t ib_cfgparser_parse(ib_cfgparser_t *cp,
                               const char *file)
{
    IB_FTRACE_INIT(ib_cfgparser_parse);
    int fd = open(file, O_RDONLY);
    uint8_t buf[8192];
    uint8_t *buf_end = buf + sizeof(buf);
    uint8_t *buf_mark = buf;
    ssize_t nbytes;
    ib_status_t rc = IB_OK;

    if (fd == -1) {
        int ec = errno;
        ib_log_error(cp->ib, 1, "Could not open config file \"%s\": (%d) %s",
                     file, ec, strerror(ec));
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

#define bufremain (sizeof(buf) - (buf_mark - buf) + 1)

    ib_log_debug(cp->ib, 4, "Reading config \"%s\" via fd=%d", file, fd);
    while ((nbytes = read(fd, buf_mark, bufremain))) {
        int ec = errno;
        size_t remaining;
        uint8_t *chunk_start;
        uint8_t *chunk_end;

        ib_log_debug(cp->ib, 9, "Read a %d byte chunk", (int)nbytes);
        if (nbytes == -1) {
            if (ec == EAGAIN) {
                continue;
            }
            else {
                break;
            }
        }

        /* Move the buf_mark to where the data ends in buf. */
        buf_mark += nbytes;

        /* Process all lines of data in buf. */
        chunk_start = buf;
        chunk_end = buf;
        while (chunk_end < buf_mark) {
            /* Do not go too far. */
            if (chunk_end >= buf_end) {
                ib_log_error(cp->ib, 1, "Error parsing \"%s\": Line >%d bytes",
                             file, (int)sizeof(buf));
                IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
            }

            /* Process a line of input. */
            if (*chunk_end == '\n') {
                size_t chunk_len = (chunk_end - chunk_start) + 1;

                /// @todo Make the parser type configurable
                ib_log_debug(cp->ib, 9, "Parsing %d byte chunk", (int)chunk_len);
                rc = ib_cfgparser_ragel_parse_chunk(cp, chunk_start, chunk_len);
                if (rc != IB_OK) {
                    ib_log_error(cp->ib, 1, "Error parsing config file: %d", rc);
                    IB_FTRACE_RET_STATUS(rc);
                }

                /* Move chunk markers to potential next line in buf. */
                chunk_end++;
                chunk_start = chunk_end;
            }

            chunk_end++;
        }
        ib_log_debug(cp->ib, 4, "buf=%p buf_mark=%p buf_end=%p",
                     buf, buf_mark, buf_end);
        ib_log_debug(cp->ib, 4, "chunk_start=%p chunk_end=%p",
                     chunk_start, chunk_end);

        /* Move remaining data to beginning of buf so that more can be read. */
        remaining = (buf_mark - chunk_start);
        if (remaining) {
            ib_log_debug(cp->ib, 4, "Moving %d bytes", (int)remaining);
            memmove(buf, chunk_end, remaining);
            buf_mark = buf + remaining;
        }
    }
    ib_log_debug(cp->ib, 4, "Done reading config \"%s\" via fd=%d errno=%d", file, fd, errno);

    cfgp_dump(cp);
    IB_FTRACE_RET_STATUS(rc);
}

static void cfgp_set_current(ib_cfgparser_t *cp, ib_context_t *ctx)
{
    IB_FTRACE_INIT(cfgp_set_current);
    cp->cur_ctx = ctx;
    cp->cur_loc = (ib_loc_t *)ctx->fn_ctx_data;
    cp->cur_site = cp->cur_loc?cp->cur_loc->site:NULL;
    if (cp->cur_site != NULL) {
        if (cp->cur_site->locations != NULL) {
            cp->cur_loc = (ib_loc_t *)ib_list_node_data(ib_list_last(cp->cur_site->locations));
        }
        else {
            cp->cur_loc = cp->cur_site->default_loc;
        }
    }
    IB_FTRACE_RET_VOID();
}

ib_status_t ib_cfgparser_context_push(ib_cfgparser_t *cp,
                                      ib_context_t *ctx)
{
    IB_FTRACE_INIT(ib_cfgparser_context_push);
    ib_engine_t *ib = cp->ib;
    ib_status_t rc;

    rc = ib_list_push(cp->stack, ctx);
    if (rc != IB_OK) {
        ib_log_debug(ib, 4, "Failed to push context %p: %d", ctx, rc);
        IB_FTRACE_RET_STATUS(rc);
    }
    cfgp_set_current(cp, ctx);

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_cfgparser_context_pop(ib_cfgparser_t *cp,
                                     ib_context_t **pctx)
{
    IB_FTRACE_INIT(ib_cfgparser_context_pop);
    ib_engine_t *ib = cp->ib;
    ib_context_t *ctx;
    ib_status_t rc;

    if (pctx != NULL) {
        *pctx = NULL;
    }

    /* Remove the last item. */
    rc = ib_list_pop(cp->stack, &ctx);
    if (rc != IB_OK) {
        ib_log_debug(ib, 4, "Failed to pop context: %d", rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    if (pctx != NULL) {
        *pctx = ctx;
    }

    /* The last in the list is now the current. */
    ctx = (ib_context_t *)ib_list_node_data(ib_list_last(cp->stack));
    cfgp_set_current(cp, ctx);

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t DLL_PUBLIC ib_cfgparser_block_push(ib_cfgparser_t *cp,
                                               const char *name)
{
    IB_FTRACE_INIT(ib_cfgparser_block_push);
    ib_engine_t *ib = cp->ib;
    ib_status_t rc;

    rc = ib_list_push(cp->block, (void *)name);
    if (rc != IB_OK) {
        ib_log_debug(ib, 4, "Failed to push block %p: %d", name, rc);
        IB_FTRACE_RET_STATUS(rc);
    }
    cp->cur_blkname = name;

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t DLL_PUBLIC ib_cfgparser_block_pop(ib_cfgparser_t *cp,
                                              const char **pname)
{
    IB_FTRACE_INIT(ib_cfgparser_block_pop);
    ib_engine_t *ib = cp->ib;
    const char *name;
    ib_status_t rc;

    if (pname != NULL) {
        *pname = NULL;
    }

    rc = ib_list_pop(cp->block, &name);
    if (rc != IB_OK) {
        ib_log_debug(ib, 4, "Failed to pop block: %d", rc);
        cp->cur_blkname = NULL;
        IB_FTRACE_RET_STATUS(rc);
    }

    if (pname != NULL) {
        *pname = name;
    }

    /* The last in the list is now the current. */
    cp->cur_blkname = (const char *)ib_list_node_data(ib_list_last(cp->block));

    IB_FTRACE_RET_STATUS(IB_OK);
}

void ib_cfgparser_destroy(ib_cfgparser_t *cp)
{
    IB_FTRACE_INIT(ib_cfgparser_destroy);

    if (cp != NULL) {
        ib_mpool_destroy(cp->mp);
    }
    IB_FTRACE_RET_VOID();
}

ib_status_t ib_config_register_directives(ib_engine_t *ib,
                                          const ib_dirmap_init_t *init)
{
    IB_FTRACE_INIT(ib_conf_register_directives);
    const ib_dirmap_init_t *rec = init;
    ib_status_t rc;

    while ((rec != NULL) && (rec->name != NULL)) {
        ib_log_debug(ib, 4, "DIRECTIVE: %s type=%d", rec->name, rec->type);

        rc = ib_hash_set(ib->dirmap, rec->name, (void *)rec);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }

        rec++;
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_config_register_directive(ib_engine_t *ib,
                                         const char *name,
                                         ib_dirtype_t type,
                                         ib_void_fn_t fn_config,
                                         ib_config_cb_blkend_fn_t fn_blkend,
                                         void *cbdata)
{
    IB_FTRACE_INIT(ib_conf_register_directive);
    ib_dirmap_init_t *rec;
    ib_status_t rc;

    rec = (ib_dirmap_init_t *)ib_mpool_alloc(ib->config_mp, sizeof(*rec));
    if (rec == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    rec->name = name;
    rec->type = type;
    rec->cb._init = fn_config;
    rec->fn_blkend = fn_blkend;
    rec->cbdata = cbdata;

    rc = ib_hash_set(ib->dirmap, rec->name, (void *)rec);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_config_directive_process(ib_cfgparser_t *cp,
                                        const char *name,
                                        ib_list_t *args)
{
    IB_FTRACE_INIT(ib_config_directive_process);
    ib_engine_t *ib = cp->ib;
    ib_dirmap_init_t *rec;
    size_t nargs = ib_list_elements(args);
    const char *p1;
    const char *p2;
    ib_status_t rc;

    rc = ib_hash_get(ib->dirmap, name, (void *)&rec);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    switch (rec->type) {
        case IB_DIRTYPE_ONOFF:
            if (nargs != 1) {
                ib_log_error(ib, 1, "OnOff directive \"%s\" takes one parameter, not %d",
                             name, nargs);
                rc = IB_EINVAL;
                break;
            }
            ib_list_shift(args, &p1);
            if (   (strcasecmp("on", p1) == 0)
                || (strcasecmp("yes", p1) == 0)
                || (strcasecmp("true", p1) == 0))
            {
                rc = rec->cb.fn_onoff(cp, name, 1, rec->cbdata);
            }
            else {
                rc = rec->cb.fn_onoff(cp, name, 0, rec->cbdata);
            }
            break;
        case IB_DIRTYPE_PARAM1:
            if (nargs != 1) {
                ib_log_error(ib, 1, "Param1 directive \"%s\" takes one parameter, not %d",
                             name, nargs);
                rc = IB_EINVAL;
                break;
            }
            ib_list_shift(args, &p1);
            rc = rec->cb.fn_param1(cp, name, p1, rec->cbdata);
            break;
        case IB_DIRTYPE_PARAM2:
            if (nargs != 2) {
                ib_log_error(ib, 1, "Param2 directive \"%s\" takes two parameters, not %d",
                             name, nargs);
                rc = IB_EINVAL;
                break;
            }
            ib_list_shift(args, &p1);
            ib_list_shift(args, &p2);
            rc = rec->cb.fn_param2(cp, name, p1, p2, rec->cbdata);
            break;
        case IB_DIRTYPE_LIST:
            rc = rec->cb.fn_list(cp, name, args, rec->cbdata);
            break;
        case IB_DIRTYPE_SBLK1:
            if (nargs != 1) {
                ib_log_error(ib, 1, "SBlk1 directive \"%s\" takes one parameter, not %d",
                             name, nargs);
                rc = IB_EINVAL;
                break;
            }
            ib_list_shift(args, &p1);
            rc = rec->cb.fn_sblk1(cp, name, p1, rec->cbdata);
            break;
        default:
            rc = IB_EINVAL;
    }

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_config_block_start(ib_cfgparser_t *cp,
                                  const char *name,
                                  ib_list_t *args)
{
    ib_status_t rc = ib_cfgparser_block_push(cp, name);
    if (rc != IB_OK) {
        return rc;
    }
    return ib_config_directive_process(cp, name, args);
}

ib_status_t ib_config_block_process(ib_cfgparser_t *cp,
                                    const char *name)
{
    IB_FTRACE_INIT(ib_config_block_process);
    ib_engine_t *ib = cp->ib;
    ib_dirmap_init_t *rec;
    ib_status_t rc;

    /* Finished with this block. */
    rc = ib_cfgparser_block_pop(cp, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_hash_get(ib->dirmap, name, (void *)&rec);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = IB_OK;
    switch (rec->type) {
        case IB_DIRTYPE_SBLK1:
            if (rec->fn_blkend != NULL) {
                rc = rec->fn_blkend(cp, name, rec->cbdata);
            }
            break;
        default:
            rc = IB_EINVAL;
    }

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_site_create(ib_site_t **psite,
                                      ib_engine_t *ib,
                                      const char *name)
{
    IB_FTRACE_INIT(ib_site_create);
    ib_mpool_t *pool = ib->config_mp;
    ib_status_t rc;

    /* Create the main structure in the config memory pool */
    *psite = (ib_site_t *)ib_mpool_calloc(pool, 1, sizeof(**psite));
    if (*psite == NULL) {
        rc = IB_EALLOC;
        IB_FTRACE_RET_STATUS(rc);
    }
    (*psite)->ib = ib;
    (*psite)->mp = pool;
    (*psite)->name = name; /// @todo Copy???

    /* Remaining fields are NULL via calloc. */

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_site_address_add(ib_site_t *site,
                                const char *ip)
{
    IB_FTRACE_INIT(ib_site_address_add);
    ib_status_t rc;

    /* Create a list if this is the first item. */
    if (site->ips == NULL) {
        rc = ib_list_create(&site->ips, site->mp);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /// @todo: use regex
    rc = ib_list_push(site->ips, (void *)ip);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_site_address_validate(ib_site_t *site,
                                     const char *ip)
{
    IB_FTRACE_INIT(ib_site_address_validate);
    IB_FTRACE_RET_STATUS(IB_ENOTIMPL);
}

ib_status_t ib_site_hostname_add(ib_site_t *site,
                                 const char *host)
{
    IB_FTRACE_INIT(ib_site_hostname_add);
    ib_status_t rc;

    /* Create a list if this is the first item. */
    if (site->hosts == NULL) {
        rc = ib_list_create(&site->hosts, site->mp);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /// @todo: use regex
    rc = ib_list_push(site->hosts, (void *)host);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_site_hostname_validate(ib_site_t *site,
                                      const char *host)
{
    IB_FTRACE_INIT(ib_site_hostname_validate);
    IB_FTRACE_RET_STATUS(IB_ENOTIMPL);
}

ib_status_t ib_site_loc_create(ib_site_t *site,
                               ib_loc_t **ploc,
                               const char *path)
{
    IB_FTRACE_INIT(ib_site_loc_create);
    ib_loc_t *loc;
    ib_status_t rc;

    if (ploc != NULL) {
        *ploc = NULL;
    }

    /* Create a list if this is the first item. */
    if (site->locations == NULL) {
        rc = ib_list_create(&site->locations, site->mp);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Create the location structure in the site memory pool */
    loc = (ib_loc_t *)ib_mpool_calloc(site->mp, 1, sizeof(*loc));
    if (loc == NULL) {
        rc = IB_EALLOC;
        IB_FTRACE_RET_STATUS(rc);
    }
    loc->site = site;
    loc->path = path;

    if (ploc != NULL) {
        *ploc = loc;
    }

    rc = ib_list_push(site->locations, (void *)loc);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_site_loc_create_default(ib_site_t *site,
                                       ib_loc_t **ploc)
{
    IB_FTRACE_INIT(ib_site_loc_create);
    ib_loc_t *loc;
    ib_status_t rc;

    if (ploc != NULL) {
        *ploc = NULL;
    }

    /* Create the location structure in the site memory pool */
    loc = (ib_loc_t *)ib_mpool_calloc(site->mp, 1, sizeof(*loc));
    if (loc == NULL) {
        rc = IB_EALLOC;
        IB_FTRACE_RET_STATUS(rc);
    }
    loc->site = site;
    loc->path = "/";

    if (ploc != NULL) {
        *ploc = loc;
    }

    site->default_loc = loc;
    IB_FTRACE_RET_STATUS(IB_OK);
}

