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
 * @brief IronBee - Data Access
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>


#include <ironbee/engine.h>
#include <ironbee/util.h>
#include <ironbee/provider.h>

#include "ironbee_private.h"


/* -- Exported Data Access Routines -- */

ib_status_t ib_data_add(ib_provider_inst_t *dpi,
                        ib_field_t *f)
{
    IB_FTRACE_INIT(ib_data_add);
    IB_PROVIDER_API_TYPE(data) *api = dpi->pr->api;
    ib_status_t rc;

    rc = api->add(dpi, f);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_data_add_num_ex(ib_provider_inst_t *dpi,
                               const char *name,
                               size_t nlen,
                               uint64_t val,
                               ib_field_t **pf)
{
    IB_FTRACE_INIT(ib_data_add_num);
    IB_PROVIDER_API_TYPE(data) *api = dpi->pr->api;
    ib_field_t *f;
    ib_status_t rc;

    if (pf != NULL) {
        *pf = NULL;
    }

    rc = ib_field_create_ex(&f, dpi->mp, name, nlen, IB_FTYPE_NUM, &val);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = api->add(dpi, f);
    if ((rc == IB_OK) && (pf != NULL)) {
        *pf = f;
    }

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_data_add_nulstr_ex(ib_provider_inst_t *dpi,
                                  const char *name,
                                  size_t nlen,
                                  char *val,
                                  ib_field_t **pf)
{
    IB_FTRACE_INIT(ib_data_add_nulstr);
    IB_PROVIDER_API_TYPE(data) *api = dpi->pr->api;
    ib_field_t *f;
    ib_status_t rc;

    if (pf != NULL) {
        *pf = NULL;
    }

    rc = ib_field_create_ex(&f, dpi->mp, name, nlen, IB_FTYPE_NULSTR, &val);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = api->add(dpi, f);
    if ((rc == IB_OK) && (pf != NULL)) {
        *pf = f;
    }

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_data_add_bytestr_ex(ib_provider_inst_t *dpi,
                                   const char *name,
                                   size_t nlen,
                                   uint8_t *val,
                                   size_t vlen,
                                   ib_field_t **pf)
{
    IB_FTRACE_INIT(ib_data_add_bytestr);
    IB_PROVIDER_API_TYPE(data) *api = dpi->pr->api;
    ib_field_t *f;
    ib_status_t rc;

    if (pf != NULL) {
        *pf = NULL;
    }

    rc = ib_field_alias_mem_ex(&f, dpi->mp, name, nlen, val, vlen);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = api->add(dpi, f);
    if ((rc == IB_OK) && (pf != NULL)) {
        *pf = f;
    }

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_data_add_list_ex(ib_provider_inst_t *dpi,
                                const char *name,
                                size_t nlen,
                                ib_field_t **pf)
{
    IB_FTRACE_INIT(ib_data_add_list);
    IB_PROVIDER_API_TYPE(data) *api = dpi->pr->api;
    ib_field_t *f;
    ib_status_t rc;

    if (pf != NULL) {
        *pf = NULL;
    }

    rc = ib_field_create_ex(&f, dpi->mp, IB_S2SL(name), IB_FTYPE_LIST, NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = api->add(dpi, f);
    if ((rc == IB_OK) && (pf != NULL)) {
        *pf = f;
    }

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_data_get_ex(ib_provider_inst_t *dpi,
                           const char *name,
                           size_t nlen,
                           ib_field_t **pf)
{
    IB_FTRACE_INIT(ib_data_get);
    IB_PROVIDER_API_TYPE(data) *api = dpi->pr->api;
    ib_status_t rc;

    rc = api->get(dpi, name, nlen, pf);
    IB_FTRACE_RET_STATUS(rc);
}

