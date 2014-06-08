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
 * @brief Apache 2.x Module to replace byte ranges in input or output stream
 *
 * @author Nick Kew <nkew@qualys.com>
 */

#include <httpd.h>
#include <http_protocol.h>
#include <http_request.h>
#include <http_connection.h>
#include <http_config.h>
#include <http_log.h>
#include <util_filter.h>
#include <assert.h>

#include "mod_range_filter.h"

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation-deprecated-sync"
#endif
#include <apr_strings.h>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

/* Hack to detect 2.2 vs 2.4 server versions.
 * This is a 2.3.x version shortly after the module declaration syntax changed
 * and might leave some 2.3.x in limbo, but should hopefully do the job
 * of reliably distinguishing 2.2 vs 2.4.
 */
#define NEWVERSION AP_MODULE_MAGIC_AT_LEAST(20100919,1)


module AP_MODULE_DECLARE_DATA range_filter_module;

/* per-dir configuration.  Turn filtering on or off */
typedef struct range_filter_conf {
    int filter_input;
    int filter_output;
} range_filter_conf;

/* per-filter ctx is identical for input and output filters. */
typedef struct filter_ctx {
    apr_off_t count;               /* bytes already consumed&forgotten */
    apr_array_header_t *edits;     /* edits we're asked to perform */
    apr_bucket_brigade *bb;        /* data buffer */
    apr_bucket_brigade *newb;      /* data buffer */
} filter_ctx;

/* List of edits to perform.  Populated by exported API functions;
 * processed by our filter functions.
 */
typedef struct req_edits {
    apr_array_header_t *edits_in;
    apr_array_header_t *edits_out;
} req_edits;

/* Definition of an individual edit */
typedef struct range_edit {
    apr_off_t start;             /* start of edit, measured in bytes from
                                  * start of unedited data stream
                                  */
    apr_size_t bytes;            /* Number of bytes to delete in this edit */
    const char *subs;            /* data to insert in this edit */
    apr_size_t len;              /* length of data to insert */
} range_edit;

/**
 * Comparison function for qsort to order edits
 * Sort in reverse so apr_array_pop discards "first" elt for us
 *
 * @param[in] a - first edit
 * @param[in] b - second edit
 * @return difference in edits respective positions in stream
 */
static int qcompare(const void *a, const void *b)
{
    return ((range_edit*)b)->start - ((range_edit*)a)->start;
}
/**
 * HTTPD filter function to apply edits to response data
 *
 * @param[in] f - the filter struct
 * @param[in] bb - the bucket brigade (data)
 * @return status propagated from next filter in chain
 */
static apr_status_t range_filter_out(ap_filter_t *f, apr_bucket_brigade *bb)
{
    apr_status_t rv = APR_SUCCESS;
    apr_off_t bytes = 0;
    apr_bucket *b;
    apr_bucket_brigade *tmpb;
    int seen_eos = 0;
    size_t offs;

    filter_ctx *ctx = f->ctx;
    if (!ctx) {
        f->ctx = ctx = apr_pcalloc(f->r->pool, sizeof(filter_ctx));
        ctx->bb = apr_brigade_create(f->r->pool,
                                     f->r->connection->bucket_alloc);
        apr_table_unset(f->r->headers_out, "Content-Length");
    }
    offs = ctx->count;

    /* append to any data left over from last time */
    APR_BRIGADE_CONCAT(ctx->bb, bb);

    /* We need to count bytes even if there are no edits.
     * There may be edits in future!
     * FIXME: this is inefficient if it causes any buckets to mutate.
     *        Can't happen with content-aware filters like Ironbee!
     */
    rv = apr_brigade_length(ctx->bb, 1, &bytes);
    ap_assert(rv == APR_SUCCESS);

    if (!ctx->edits) {
        req_edits *redits = ap_get_module_config(f->r->request_config,
                                                 &range_filter_module);
        if (redits) {
            ctx->edits = redits->edits_out;
        }
    }
    if (!ctx->edits || apr_is_empty_array(ctx->edits)) {
        /* Nothing to do but record how much data we passed */
        ctx->count += bytes;
        rv = ap_pass_brigade(f->next, ctx->bb);
        (void)apr_brigade_cleanup(ctx->bb);
        return rv;
    }

    /* OK, go through edits, and apply any that are in range. */
    /* Sort first so we can deal with offsets that move with each edit */
    qsort(ctx->edits->elts, ctx->edits->nelts, ctx->edits->elt_size, qcompare);

    /* Now we can apply in-range edits in order */
    while (!apr_is_empty_array(ctx->edits)) {
        range_edit *edits = (range_edit*) ctx->edits->elts;
        range_edit edit = edits[ctx->edits->nelts-1];

        /* If edit is out of range, leave it for next time.
         * We can pass on everything before this but we have to save
         * what we're due to edit.
         */
        if ((size_t)(edit.start + edit.bytes) > (size_t)(offs + bytes)) {
            /* all done with the data for now */
            break;
        }
        
        /* Edit is in-range.  So apply it and remove from list */

        /* split the brigade at the start of the edit
         * We can re-use the now-emptied bb for tmp data
         */
        rv = apr_brigade_partition(ctx->bb, edit.start - offs, &b);
        tmpb = apr_brigade_split_ex(ctx->bb, b, bb);

        /* and remove what's to be cut (if any) .... */
        if (edit.bytes > 0) {
            rv = apr_brigade_partition(tmpb, edit.bytes, &b);
            ctx->newb = apr_brigade_split_ex(tmpb, b, ctx->newb);
            (void)apr_brigade_cleanup(tmpb);
        }
        else {
            /* newb should be tmpb */
            ctx->newb = tmpb;
        }

        /* Now we can insert the new data (if any) */
        /* TODO: consider API.  Pool bucket here matches pool alloc in
         * range_substitute_out.
         */
        b = apr_bucket_pool_create(edit.subs, edit.len, f->r->pool,
                                   f->r->connection->bucket_alloc);
        APR_BRIGADE_INSERT_TAIL(ctx->bb, b);

        /* TODO: think about this.  Good practice to send it on right now,
         * with the proviso that Ironbee flush policies might suggest otherwise.
         */
        rv = ap_pass_brigade(f->next, ctx->bb);
        (void)apr_brigade_cleanup(ctx->bb);
        tmpb = ctx->bb;
        ctx->bb = ctx->newb;
        ctx->newb = tmpb;
        /* increment counters */
        bytes -= edit.start + edit.bytes - offs;
        offs = edit.start + edit.bytes;

        /* All done with this edit.  Chop it. */
        apr_array_pop(ctx->edits);
    }

    /* we already passed edited data on.  Setaside what's left & return */
    /* FIXME - maybe use Ironbee flush rules to set max. amount to save here? */
    for (b = APR_BRIGADE_FIRST(ctx->bb);
         b != APR_BRIGADE_SENTINEL(ctx->bb) && !seen_eos;
         b = APR_BUCKET_NEXT(b)) {
        if (!seen_eos) {
            apr_bucket_setaside(b, f->r->pool);
            seen_eos = APR_BUCKET_IS_EOS(b);
        }
    }
    if (seen_eos) {
        rv = ap_pass_brigade(f->next, ctx->bb);
    }
    ctx->count = offs;
    return rv;
}

/**
 * HTTPD filter function to apply edits to request data
 *
 * @param[in] f - the filter struct
 * @param[in] bb - the bucket brigade (data)
 * @param[in] mode - currently ignored
 * @param[in] block - blocking requested
 * @param[in] readbytes - data size requested
 * @return status propagated from next filter in chain
 *
 * FIXME: flesh out handling mode/block/readbytes
 */

static apr_status_t range_filter_in(ap_filter_t *f,
                                    apr_bucket_brigade *bb,
                                    ap_input_mode_t mode,
                                    apr_read_type_e block,
                                    apr_off_t readbytes)
{
    apr_status_t rv = APR_SUCCESS;
    apr_off_t bytes = 0;
    apr_off_t bytes_ret = 0;
    apr_off_t offs = 0;
    apr_bucket *b;
    apr_bucket_brigade *tmpb;
    int seen_eos = 0;

    filter_ctx *ctx = f->ctx;
    if (!ctx) {
        f->ctx = ctx = apr_pcalloc(f->r->pool, sizeof(filter_ctx));
        ctx->bb = apr_brigade_create(f->r->pool,
                                     f->r->connection->bucket_alloc);
    }

    /* top up data before operating */
    rv = ap_get_brigade(f->next, ctx->bb, AP_MODE_READBYTES, block, readbytes);
    APR_BRIGADE_CONCAT(bb, ctx->bb);
    (void)apr_brigade_length(bb, 1, &bytes);

    if (!ctx->edits) {
        req_edits *redits = ap_get_module_config(f->r->request_config,
                                                 &range_filter_module);
        if (redits) {
            ctx->edits = redits->edits_in;
        }
    }
    if (!ctx->edits || apr_is_empty_array(ctx->edits)) {
        /* Nothing to do but record how much data we passed */
        ctx->count += bytes;
        return rv;
    }

    /* OK, go through edits, and apply any that are in range. */
    /* Sort first so we can deal with offsets that move with each edit */
    qsort(ctx->edits->elts, ctx->edits->nelts, ctx->edits->elt_size, qcompare);

    /* Now we can apply in-range edits in order */
    while (!apr_is_empty_array(ctx->edits)) {
        range_edit *edits = (range_edit*) ctx->edits->elts;
        range_edit edit = edits[ctx->edits->nelts-1];

        /* If edit is out of range, leave it for next time.
         * We can pass on everything before this but we have to save
         * what we're due to edit.
         */
        if ((size_t)(edit.start + edit.bytes) > (size_t)(ctx->count + bytes)) {
            /* all done with the data for now */
            bytes_ret = edit.start;
            break;
        }

        /* Edit is in-range.  So apply it and remove from list */

        /* split the brigade at the start of the edit
         * We can re-use the now-emptied bb for tmp data
         */
        rv = apr_brigade_partition(bb, edit.start - ctx->count + offs, &b);
        ctx->newb = apr_brigade_split_ex(bb, b, ctx->newb);

        /* and remove what's to be cut (if any) .... */
        if (edit.bytes > 0) {
            rv = apr_brigade_partition(ctx->newb, edit.bytes, &b);
            ctx->bb = apr_brigade_split_ex(ctx->newb, b, ctx->bb);
            (void)apr_brigade_cleanup(ctx->newb);
        }
        else {
            /* newb should be tmpb */
            tmpb = ctx->bb;
            ctx->bb = ctx->newb;
            ctx->newb = tmpb;
        }

        /* Now we can insert the new data (if any) */
        /* TODO: consider API.  Pool bucket here matches pool alloc in
         * range_substitute_out.
         */
        b = apr_bucket_pool_create(edit.subs, edit.len, f->r->pool,
                                   f->r->connection->bucket_alloc);
        APR_BRIGADE_INSERT_TAIL(bb, b);
        offs += edit.len - edit.bytes;
        APR_BRIGADE_CONCAT(bb, ctx->bb);

        /* All done with this edit.  Chop it. */
        apr_array_pop(ctx->edits);
    }
    /* If/when we first hit an out-of-range edit, return all before it
     * and use its start as bytes
     */
    if (bytes_ret > 0) {
        /* sanity check.  Legitimate if ironbee is buffering ahead of us */
        if (bytes_ret > bytes) {
            ctx->count += bytes;
        }
        else {
            ctx->count += bytes_ret;
            rv = apr_brigade_partition(bb, bytes_ret + offs, &b);
            ctx->bb = apr_brigade_split_ex(bb, b, ctx->bb);
        }
        /* set aside data we're sitting on */
        /* FIXME - use Ironbee flush rules to set max. amount to save here? */
        for (b = APR_BRIGADE_FIRST(ctx->bb);
             b != APR_BRIGADE_SENTINEL(ctx->bb) && !seen_eos;
             b = APR_BUCKET_NEXT(b)) {
            apr_bucket_setaside(b, f->r->pool);
            seen_eos = APR_BUCKET_IS_EOS(b);
        }
        if (seen_eos) {
            /* nothing more to do.  Return all remaining data to caller */
            APR_BRIGADE_CONCAT(bb, ctx->bb);
        }
    }
    else {
        /* we're returning all the data */
        ctx->count += bytes;
    }

    return rv;
}
/**
 * HTTPD callback function to insert filters
 * @param[in] r - the Request
 */
static void range_filter_insert(request_rec *r)
{
    range_filter_conf *cfg;

    cfg = ap_get_module_config(r->per_dir_config, &range_filter_module);

    /* Default to ON: the "unset" value is -1 */
    if (cfg->filter_input) {
        ap_add_input_filter("range-edit", NULL, r, r->connection);
    }
    if (cfg->filter_output) {
        ap_add_output_filter("range-edit", NULL, r, r->connection);
    }
}

/**
 * Exported API function for another module to request editing Request data
 *
 * @param[in] r - the Request
 * @param[in] start - start of edit
 * @param[in] bytes - bytes to delete
 * @param[in] subs - replacement data
 * @param[in] len - length of replacement data
 * @return - Success, or error code if not enabled
 */
static apr_status_t range_substitute_in(request_rec *r,
                                        apr_off_t start,
                                        apr_size_t bytes,
                                        const char *subs,
                                        apr_size_t len)
{
    req_edits *edits;
    range_edit edit;
    range_filter_conf *conf;

    conf = ap_get_module_config(r->per_dir_config, &range_filter_module);
    if (!conf->filter_input) {
        return APR_EGENERAL;
    }

    edits = ap_get_module_config(r->request_config, &range_filter_module);
    if (!edits) {
        edits = apr_pcalloc(r->pool, sizeof(req_edits));
        ap_set_module_config(r->request_config, &range_filter_module, edits);
    }

    if (!edits->edits_in) {
        edits->edits_in = apr_array_make(r->pool, 20, sizeof(range_edit));
    }

    edit.start = start;
    edit.bytes = bytes;
    edit.subs = apr_pmemdup(r->pool, subs, len);
    edit.len = len;
    APR_ARRAY_PUSH(edits->edits_in, range_edit) = edit;

    return APR_SUCCESS;
}
/**
 * Exported API function for another module to request editing Response data
 *
 * @param[in] r - the Request
 * @param[in] start - start of edit
 * @param[in] bytes - bytes to delete
 * @param[in] subs - replacement data
 * @param[in] len - length of replacement data
 * @return - Success, or error code if not enabled
 */
static apr_status_t range_substitute_out(request_rec *r,
                                         apr_off_t start,
                                         apr_size_t bytes,
                                         const char *subs,
                                         apr_size_t len)
{
    req_edits *edits;
    range_edit edit;
    range_filter_conf *conf;

    conf = ap_get_module_config(r->per_dir_config, &range_filter_module);
    if (!conf->filter_output) {
        return APR_EGENERAL;
    }

    edits = ap_get_module_config(r->request_config, &range_filter_module);
    if (!edits) {
        edits = apr_pcalloc(r->pool, sizeof(req_edits));
        ap_set_module_config(r->request_config, &range_filter_module, edits);
    }

    if (!edits->edits_out) {
        edits->edits_out = apr_array_make(r->pool, 20, sizeof(range_edit));
    }

    edit.start = start;
    edit.bytes = bytes;
    edit.subs = apr_pmemdup(r->pool, subs, len);
    edit.len = len;
    APR_ARRAY_PUSH(edits->edits_out, range_edit) = edit;

    return APR_SUCCESS;
}

/**
 * HTTPD module function to insert hooks, declare filters, and export API
 * @param[in] pool - APR pool
 */
static void range_filter_hooks(apr_pool_t *pool)
{
    /* Our header processing uses the same hooks as mod_headers and
     * needs to order itself with reference to that module if loaded
     */
    static const char * const mod_ironbee[] = { "mod_headers.c", "mod_ironbee.c", NULL };

    /* Main input and output filters */
    /* Set filter level between resource and content_set */
    ap_register_input_filter("range-edit", range_filter_in, NULL,
                             AP_FTYPE_RESOURCE);
    ap_register_output_filter("range-edit", range_filter_out, NULL,
                              AP_FTYPE_CONTENT_SET-1);

    /* Use our own insert filter hook.  This is best going last so anything
     * 'clever' happening elsewhere isn't troubled with ordering it.
     */
    ap_hook_insert_filter(range_filter_insert, mod_ironbee, NULL, APR_HOOK_LAST);

    /* Export our API */
    APR_REGISTER_OPTIONAL_FN(range_substitute_out);
    APR_REGISTER_OPTIONAL_FN(range_substitute_in);
}

/******************** CONFIG STUFF *******************************/

/**
 * Function to initialise HTTPD per-dir configuration
 * @param[in] p - The Pool
 * @param[in] dummy - unused
 * @return The created configuration struct
 */
static void *range_filter_create_cfg(apr_pool_t *p, char *dummy)
{
    range_filter_conf *cfg = apr_palloc(p, sizeof(range_filter_conf));
    cfg->filter_input = cfg->filter_output = -1;
    return cfg;
}
/**
 * Function to merge HTTPD per-dir configurations
 * @param[in] p - The Pool
 * @param[in] BASE - The base config
 * @param[in] ADD - The config to merge in
 * @return The new merged configuration struct
 */
static void *range_filter_merge_cfg(apr_pool_t *p, void *BASE, void *ADD)
{
    range_filter_conf *base = BASE;
    range_filter_conf *add = ADD;
    range_filter_conf *cfg = apr_palloc(p, sizeof(range_filter_conf));
    cfg->filter_input = (add->filter_input == -1)
                                    ? base->filter_input  :  add->filter_input;
    cfg->filter_output = (add->filter_output == -1)
                                    ? base->filter_output : add->filter_output;
    return cfg;
}


/**
 * Module Directives
 */
static const command_rec range_filter_cmds[] = {
    AP_INIT_FLAG("RangeFilterIn", ap_set_flag_slot,
                 (void*)APR_OFFSETOF(range_filter_conf, filter_input),
                 ACCESS_CONF, "Enable range editing of input data"),
    AP_INIT_FLAG("RangeFilterOut", ap_set_flag_slot,
                 (void*)APR_OFFSETOF(range_filter_conf, filter_output),
                 ACCESS_CONF, "Enable range editing of output data"),
    {NULL, {NULL}, NULL, 0, 0, NULL}
};

/**
 * Declare the module.
 */
#if NEWVERSION
AP_DECLARE_MODULE(range_filter)
#else
module AP_MODULE_DECLARE_DATA range_filter_module
#endif
= {
    STANDARD20_MODULE_STUFF,
    range_filter_create_cfg,
    range_filter_merge_cfg,
    NULL,
    NULL,
    range_filter_cmds,
    range_filter_hooks
};
