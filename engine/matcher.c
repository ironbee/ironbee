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
 * @brief IronBee - Matcher
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <string.h>

#include <ironbee/engine.h>
#include <ironbee/mpool.h>
#include <ironbee/bytestr.h>
#include <ironbee/provider.h>

#include "ironbee_private.h"


ib_status_t ib_matcher_create(ib_engine_t *ib,
                              ib_mpool_t *pool,
                              const char *key,
                              ib_matcher_t **pm)
{
    IB_FTRACE_INIT(ib_matcher_create);
    ib_provider_t *mpr;
    ib_status_t rc;

    *pm = (ib_matcher_t *)ib_mpool_alloc(pool, sizeof(**pm));
    if (*pm == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    rc = ib_provider_lookup(ib, IB_PROVIDER_TYPE_MATCHER, key, &mpr);
    if (rc != IB_OK) {
        *pm = NULL;
        IB_FTRACE_RET_STATUS(rc);
    }

    (*pm)->ib = ib;
    (*pm)->mp = pool;
    (*pm)->mpr = mpr;
    (*pm)->mpi = NULL;
    (*pm)->key = ib_mpool_strdup(pool, key);

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_matcher_instance_create(ib_engine_t *ib,
                                       ib_mpool_t *pool,
                                       const char *key,
                                       ib_matcher_t **pm)
{
    IB_FTRACE_INIT(ib_matcher_instance_create);
    ib_provider_t *mpr;
    ib_status_t rc;

    *pm = (ib_matcher_t *)ib_mpool_alloc(pool, sizeof(**pm));
    if (*pm == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    rc = ib_provider_lookup(ib, IB_PROVIDER_TYPE_MATCHER, key, &mpr);
    if (rc != IB_OK) {
        *pm = NULL;
        IB_FTRACE_RET_STATUS(rc);
    }

    (*pm)->ib = ib;
    (*pm)->mp = pool;
    (*pm)->mpr = mpr;

    (*pm)->key = ib_mpool_strdup(pool, key);

    rc = ib_provider_instance_create_ex(ib, mpr, &((*pm)->mpi), pool,
                                        NULL);
    if (rc != IB_OK) {
        *pm = NULL;
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

void *ib_matcher_compile(ib_matcher_t *m,
                         const char *patt,
                         const char **errptr,
                         int *erroffset)
{
    IB_FTRACE_INIT(ib_matcher_compile);
    IB_PROVIDER_API_TYPE(matcher) *mapi;
    void *cpatt;
    ib_status_t rc;

    mapi = (IB_PROVIDER_API_TYPE(matcher) *)m->mpr->api;
    rc = mapi->compile_pattern(m->mpr, m->mp, &cpatt, patt,
                               errptr, erroffset);
    if (rc != IB_OK) {
        ib_log_debug(m->ib, 4, "Failed to compile %s patt: (%d) %s at "
                     "offset %d", rc, errptr, erroffset);
        IB_FTRACE_RET_PTR(void, NULL);
    }

    IB_FTRACE_RET_PTR(void, cpatt);
}

ib_status_t ib_matcher_match_buf(ib_matcher_t *m,
                                 void *cpatt,
                                 ib_flags_t flags,
                                 const uint8_t *data,
                                 size_t dlen,
                                 void *ctx)
{
    IB_FTRACE_INIT(ib_matcher_match_buf);
    IB_PROVIDER_API_TYPE(matcher) *mapi;
    ib_status_t rc;

    mapi = (IB_PROVIDER_API_TYPE(matcher) *)m->mpr->api;
    rc = mapi->match_compiled(m->mpr, cpatt, 0, data, dlen, ctx);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_matcher_match_field(ib_matcher_t *m,
                                   void *cpatt,
                                   ib_flags_t flags,
                                   ib_field_t *f,
                                   void *ctx)
{
    IB_FTRACE_INIT(ib_matcher_match_field);
    IB_PROVIDER_IFACE_TYPE(matcher) *iface;
    ib_bytestr_t *bs;
    char *cs;
    ib_status_t rc;

    iface = (IB_PROVIDER_IFACE_TYPE(matcher) *)m->mpr->iface;

    switch (f->type) {
        case IB_FTYPE_BYTESTR:
            bs = ib_field_value_bytestr(f);
            rc = iface->match_compiled(m->mpr, cpatt, flags,
                                       ib_bytestr_ptr(bs),
                                       ib_bytestr_length(bs), ctx);
            break;
        case IB_FTYPE_NULSTR:
            cs = ib_field_value_nulstr(f);
            rc = iface->match_compiled(m->mpr, cpatt, flags,
                                       (uint8_t *)cs,
                                       strlen(cs), ctx);
            break;
        /// @todo How to handle numeric fields???
        default:
            rc = IB_EINVAL;
            ib_log_error(m->ib, 3, "Not matching against field type=%d",
                         f->type);
            break;
    }

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_matcher_add_pattern(ib_matcher_t *m,
                                   const char *patt)
{
    IB_FTRACE_INIT(ib_matcher_add_pattern);
    IB_FTRACE_RET_STATUS(IB_ENOTIMPL);
}

ib_status_t ib_matcher_add_pattern_ex(ib_matcher_t *m,
                                      const char *patt,
                                      ib_void_fn_t callback,
                                      void *arg,
                                      const char **errptr,
                                      int *erroffset)
{
    IB_FTRACE_INIT(ib_matcher_add_pattern_ex);
    IB_PROVIDER_API_TYPE(matcher) *mapi;
    ib_status_t rc; 
    
    mapi = (IB_PROVIDER_API_TYPE(matcher) *)m->mpr->api;
    
    rc = mapi->add_pattern_ex(m->mpi, &m->mpr->data, patt, callback, arg,
                               errptr, erroffset);
    if (rc != IB_OK) {
        ib_log_debug(m->mpr->ib, 4, "Failed to add pattern %s patt: (%d) %s at "
                               "offset %d", patt, rc, errptr, erroffset);
    }
    IB_FTRACE_RET_STATUS(rc);

}

ib_status_t ib_matcher_exec_buf(ib_matcher_t *m,
                                ib_flags_t flags,
                                const uint8_t *data,
                                size_t dlen,
                                void *ctx)
{
    IB_FTRACE_INIT(ib_matcher_exec_buf);
    IB_PROVIDER_API_TYPE(matcher) *mapi;
    ib_status_t rc;

    mapi = (IB_PROVIDER_API_TYPE(matcher) *)m->mpr->api;
    rc = mapi->match(m->mpi, 0, data, dlen, ctx);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_matcher_exec_field(ib_matcher_t *m,
                                  ib_flags_t flags,
                                  ib_field_t *f,
                                  void *ctx)
{
    IB_FTRACE_INIT(ib_matcher_exec_field);
    IB_PROVIDER_IFACE_TYPE(matcher) *iface;
    ib_bytestr_t *bs;
    char *cs;
    ib_status_t rc;

    iface = (IB_PROVIDER_IFACE_TYPE(matcher) *)m->mpr->iface;

    switch (f->type) {
        case IB_FTYPE_BYTESTR:
            bs = ib_field_value_bytestr(f);
            rc = iface->match(m->mpi, flags,
                              ib_bytestr_ptr(bs),
                              ib_bytestr_length(bs), ctx);
            break;
        case IB_FTYPE_NULSTR:
            cs = ib_field_value_nulstr(f);
            rc = iface->match(m->mpi, flags,
                                       (uint8_t *)cs,
                                       strlen(cs), ctx);
            break;
        /// @todo How to handle numeric fields???
        default:
            rc = IB_EINVAL;
            ib_log_error(m->ib, 3, "Not matching against field type=%d",
                         f->type);
            break;
    }

    IB_FTRACE_RET_STATUS(rc);
}


