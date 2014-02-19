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

//////////////////////////////////////////////////////////////////////////////
/// @file
/// @brief IronBee --- Simple fixture for Ironbee tests
///
/// @author Nick LeRoy <nleroy@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#ifndef __SIMPLE_FIXTURE_H__
#define __SIMPLE_FIXTURE_H__

#include <ironbee/mpool.h>
#include <ironbee/mm_mpool.h>

#include "gtest/gtest.h"

#include <stdexcept>


class SimpleFixture : public testing::Test
{
public:
    SimpleFixture() : m_pool(NULL)
    {
    }
    virtual ~SimpleFixture( )
    {
        DestroyMemPool( );
    }

    virtual void SetUp()
    {
        CreateMemPool( );
    }

    virtual void TearDown()
    {
        DestroyMemPool( );
    }

    // Memory pool creator / destroyer
    void CreateMemPool( void ) const
    {
        if (m_pool == NULL) {
            ib_status_t rc = ib_mpool_create(&m_pool, NULL, NULL);
            if (rc != IB_OK) {
                throw std::runtime_error("Could not initialize mpool.");
            }
        }
    }
    void DestroyMemPool( void ) const
    {
        if (m_pool != NULL) {
            ib_mpool_destroy(m_pool);
            m_pool = NULL;
        }
    }

    // Memory pool accessors
    const char *AllocError(size_t nelem, size_t size) const
    {
        snprintf(m_error_buf, m_error_bufsize,
                 "Failed to allocate %zd elements of size %zd", nelem, size);
        return m_error_buf;
    }
    const char *AllocError(size_t size) const
    {
        snprintf(m_error_buf, m_error_bufsize,
                 "Failed to allocate size %zd", size);
        return m_error_buf;
    }
    const char *AllocError(const char *str) const
    {
        snprintf(m_error_buf, m_error_bufsize,
                 "Failed to duplicate string of size %zd", strlen(str)+1);
        return m_error_buf;
    }
    ib_mpool_t *MemPool( void ) const
    {
        return m_pool;
    }
    ib_mm_t MM( void ) const
    {
        return ib_mm_mpool(m_pool);
    }
    void *MemPoolAlloc(size_t size) const
    {
        void *p = ib_mpool_alloc(m_pool, size);
        if (p == NULL) {
            throw std::runtime_error(AllocError(size));
        }
        return p;
    }
    void *MemPoolCalloc(size_t nelem, size_t size) const
    {
        void *p = ib_mpool_calloc(m_pool, nelem, size);
        if (p == NULL) {
            throw std::runtime_error(AllocError(nelem, size));
        }
        return p;
    }
    char *MemPoolStrDup(const char *src) const
    {
        char *p = ib_mpool_strdup(m_pool, src);
        if (p == NULL) {
            throw std::runtime_error(AllocError(src));
        }
        return p;
    }
    char *MemPoolMemDupToStr(const void *src, size_t size) const
    {
        char *p = ib_mpool_memdup_to_str(m_pool, src, size);
        if (p == NULL) {
            throw std::runtime_error(AllocError(size+1));
        }
        return p;
    }
    void *MemPoolMemDup(const void *src, size_t size) const
    {
        void *p = ib_mpool_memdup(m_pool, src, size);
        if (p == NULL) {
            throw std::runtime_error(AllocError(size));
        }
        return p;
    }

protected:
    static const size_t m_error_bufsize = 128;
    mutable ib_mpool_t *m_pool;
    mutable char        m_error_buf[m_error_bufsize + 1];
};

#endif /* __SIMPLE_FIXTURE_H__ */
