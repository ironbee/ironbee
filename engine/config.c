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
 * @brief IronBee &mdash; Configuration
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/config.h>

#include <ironbee/debug.h>
#include <ironbee/mpool.h>

#include "config-parser.h"
#include "engine_private.h"

#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <inttypes.h>

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

/* Get an option value from a name/mapping. */
static ib_status_t cfgp_opval(const char *opname, const ib_strval_t *map,
                              ib_num_t *pval)
{
    ib_strval_t *rec = (ib_strval_t *)map;

    while (rec->str != NULL) {
        if (strcasecmp(opname, rec->str) == 0) {
            *pval = rec->val;
            return IB_OK;
        }
        rec++;
    }

    *pval = 0;

    return IB_EINVAL;
}



/* -- Configuration Parser Routines -- */

ib_status_t ib_cfgparser_create(ib_cfgparser_t **pcp,
                                ib_engine_t *ib)
{
    IB_FTRACE_INIT();
    ib_mpool_t *pool;
    ib_status_t rc;

    /* Create parser memory pool */
    rc = ib_mpool_create(&pool, "cfgparser", ib->mp);
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

    /* Create the include tracking list */
    rc = ib_hash_create(&((*pcp)->includes), pool);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Other fields are NULLed via calloc */

    ib_log_debug3(ib, "Stack: ctx=%p(%s) site=%p(%s) loc=%p(%s)",
                  (*pcp)->cur_ctx, ib_context_full_get((*pcp)->cur_ctx),
                  (*pcp)->cur_site, (*pcp)->cur_site?(*pcp)->cur_site->name:"NONE",
                  (*pcp)->cur_loc, (*pcp)->cur_loc?(*pcp)->cur_loc->path:"/");

    IB_FTRACE_RET_STATUS(rc);

failed:
    /* Make sure everything is cleaned up on failure */
    ib_engine_pool_destroy(ib, pool);
    *pcp = NULL;

    IB_FTRACE_RET_STATUS(rc);
}

/// @todo Create a ib_cfgparser_parse_ex that can parse non-files (DBs, etc)

ib_status_t ib_cfgparser_parse(ib_cfgparser_t *cp,
                               const char *file)
{
    IB_FTRACE_INIT();
    int ec;                                    /**< Error code for sys calls. */
    int fd = open(file, O_RDONLY);             /**< File to read. */
    unsigned lineno = 1;                       /**< Current line number */
    ssize_t nbytes = 0;                        /**< Bytes read by one read(). */
    const size_t bufsz = 8192;                 /**< Buffer size. */
    size_t buflen = 0;                         /**< Last char in buffer. */
    char *buf = NULL;                          /**< Buffer. */
    char *eol = 0;                             /**< buf[eol] = end of line. */
    char *bol = 0;                             /**< buf[bol] = begin line. */

    ib_status_t rc = IB_OK;
    unsigned error_count = 0;
    ib_status_t error_rc = IB_OK;

    if (fd == -1) {
        ec = errno;
        ib_log_error(cp->ib,  "Could not open config file \"%s\": (%d) %s",
                     file, ec, strerror(ec));
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    buf = (char *)malloc(sizeof(*buf)*bufsz);

    if (buf==NULL) {
        ib_log_error(cp->ib,
            "Unable to allocate buffer for configuration file.");
        close(fd);
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Fill the buffer, parse each line. Conditionally read another line. */
    do {
        nbytes = read(fd, buf+buflen, bufsz-buflen);
        buflen += nbytes;
        ib_log_debug3(cp->ib,
                      "Read a %zd byte chunk. Total len=%zd",
                      nbytes, buflen);

        if ( nbytes == 0 ) { /* EOF */
            rc = ib_cfgparser_parse_buffer(
                cp, buf, nbytes, file, lineno, true);
            ++lineno;
            if (rc != IB_OK) {
                ++error_count;
                error_rc = rc;
            }

            break;
        }
        else if ( nbytes > 0 ) { /* Normal. */

            /* The first line always begins at buf[0]. */
            bol = buf;
            eol = (char *)memchr(bol, '\n', buflen);

            /* Check that we found at least 1 end-of-line in this file. */
            if (eol == NULL) {
                if (buflen<bufsz) {
                    /* There is no end-of-line (\n) character and
                     * there is more to read.
                     * Kick back out to while loop. */
                    continue;
                }
                else {
                    /* There is no end of line and there is no more
                     * space in the buffer. This is an error. */
                    ib_log_error(cp->ib,
                                 "Unable to read a configuration line "
                                 "larger than %zd bytes from file %s. "
                                 "Parsing has failed.",
                                 buflen, file);
                    free(buf);
                    close(fd);
                    IB_FTRACE_RET_STATUS(IB_EINVAL);
                }
            }
            else {
                /* We have found at least one end-of-line character.
                 * Iterate through it and all others, passing each line to
                 * ib_cfgparser_parse_buffer */
                do {
                    rc = ib_cfgparser_parse_buffer(
                        cp, bol, eol-bol+1, file, lineno, false);
                    ++lineno;
                    if (rc != IB_OK) {
                        ++error_count;
                        error_rc = rc;
                    }
                    bol = eol+1;
                    eol = (char *)memchr(bol, '\n', buf+buflen-bol);
                } while (eol != NULL);

                /* There are no more end-of-line opportunities.
                 * Now move the last end-of-line to the beginning. */
                ib_log_debug2(cp->ib,
                              "Buffer of length %zd must be shrunk.",
                              buflen);
                ib_log_debug2(cp->ib,
                              "Beginning of last line is at index %zd.",
                              bol-buf);
                buflen = buf + buflen - bol;
                if (buflen > 0) {
                    ib_log_debug2(cp->ib,
                                 "Discarding parsed lines."
                                 " Moving %p to %p with length %zd.",
                                 bol, buf, buflen);
                    memmove(buf, bol, buflen);
                }
            }
        }
        else {
            /* nbytes < 0. This is an error. */
            ib_log_error(cp->ib,
                "Error reading log file %s - %s.", file, strerror(errno));
            free(buf);
            close(fd);
            IB_FTRACE_RET_STATUS(IB_ETRUNC);
        }
    } while (nbytes > 0);

    free(buf);
    close(fd);
    ib_log_debug3(cp->ib,
                  "Done reading config \"%s\" via fd=%d errno=%d",
                  file, fd, errno);
    if ( (error_count == 0) && (rc == IB_OK) ) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    else if (rc == IB_OK) {
        rc = error_rc;
    }
    ib_log_error(cp->ib,
                 "%u Error(s) parsing config file: %s",
                 error_count, ib_status_to_string(rc));
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_cfgparser_parse_buffer(ib_cfgparser_t *cp,
                                      const char     *buffer,
                                      size_t          length,
                                      const char     *file,
                                      unsigned        lineno,
                                      bool       more)
{
    IB_FTRACE_INIT();

    cp->cur_file = file;
    cp->cur_lineno = lineno;
    IB_FTRACE_RET_STATUS(
        ib_cfgparser_ragel_parse_chunk(
            cp,
            buffer,
            length,
            file,
            lineno,
            (more == true ? 1 : 0)
        )
    );
}

static void cfgp_set_current(ib_cfgparser_t *cp, ib_context_t *ctx)
{
    IB_FTRACE_INIT();
    cp->cur_ctx = ctx;
    cp->cur_loc = (ib_loc_t *)ctx->fn_ctx_data;
    cp->cur_site = cp->cur_loc?cp->cur_loc->site:NULL;
    IB_FTRACE_RET_VOID();
}

ib_status_t ib_cfgparser_context_push(ib_cfgparser_t *cp,
                                      ib_context_t *ctx)
{
    IB_FTRACE_INIT();
    ib_engine_t *ib = cp->ib;
    ib_status_t rc;

    rc = ib_list_push(cp->stack, ctx);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to push context %p(%s): %s",
                     ctx, ib_context_full_get(ctx), ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }
    cfgp_set_current(cp, ctx);

    ib_log_debug3(ib, "Stack: ctx=%p(%s) site=%p(%s) loc=%p(%s)",
                  cp->cur_ctx, ib_context_full_get(cp->cur_ctx),
                  cp->cur_site, cp->cur_site?cp->cur_site->name:"NONE",
                  cp->cur_loc, cp->cur_loc?cp->cur_loc->path:"/");

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_cfgparser_context_pop(ib_cfgparser_t *cp,
                                     ib_context_t **pctx)
{
    IB_FTRACE_INIT();
    ib_context_t *ctx;
    ib_status_t rc;

    if (pctx != NULL) {
        *pctx = NULL;
    }

    /* Remove the last item. */
    rc = ib_list_pop(cp->stack, &ctx);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp,
                         "Failed to pop context: %s",
                         ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    if (pctx != NULL) {
        *pctx = ctx;
    }

    /* The last in the list is now the current. */
    ctx = (ib_context_t *)ib_list_node_data(ib_list_last(cp->stack));
    cfgp_set_current(cp, ctx);

    ib_cfg_log_debug3(cp, "Stack: ctx=%p(%s) site=%p(%s) loc=%p(%s)",
                      cp->cur_ctx, ib_context_full_get(cp->cur_ctx),
                      cp->cur_site, cp->cur_site?cp->cur_site->name:"NONE",
                      cp->cur_loc, cp->cur_loc?cp->cur_loc->path:"/");

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t DLL_PUBLIC ib_cfgparser_block_push(ib_cfgparser_t *cp,
                                               const char *name)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    rc = ib_list_push(cp->block, (void *)name);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp,
                         "Failed to push block %p: %s",
                         name, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }
    cp->cur_blkname = name;

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t DLL_PUBLIC ib_cfgparser_block_pop(ib_cfgparser_t *cp,
                                              const char **pname)
{
    IB_FTRACE_INIT();
    const char *name;
    ib_status_t rc;

    if (pname != NULL) {
        *pname = NULL;
    }

    rc = ib_list_pop(cp->block, &name);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp,
                         "Failed to pop block: %s",
                         ib_status_to_string(rc));
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
    IB_FTRACE_INIT();

    if (cp != NULL) {
        ib_engine_pool_destroy(cp->ib, cp->mp);
    }

    IB_FTRACE_RET_VOID();
}

ib_status_t ib_config_register_directives(ib_engine_t *ib,
                                          const ib_dirmap_init_t *init)
{
    IB_FTRACE_INIT();
    const ib_dirmap_init_t *rec = init;
    ib_status_t rc;

    while ((rec != NULL) && (rec->name != NULL)) {
        rc = ib_hash_set(ib->dirmap, rec->name, (void *)rec);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }

        rec++;
    }

    IB_FTRACE_RET_STATUS(IB_OK);
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
    IB_FTRACE_INIT();
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
    rec->cbdata_cb = cbdata_config;
    rec->cbdata_blkend = cbdata_blkend;
    rec->valmap = valmap;

    rc = ib_hash_set(ib->dirmap, rec->name, (void *)rec);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_config_directive_process(ib_cfgparser_t *cp,
                                        const char *name,
                                        ib_list_t *args)
{
    IB_FTRACE_INIT();
    ib_engine_t *ib = cp->ib;
    ib_dirmap_init_t *rec;
    ib_list_node_t *node;
    size_t nargs = ib_list_elements(args);
    const char *p1;
    const char *p2;
    ib_flags_t flags;
    ib_flags_t fmask;
    ib_status_t rc;
    int i;

    rc = ib_hash_get(ib->dirmap, &rec, name);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    switch (rec->type) {
        case IB_DIRTYPE_ONOFF:
            if (nargs != 1) {
                ib_cfg_log_error(cp,
                                 "OnOff directive \"%s\" "
                                 "takes one parameter, not %zd",
                                 name, nargs);
                rc = IB_EINVAL;
                break;
            }
            ib_list_shift(args, &p1);
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
                                 "Param1 directive \"%s\" "
                                 "takes one parameter, not %zd",
                                 name, nargs);
                rc = IB_EINVAL;
                break;
            }
            ib_list_shift(args, &p1);
            rc = rec->cb.fn_param1(cp, name, p1, rec->cbdata_cb);
            break;
        case IB_DIRTYPE_PARAM2:
            if (nargs != 2) {
                ib_cfg_log_error(cp,
                                 "Param2 directive \"%s\" "
                                 "takes two parameters, not %zd",
                                 name, nargs);
                rc = IB_EINVAL;
                break;
            }
            ib_list_shift(args, &p1);
            ib_list_shift(args, &p2);
            rc = rec->cb.fn_param2(cp, name, p1, p2, rec->cbdata_cb);
            break;
        case IB_DIRTYPE_LIST:
            rc = rec->cb.fn_list(cp, name, args, rec->cbdata_cb);
            break;
        case IB_DIRTYPE_OPFLAGS:
            i = 0;
            flags = 0;
            fmask = 0;

            IB_LIST_LOOP(args, node) {
                const char *opname = (const char *)ib_list_node_data(node);
                int oper = (*opname == '-') ? -1 : ((*opname == '+') ? 1 : 0);
                ib_num_t val;

                /* If the first option does not use an operator, then
                 * this is setting all flags so set all the mask bits.
                 */
                if ((i == 0) && (oper == 0)) {
                    fmask = ~0;
                }

                ib_cfg_log_debug3(cp, "Processing %s option: %s", name, opname);

                /* Remove the operator from the name if required.
                 * and determine the numeric value of the option
                 * by using the value map.
                 */
                if (oper != 0) {
                    opname++;
                }

                rc = cfgp_opval(opname, rec->valmap, &val);
                if (rc != IB_OK) {
                    ib_cfg_log_error(cp, "Invalid %s option: %s", name, opname);
                    IB_FTRACE_RET_STATUS(rc);
                }

                /* Mark which bit(s) we are setting. */
                fmask |= val;

                /* Set/Unset the appropriate bits. */
                if (oper == -1) {
                    flags = flags & ~val;
                }
                else {
                    flags |= val;
                }

                i++;
            }

            rc = rec->cb.fn_opflags(cp, name, flags, fmask, rec->cbdata_cb);
            break;
        case IB_DIRTYPE_SBLK1:
            if (nargs != 1) {
                ib_cfg_log_error(cp,
                                 "SBlk1 directive \"%s\" "
                                 "takes one parameter, not %zd",
                                 name, nargs);
                rc = IB_EINVAL;
                break;
            }
            ib_list_shift(args, &p1);
            rc = rec->cb.fn_sblk1(cp, name, p1, rec->cbdata_cb);
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
    IB_FTRACE_INIT();
    ib_engine_t *ib = cp->ib;
    ib_dirmap_init_t *rec;
    ib_status_t rc;

    /* Finished with this block. */
    rc = ib_cfgparser_block_pop(cp, NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_hash_get(ib->dirmap, &rec, name);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
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

    IB_FTRACE_RET_STATUS(rc);
}

void ib_cfg_log_f(ib_cfgparser_t *cp, ib_log_level_t level,
                 const char *file, int line,
                 const char *fmt, ...)
{
    IB_FTRACE_INIT();
    assert(cp != NULL);

    va_list ap;
    va_start(ap, fmt);

    ib_cfg_vlog(cp, level, file, line, fmt, ap);

    va_end(ap);

    IB_FTRACE_RET_VOID();
}

void ib_cfg_log_ex_f(const ib_engine_t *ib,
                     const char *cfgfile, unsigned int cfgline,
                     ib_log_level_t level,
                     const char *file, int line,
                     const char *fmt, ...)
{
    IB_FTRACE_INIT();
    assert(ib != NULL);

    va_list ap;
    va_start(ap, fmt);

    ib_cfg_vlog_ex(ib, cfgfile, cfgline, level, file, line, fmt, ap);

    va_end(ap);

    IB_FTRACE_RET_VOID();
}

void ib_cfg_vlog_ex(const ib_engine_t *ib,
                    const char *cfgfile, unsigned int cfgline,
                    ib_log_level_t level,
                    const char *file, int line,
                    const char *fmt, va_list ap)
{
    IB_FTRACE_INIT();
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

    ib_vlog_ex(ib, level, file, line, which_fmt, ap);

    if (new_fmt != NULL) {
        free(new_fmt);
    }

    IB_FTRACE_RET_VOID();
}

void ib_cfg_vlog(ib_cfgparser_t *cp, ib_log_level_t level,
                 const char *file, int line,
                 const char *fmt, va_list ap)
{
    IB_FTRACE_INIT();

    ib_cfg_vlog_ex(cp->ib, cp->cur_file, cp->cur_lineno,
                   level, file, line, fmt, ap);

    IB_FTRACE_RET_VOID();
}
