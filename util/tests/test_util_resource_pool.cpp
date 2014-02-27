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
/// @brief IronBee --- Array Test Functions
///
/// @author Brian Rectanus <brectanus@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "ironbee_config_auto.h"

#include <ironbee/mm.h>
#include <ironbee/mm_mpool.h>
#include <ironbee/resource_pool.h>

#include "gtest/gtest.h"

namespace {
extern "C" {
    //! The resource we are going to build and test the resource pool with.
    struct resource_t {
        int preuse;
        int postuse;
        int use;
        int destroy;
    };
    typedef struct resource_t resource_t;

    //! Callback data for resource tests.
    struct cbdata_t {
        ib_mm_t mm;
    };
    typedef struct cbdata_t cbdata_t;

    ib_status_t create_fn(void *resource, void *data) {
        cbdata_t *cbdata = reinterpret_cast<cbdata_t *>(data);
        resource_t *tmp_r = reinterpret_cast<resource_t *>(
            ib_mm_calloc(cbdata->mm, sizeof(*tmp_r), 1));
        *(resource_t **)resource = tmp_r;
        return IB_OK;
    }

    void destroy_fn(void *resource, void *data) {
        resource_t *r = reinterpret_cast<resource_t *>(resource);
        ++(r->destroy);
    }
    void preuse_fn(void *resource, void *data) {
        resource_t *r = reinterpret_cast<resource_t *>(resource);
        ++(r->preuse);
    }
    ib_status_t postuse_fn(void *resource, void *data) {
        resource_t *r = reinterpret_cast<resource_t *>(resource);
        ++(r->postuse);
        return (r->postuse >= 5)? IB_EINVAL : IB_OK;
    }
} /* Close extern "C" */

} /* Close anonymous namespace. */

class ResourcePoolTest : public ::testing::Test {
public:
    virtual void SetUp()
    {
        ASSERT_EQ(IB_OK, ib_mpool_create(&m_mp, "ResourcePoolTest", NULL));
        m_cbdata.mm = ib_mm_mpool(m_mp);
        void *cbdata = reinterpret_cast<void *>(&m_cbdata);
        ASSERT_EQ(IB_OK, ib_resource_pool_create(
            &m_rp,
            ib_mm_mpool(m_mp),
            1,
            10,
            &create_fn,
            cbdata,
            &destroy_fn,
            cbdata,
            &preuse_fn,
            cbdata,
            &postuse_fn,
            cbdata
        ));
    }

    virtual void TearDown()
    {
        ib_mpool_release(m_mp);
    }
protected:
    cbdata_t m_cbdata;
    ib_mpool_t *m_mp;
    ib_resource_pool_t *m_rp;
};

TEST_F(ResourcePoolTest, create) {
    ASSERT_TRUE(m_mp);
    ASSERT_TRUE(m_rp);
}

TEST_F(ResourcePoolTest, get_release) {
    ib_resource_t *ib_r;
    resource_t *r;

    ASSERT_EQ(IB_OK, ib_resource_acquire(m_rp, &ib_r));

    r = reinterpret_cast<resource_t *>(ib_resource_get(ib_r));
    ASSERT_TRUE(r);
    ASSERT_EQ(1U, ib_resource_use_get(ib_r));
    ASSERT_EQ(1, r->preuse);
    ASSERT_EQ(0, r->use);
    ASSERT_EQ(0, r->postuse);
    ASSERT_EQ(0, r->destroy);
    ++(r->use);

    ASSERT_EQ(IB_OK, ib_resource_release(ib_r));
    ASSERT_EQ(1, r->preuse);
    ASSERT_EQ(1, r->use);
    ASSERT_EQ(1, r->postuse);
    ASSERT_EQ(0, r->destroy);

    ASSERT_EQ(IB_OK, ib_resource_acquire(m_rp, &ib_r));
    ASSERT_EQ(2U, ib_resource_use_get(ib_r));
    ASSERT_EQ(IB_OK, ib_resource_release(ib_r));
    ASSERT_EQ(2, r->preuse);
    ASSERT_EQ(2, r->postuse);
    ASSERT_EQ(0, r->destroy);

    ASSERT_EQ(IB_OK, ib_resource_acquire(m_rp, &ib_r));
    ASSERT_EQ(3U, ib_resource_use_get(ib_r));
    ASSERT_EQ(IB_OK, ib_resource_release(ib_r));
    ASSERT_EQ(3, r->preuse);
    ASSERT_EQ(3, r->postuse);
    ASSERT_EQ(0, r->destroy);

    ASSERT_EQ(IB_OK, ib_resource_acquire(m_rp, &ib_r));
    ASSERT_EQ(4U, ib_resource_use_get(ib_r));
    ASSERT_EQ(IB_OK, ib_resource_release(ib_r));
    ASSERT_EQ(4, r->preuse);
    ASSERT_EQ(4, r->postuse);
    ASSERT_EQ(0, r->destroy);

    /* FIXME - destroy after 5. */

    ASSERT_EQ(IB_OK, ib_resource_acquire(m_rp, &ib_r));
    ASSERT_EQ(5U, ib_resource_use_get(ib_r));
    ASSERT_EQ(IB_OK, ib_resource_release(ib_r));
    ASSERT_EQ(0U, ib_resource_use_get(ib_r));
    ASSERT_EQ(5, r->preuse);
    ASSERT_EQ(5, r->postuse);
    ASSERT_EQ(1, r->destroy);

    /* Get an return a resource. This should be no and NOT change r. */
    ASSERT_EQ(IB_OK, ib_resource_acquire(m_rp, &ib_r));
    ASSERT_EQ(IB_OK, ib_resource_release(ib_r));
    ASSERT_EQ(1U, ib_resource_use_get(ib_r));
    ASSERT_EQ(5, r->preuse);
    ASSERT_EQ(5, r->postuse);
    ASSERT_EQ(1, r->destroy);

    /* Get the new r and check it. */
    r = reinterpret_cast<resource_t *>(ib_resource_get(ib_r));
    ASSERT_EQ(1, r->preuse);
    ASSERT_EQ(0, r->use);
    ASSERT_EQ(1, r->postuse);
    ASSERT_EQ(0, r->destroy);
}

TEST_F(ResourcePoolTest, limit_reached) {
    ib_resource_t *ib_r[11];

    for (int i = 0; i < 10; ++i) {
        ASSERT_EQ(IB_OK, ib_resource_acquire(m_rp, &ib_r[i]));
    }

    /* Fail to get the 11th resource. */
    ASSERT_EQ(IB_DECLINED, ib_resource_acquire(m_rp, &ib_r[10]));

    /* Return one, and get one. */
    ASSERT_EQ(IB_OK, ib_resource_release(ib_r[0]));
    ASSERT_EQ(IB_OK, ib_resource_acquire(m_rp, &ib_r[0]));

    /* Return them all. */
    for (int i = 0; i < 10; ++i) {
        ASSERT_EQ(IB_OK, ib_resource_release(ib_r[i]));
    }
}
