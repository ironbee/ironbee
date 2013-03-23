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

#include "config-parser.h"
#include "engine_private.h"

#include <ironbee/mpool.h>
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


/* -- Configuration Parser Routines -- */

ib_status_t ib_cfgparser_create(ib_cfgparser_t **pcp,
                                ib_engine_t *ib)
{
    assert(pcp != NULL);
    assert(ib != NULL);

    ib_mpool_t *pool;
    ib_status_t rc;
    ib_cfgparser_t *cp;

    *pcp = NULL;

    /* Create parser memory pool */
    rc = ib_mpool_create(&pool, "cfgparser", ib->mp);
    if (rc != IB_OK) {
        rc = IB_EALLOC;
        goto failed;
    }

    /* Create the configuration parser object from the memory pool */
    cp = (ib_cfgparser_t *)ib_mpool_calloc(pool, sizeof(*cp), 1);
    if (cp == NULL) {
        rc = IB_EALLOC;
        goto failed;
    }
    cp->ib = ib;
    cp->mp = pool;

    /* Create the stack */
    rc = ib_list_create(&(cp->stack), pool);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Create the block tracking list */
    rc = ib_list_create(&(cp->block), pool);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Create the include tracking list */
    rc = ib_hash_create(&(cp->includes), pool);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Other fields are NULLed via calloc */
    *pcp = cp;
    return IB_OK;

failed:
    /* Make sure everything is cleaned up on failure */
    ib_engine_pool_destroy(ib, pool);

    return rc;
}

static char *find_eol(char *buf, size_t len, size_t *skip)
{
    char *cr;
    char *lf;

    cr = (char *)memchr(buf, '\r', len);
    lf = (char *)memchr(buf, '\n', len);

    if ( (cr != NULL) && (lf == (cr + 1)) ) {
        *skip = 1;
        *cr = '\n';
        *lf = ' ';
        return cr;
    }
    else if (lf != NULL) {
        *skip = 1;
        return lf;
    }
    else {
        *skip = 0;
        return NULL;
    }
}

/// @todo Create a ib_cfgparser_parse_ex that can parse non-files (DBs, etc)

ib_status_t ib_cfgparser_parse(ib_cfgparser_t *cp,
                               const char *file)
{
    int ec             = 0;    /* Error code for sys calls. */
    int fd             = 0;    /* File to read. */
    unsigned lineno    = 1;    /* Current line number */
    ssize_t nbytes     = 0;    /* Bytes read by one read(). */
    const size_t bufsz = 8192; /* Buffer size. */
    size_t buflen      = 0;    /* Last char in buffer. */
    char *buf          = NULL; /* Buffer. */
    char *eol          = 0;    /* buf[eol] = end of line. */
    char *bol          = 0;    /* buf[bol] = begin line. */

    ib_status_t rc = IB_OK;
    unsigned error_count = 0;
    ib_status_t error_rc = IB_OK;

    fd = open(file, O_RDONLY);
    if (fd == -1) {
        ec = errno;
        ib_cfg_log_error(cp, "Could not open config file \"%s\": (%d) %s",
                         file, ec, strerror(ec));
        return IB_EINVAL;
    }

    buf = (char *)malloc(sizeof(*buf)*bufsz);

    if (buf==NULL) {
        ib_cfg_log_error(cp,
                         "Unable to allocate buffer for configuration file.");
        close(fd);
        return IB_EALLOC;
    }

    /* Fill the buffer, parse each line. Conditionally read another line. */
    do {
        nbytes = read(fd, buf+buflen, bufsz-buflen);
        buflen += nbytes;
        ib_cfg_log_debug3(cp,
                          "Read a %zd byte chunk. Total len=%zd",
                          nbytes, buflen);

        if ( nbytes == 0 ) { /* EOF */
            rc = ib_cfgparser_parse_buffer(
                cp, buf, nbytes, file, lineno, true
            );
            ++lineno;
            if (rc != IB_OK) {
                ++error_count;
                error_rc = rc;
            }

            break;
        }
        else if ( nbytes > 0 ) { /* Normal. */
            size_t skip;

            /* The first line always begins at buf[0]. */
            bol = buf;
            eol = find_eol(bol, buflen, &skip);

            /* Check that we found at least 1 end-of-line in this file. */
            if (eol == NULL) {
                if (buflen < bufsz) {
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
                    return IB_EINVAL;
                }
            }
            else {
                /* We have found at least one end-of-line character.
                 * Iterate through it and all others, passing each line to
                 * ib_cfgparser_parse_buffer() */
                do {
                    rc = ib_cfgparser_parse_buffer(cp, bol, eol-bol+skip,
                                                   file, lineno, false);
                    ++lineno;
                    if (rc != IB_OK) {
                        ++error_count;
                        error_rc = rc;
                    }
                    bol = eol + skip;

                    /* Find next end of line */
                    eol = find_eol(bol, buf+buflen-bol, &skip);
                } while (eol != NULL);

                /* There are no more end-of-line opportunities.
                 * Now move the last end-of-line to the beginning. */
                ib_cfg_log_debug2(cp,
                                  "Buffer of length %zd must be shrunk.",
                                  buflen);
                ib_cfg_log_debug2(cp,
                                  "Beginning of last line is at index %zd.",
                                  bol-buf);
                buflen = buf + buflen - bol;
                if (buflen > 0) {
                    ib_cfg_log_debug2(cp,
                                      "Discarding parsed lines."
                                      " Moving %p to %p with length %zd.",
                                      bol, buf, buflen);
                    memmove(buf, bol, buflen);
                }
            }
        }
        else {
            /* nbytes < 0. This is an error. */
            ib_cfg_log_error(cp,
                             "Error reading log file %s - %s.",
                             file, strerror(errno));
            free(buf);
            close(fd);
            return IB_ETRUNC;
        }
    } while (nbytes > 0);

    free(buf);
    close(fd);
    ib_cfg_log_debug3(cp,
                      "Done reading config \"%s\" via fd=%d errno=%d",
                      file, fd, errno);

    if ( (error_count == 0) && (rc == IB_OK) ) {
        return IB_OK;
    }
    else if (rc == IB_OK) {
        rc = error_rc;
    }
    ib_cfg_log_error(cp, "%u Error(s) parsing config file: %s",
                     error_count, ib_status_to_string(rc));
    return rc;
}

ib_status_t ib_cfgparser_parse_buffer(ib_cfgparser_t *cp,
                                      const char     *buffer,
                                      size_t          length,
                                      const char     *file,
                                      unsigned        lineno,
                                      bool            more)
{
    ib_status_t rc;
    const char *end;

    assert(cp != NULL);
    assert(buffer != NULL);

    if (
        file == NULL || cp->cur_file == NULL ||
        strcmp(cp->cur_file, file) != 0
    ) {
        cp->cur_cwd = NULL;
        cp->cur_file = NULL;
        if (file != NULL) {
            cp->cur_file = (char *)ib_mpool_strdup(cp->mp, file);
            char *pathbuf = (char *)ib_mpool_strdup(cp->mp, file);
            if (pathbuf != NULL) {
                cp->cur_cwd = dirname(pathbuf);
            }
        }
    }
    cp->cur_lineno = lineno;

    /* If the previous line ended with a continuation character,
     * join it with this line. */
    if (cp->linebuf != NULL) {
        char *newbuf;
        size_t newlen = strlen(cp->linebuf);

        /* Skip leading whitespace */
        while ( (length > 0) && (isspace(*buffer) != 0) ) {
            ++buffer;
            --length;
        }
        newlen += (length + 2);
        newbuf = (char *)ib_mpool_alloc(cp->mp, newlen);
        if (newbuf == NULL) {
            ib_cfg_log_error(cp,
                "Unable to allocate line continuation buffer"
            );
            return IB_EALLOC;
        }
        strcpy(newbuf, cp->linebuf);
        strcat(newbuf, " ");
        strncat(newbuf, buffer, length);
        length = newlen - 1;
        *(newbuf+length) = '\0';
        buffer = newbuf;
        cp->linebuf = NULL;
    }

    if (length == 0) {
        return IB_OK;
    }

    /* Handle lines that end with a backslash */
    end = buffer + (length - 1);
    if (*end == '\n') {
        if (end == buffer) {
            return IB_OK;
        }
        --end;
    }
    if (*end == '\r') {
        if (end == buffer) {
            return IB_OK;
        }
        --end;
    }
    if (*end == '\\') {
        size_t len = end - buffer;
        char *newbuf = (char *)ib_mpool_alloc(cp->mp, len + 1);
        if (newbuf == NULL) {
            ib_cfg_log_error(cp,
                "Unable to allocate line continuation buffer"
            );
            return IB_EALLOC;
        }
        if (len > 0) {
            memcpy(newbuf, buffer, len);
        }
        *(newbuf+len) = '\0';
        cp->linebuf = newbuf;
        return IB_OK;
    }

    ib_cfg_log_debug(cp, "Passing \"%.*s\" to Ragel", (int)length, buffer);
    rc = ib_cfgparser_ragel_parse_chunk(cp,
                                        buffer,
                                        length,
                                        file,
                                        lineno,
                                        (more ? 1 : 0) );
    return rc;
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
        ib_cfg_log_error(cp, "Failed to push context %p(%s): %s",
                         ctx, ib_context_full_get(ctx),
                         ib_status_to_string(rc));
        return rc;
    }
    cfgp_set_current(cp, ctx);

    ib_cfg_log_debug3(cp, "Stack: ctx=%p(%s)",
                      cp->cur_ctx,
                      ib_context_full_get(cp->cur_ctx));

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
        ib_cfg_log_error(cp,
                         "Failed to pop context: %s",
                         ib_status_to_string(rc));
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

    if (ctx == NULL) {
        ib_cfg_log_debug3(cp, "Stack: [empty]");
    }
    else {
        ib_cfg_log_debug3(cp,
                          "Stack: ctx=%p(%s)",
                          cp->cur_ctx, ib_context_full_get(cp->cur_ctx));
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

ib_status_t DLL_PUBLIC ib_cfgparser_block_push(ib_cfgparser_t *cp,
                                               const char *name)
{
    assert(cp != NULL);
    assert(name != NULL);
    ib_status_t rc;

    rc = ib_list_push(cp->block, (void *)name);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp,
                         "Failed to push block %p: %s",
                         name, ib_status_to_string(rc));
        return rc;
    }
    cp->cur_blkname = name;

    return IB_OK;
}

ib_status_t DLL_PUBLIC ib_cfgparser_block_pop(ib_cfgparser_t *cp,
                                              const char **pname)
{
    assert(cp != NULL);
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
        return rc;
    }

    if (pname != NULL) {
        *pname = name;
    }

    /* The last in the list is now the current. */
    cp->cur_blkname = (const char *)ib_list_node_data(ib_list_last(cp->block));

    return IB_OK;
}

ib_status_t ib_cfgparser_destroy(ib_cfgparser_t *cp)
{
    assert(cp != NULL);

    if (cp != NULL) {
        ib_engine_pool_destroy(cp->ib, cp->mp);
    }

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

    rec = (ib_dirmap_init_t *)ib_mpool_alloc(ib->config_mp, sizeof(*rec));
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

    rc = ib_hash_set(ib->dirmap, rec->name, (void *)rec);

    return rc;
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
        return rc;
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

                ib_cfg_log_debug3(cp,
                    "Processing %s option: %s", name, opname
                );

                /* Remove the operator from the name if required.
                 * and determine the numeric value of the option
                 * by using the value map.
                 */
                if (oper != 0) {
                    ++opname;
                }

                rc = cfgp_opval(opname, rec->valmap, &val);
                if (rc != IB_OK) {
                    ib_cfg_log_error(cp,
                        "Invalid %s option: %s", name, opname
                    );
                    return rc;
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

                ++i;
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

    return rc;
}

ib_status_t ib_config_block_start(ib_cfgparser_t *cp,
                                  const char *name,
                                  ib_list_t *args)
{
    assert(cp != NULL);
    assert(name != NULL);

    ib_status_t rc = ib_cfgparser_block_push(cp, name);
    if (rc != IB_OK) {
        return rc;
    }
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

    /* Finished with this block. */
    rc = ib_cfgparser_block_pop(cp, NULL);
    if (rc != IB_OK) {
        return rc;
    }

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


void ib_cfg_log_f(ib_cfgparser_t *cp, ib_log_level_t level,
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
                     ib_log_level_t level,
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
                    ib_log_level_t level,
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

    ib_log_vex_ex(ib, level, file, line, which_fmt, ap);

    if (new_fmt != NULL) {
        free(new_fmt);
    }

    return;
}

void ib_cfg_vlog(ib_cfgparser_t *cp, ib_log_level_t level,
                 const char *file, int line,
                 const char *fmt, va_list ap)
{
    assert(cp != NULL);
    assert(fmt != NULL);

    ib_cfg_vlog_ex(cp->ib, cp->cur_file, cp->cur_lineno,
                   level, file, line, fmt, ap);

    return;
}
