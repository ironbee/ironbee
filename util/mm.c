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
 * @brief IronBee --- Memory Manager Implementation
 *
 * See i_mm_t for details.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 * @nosubgrouping
 */

#include "ironbee_config_auto.h"

#include <ironbee/mm.h>

#include <string.h>
#include <strings.h>

void *ib_mm_alloc(
    ib_mm_t mm,
    size_t  size
)
{
    return mm.alloc(size, mm.alloc_data);
}

void ib_mm_register_cleanup(
    ib_mm_t             mm,
    ib_mm_cleanup_fn_t  fn,
    void               *fndata
)
{
    mm.register_cleanup(fn, fndata, mm.register_cleanup_data);
}

void *ib_mm_calloc(ib_mm_t mm, size_t count, size_t size)
{
    void *mem = ib_mm_alloc(mm, count*size);

    if (mem != NULL) {
        bzero(mem, count*size);
    }

    return mem;
}

char *ib_mm_strdup(ib_mm_t mm, const char *src)
{
    const size_t size = strlen(src) + 1;
    return ib_mm_memdup(mm, (void *)src, size);
}

void *ib_mm_memdup(ib_mm_t mm, const void *src, size_t size)
{
    void *mem = ib_mm_alloc(mm, size);
    if (mem != NULL) {
        memcpy(mem, src, size);
    }
    return mem;
}

char *ib_mm_memdup_to_str(ib_mm_t mm, const void *src, size_t size)
{
    void *mem = ib_mm_alloc(mm, size + 1 );
    if (mem != NULL) {
        memcpy(mem, src, size);
        ((char *)mem)[size] = '\0';
    }
    return mem;
}
