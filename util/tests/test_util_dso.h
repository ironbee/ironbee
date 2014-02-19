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
#ifndef __TEST_UTIL_DSO_H__
#define __TEST_UTIL_DSO_H__

/**
 * @file
 *
 * IronBee - DSO test definitions
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include <ironbee/types.h>

typedef struct ib_test_util_dso_data_t ib_test_util_dso_data_t;

typedef ib_status_t (* ib_test_dso_create_fn_t)(
    ib_test_util_dso_data_t       **dso_data,
    ib_mm_t                         mm,
    int                             num
);
typedef ib_status_t (* ib_test_dso_destroy_fn_t)(
    ib_test_util_dso_data_t        *dso_data
);

typedef ib_status_t (* ib_test_dso_setnum_fn_t)(
    ib_test_util_dso_data_t        *dso_data,
    int                             num
);
typedef ib_status_t (* ib_test_dso_getnum_fn_t)(
    const ib_test_util_dso_data_t  *dso_data,
    int                            *num
);

typedef ib_status_t (* ib_test_dso_setstr_fn_t)(
    ib_test_util_dso_data_t        *dso_data,
    const char                     *str
);
typedef ib_status_t (* ib_test_dso_getstr_fn_t)(
    const ib_test_util_dso_data_t  *dso_data,
    const char                    **str
);

typedef struct ib_test_util_dso_fns_t
{
    ib_test_dso_create_fn_t   fn_create;
    ib_test_dso_destroy_fn_t  fn_destroy;
    ib_test_dso_setnum_fn_t   fn_setnum;
    ib_test_dso_getnum_fn_t   fn_getnum;
    ib_test_dso_setstr_fn_t   fn_setstr;
    ib_test_dso_getstr_fn_t   fn_getstr;
} ib_test_util_dso_fns_t;

typedef ib_status_t (* ib_test_dso_getfns_fn_t)(
    ib_test_util_dso_fns_t        **fns
);

#endif
