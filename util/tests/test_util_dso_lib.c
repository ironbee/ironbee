//////////////////////////////////////////////////////////////////////////////
// Licensed to Qualys, Inc. (QUALYS) under one or more
// contributor license agreements.  See the NOTICE file distributed with
// this work for additional information regarding copyright ownership.
// QUALYS licenses this file to You under the Apache License, Version 2.0
// (the "License"); you may not use this file except in compliance with
// the License.  You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
/// @file
/// @brief IronBee --- Dynamic Shared Object Tests
///
/// @author Nick LeRoy <nleroy@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "ironbee_config_auto.h"

#include <ironbee/hash.h>

#include <ironbee/types.h>
#include <ironbee/mm.h>

#include "test_util_dso.h"

#include <assert.h>

const uint32_t pat1_val = 0x5a5a5a5a;
const uint32_t pat2_val = 0xa5a5a5a5;
struct ib_test_util_dso_data_t
{
    uint32_t    pat1;
    ib_mm_t     mm;
    int         num;
    const char *str;
    uint32_t    pat2;
};

static ib_status_t check_data(const ib_test_util_dso_data_t *data)
{
    assert(data != NULL);

    if (data->pat1 != pat1_val) {
        return IB_EINVAL;
    }
    if (data->pat2 != pat2_val) {
        return IB_EINVAL;
    }
    return IB_OK;
}

static ib_status_t ib_test_util_dso_create(ib_test_util_dso_data_t **data,
                                           ib_mm_t mm,
                                           int num)
{
    assert(data != NULL);

    ib_test_util_dso_data_t *newdata =
        (ib_test_util_dso_data_t *)ib_mm_alloc(mm, sizeof(*newdata));
    if (newdata == NULL) {
        return IB_EALLOC;
    }
    newdata->pat1 = pat1_val;
    newdata->pat2 = pat2_val;
    newdata->mm = mm;
    newdata->num = num;
    newdata->str = NULL;
    *data = newdata;
    return check_data(newdata);
}

static ib_status_t ib_test_util_dso_destroy(ib_test_util_dso_data_t *data)
{
    ib_status_t rc = check_data(data);
    if (rc != IB_OK) {
        return rc;
    }
    data->pat1 = 0;
    data->pat2 = 0;
    return IB_OK;
}

static ib_status_t ib_test_util_dso_setnum(ib_test_util_dso_data_t *data,
                                           int num)
{
    ib_status_t rc = check_data(data);
    if (rc != IB_OK) {
        return rc;
    }
    data->num = num;
    return IB_OK;
}

static ib_status_t ib_test_util_dso_getnum(const ib_test_util_dso_data_t *data,
                                           int *num)
{
    ib_status_t rc = check_data(data);
    if (rc != IB_OK) {
        return rc;
    }
    *num = data->num;
    return IB_OK;
}

static ib_status_t ib_test_util_dso_setstr(ib_test_util_dso_data_t *data,
                                           const char *str)
{
    ib_status_t rc = check_data(data);
    if (rc != IB_OK) {
        return rc;
    }
    if (str == NULL) {
        return IB_EINVAL;
    }
    data->str = ib_mm_strdup(data->mm, str);
    if (data->str == NULL) {
        return IB_EALLOC;
    }
    return IB_OK;
}

static ib_status_t ib_test_util_dso_getstr(const ib_test_util_dso_data_t *data,
                                           const char **str)
{
    ib_status_t rc = check_data(data);
    if (rc != IB_OK) {
        return rc;
    }
    *str = data->str;
    return IB_OK;
}

static ib_test_util_dso_fns_t dso_fns = {
    ib_test_util_dso_create,
    ib_test_util_dso_destroy,
    ib_test_util_dso_setnum,
    ib_test_util_dso_getnum,
    ib_test_util_dso_setstr,
    ib_test_util_dso_getstr,
};

ib_status_t DLL_PUBLIC ib_test_util_dso_getfns(ib_test_util_dso_fns_t **fns);

ib_status_t ib_test_util_dso_getfns(ib_test_util_dso_fns_t **fns)
{
    assert(fns != NULL);
    *fns = &dso_fns;
    return IB_OK;
}
