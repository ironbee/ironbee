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

#include <ironbee/mm_mpool_lite.h>

#include "gtest/gtest.h"

#include <stdexcept>


class SimpleFixture : public testing::Test
{
public:
    SimpleFixture() : m_pool(NULL)
    {
    }

protected:
    virtual void SetUp()
    {
        ib_status_t rc = ib_mpool_lite_create(&m_pool);
        if (rc != IB_OK) {
            throw std::runtime_error("Could not initialize mpool.");
        }
    }

    virtual void TearDown()
    {
        if (m_pool != NULL) {
            ib_mpool_lite_destroy(m_pool);
            m_pool = NULL;
        }
    }

    ib_mm_t MM() const
    {
        return ib_mm_mpool_lite(m_pool);
    }

private:
    ib_mpool_lite_t *m_pool;
};

#endif /* __SIMPLE_FIXTURE_H__ */
