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
 * @brief IronBee - Utility DSO Functions
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>

#include <ironbee/util.h>

#include "ironbee_util_private.h"

ib_status_t DLL_PUBLIC ib_dso_open(ib_dso_t **dso,
                                   const char *file,
                                   ib_mpool_t *pool)
{
    IB_FTRACE_INIT(ib_dso_open);
    void *handle;

    /// @todo Probably need to do this portably someday

    handle = dlopen(file, RTLD_LAZY);
    if (handle == NULL) {
        ib_util_log_error(1, "%s", dlerror());
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    *dso = ib_mpool_alloc(pool, sizeof(**dso));
    if (*dso == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    (*dso)->mp = pool;
    (*dso)->handle = handle;

    IB_FTRACE_RET_STATUS(IB_OK);
}


ib_status_t DLL_PUBLIC ib_dso_close(ib_dso_t *dso)
{
    IB_FTRACE_INIT(ib_dso_close);
    if (dso == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    if (dlclose(dso->handle) != 0) {
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}


ib_status_t DLL_PUBLIC ib_dso_sym_find(ib_dso_t *dso,
                                       const char *name,
                                       ib_dso_sym_t **sym)
{
    IB_FTRACE_INIT(ib_dso_sym_find);
    if (dso == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    *sym = dlsym(dso->handle, name);
    if (*sym == NULL) {
        IB_FTRACE_RET_STATUS(IB_ENOENT);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

