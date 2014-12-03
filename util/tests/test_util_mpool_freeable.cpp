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
 * @brief IronBee --- Memory Pool Freeable Tests
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include "ironbee_config_auto.h"
#include "gtest/gtest.h"

#include <ironbee/mpool_freeable.h>

TEST(MpoolFreeable, CreateDestroy) {
    ib_mpool_freeable_t *mp;

    ASSERT_EQ(IB_OK, ib_mpool_freeable_create(&mp));

    ib_mpool_freeable_destroy(mp);
}

TEST(MpoolFreeable, Alloc) {
    ib_mpool_freeable_t *mp;
    char                *mysegment;

    ASSERT_EQ(IB_OK, ib_mpool_freeable_create(&mp));

    mysegment = reinterpret_cast<char *>(
        ib_mpool_freeable_alloc(mp, sizeof(char) * 10));

    for (int i = 0; i < 10; ++i) {
        mysegment[i] = 'h';
    }

    ib_mpool_freeable_destroy(mp);
}

TEST(MpoolFreeable, AllocFree) {
    ib_mpool_freeable_t *mp;
    char                *mysegment;

    ASSERT_EQ(IB_OK, ib_mpool_freeable_create(&mp));

    mysegment = reinterpret_cast<char *>(
        ib_mpool_freeable_alloc(mp, sizeof(char) * 10));

    for (int i = 0; i < 10; ++i) {
        mysegment[i] = 'h';
    }

    ib_mpool_freeable_free(mp, mysegment);

    ib_mpool_freeable_destroy(mp);
}

TEST(MpoolFreeable, FreeFailure) {
    ib_mpool_freeable_t *mp;
    char                *mysegment;

    ASSERT_EQ(IB_OK, ib_mpool_freeable_create(&mp));

    /* Allocate a  big block. */
    mysegment = reinterpret_cast<char *>(calloc(2, 1024));

    /* Sould not break because we check the preceeding bytes. */
    ib_mpool_freeable_free(mp, mysegment+1024);

    free(mysegment);

    ib_mpool_freeable_destroy(mp);
}

TEST(MpoolFreeable, FreeFromWrongMpool) {
    ib_mpool_freeable_t *mp1;
    ib_mpool_freeable_t *mp2;
    char                *mysegment;

    ASSERT_EQ(IB_OK, ib_mpool_freeable_create(&mp1));
    ASSERT_EQ(IB_OK, ib_mpool_freeable_create(&mp2));

    mysegment = reinterpret_cast<char *>(
        ib_mpool_freeable_alloc(mp1, sizeof(char) * 10000));

    ib_mpool_freeable_free(mp2, mysegment);
    ib_mpool_freeable_free(mp1, mysegment);

    ib_mpool_freeable_destroy(mp1);
    ib_mpool_freeable_destroy(mp2);
}

//! Increment the int pointer passed in.
void inc_int_callback(void *cbdata) {
    ++(*reinterpret_cast<int *>(cbdata));
}

TEST(MpoolFreeable, Callbacks) {
    ib_mpool_freeable_t *mp;
    void                *mysegment;
    int                  segment_count = 0;
    int                  pool_count = 0;

    ASSERT_EQ(IB_OK, ib_mpool_freeable_create(&mp));

    mysegment = ib_mpool_freeable_alloc(mp, 10);

    ASSERT_EQ(
        IB_OK,
        ib_mpool_freeable_alloc_register_cleanup(
            mp,
            mysegment,
            inc_int_callback,
            &segment_count
        )
    );

    ASSERT_EQ(
        IB_OK,
        ib_mpool_freeable_register_cleanup(
            mp,
            inc_int_callback,
            &pool_count
        )
    );

    ASSERT_EQ(IB_OK, ib_mpool_freeable_ref(mp, mysegment));
    ib_mpool_freeable_free(mp, mysegment);
    ASSERT_EQ(0, segment_count);
    ASSERT_EQ(0, pool_count);
    ib_mpool_freeable_free(mp, mysegment);
    ASSERT_EQ(1, segment_count);
    ASSERT_EQ(0, pool_count);

    ib_mpool_freeable_destroy(mp);

    ASSERT_EQ(1, segment_count);
    ASSERT_EQ(1, pool_count);
}

TEST(MpoolFreeable, SegCallbacks) {
    ib_mpool_freeable_t         *mp;
    ib_mpool_freeable_segment_t *mysegment;
    int                          segment_count = 0;
    int                          pool_count = 0;

    ASSERT_EQ(IB_OK, ib_mpool_freeable_create(&mp));

    mysegment = ib_mpool_freeable_segment_alloc(mp, 10);

    ASSERT_EQ(
        IB_OK,
        ib_mpool_freeable_segment_register_cleanup(
            mp,
            mysegment,
            inc_int_callback,
            &segment_count
        )
    );

    ASSERT_EQ(
        IB_OK,
        ib_mpool_freeable_register_cleanup(
            mp,
            inc_int_callback,
            &pool_count
        )
    );

    ASSERT_EQ(IB_OK, ib_mpool_freeable_segment_ref(mp, mysegment));
    ib_mpool_freeable_segment_free(mp, mysegment);
    ASSERT_EQ(0, segment_count);
    ASSERT_EQ(0, pool_count);
    ib_mpool_freeable_segment_free(mp, mysegment);
    ASSERT_EQ(1, segment_count);
    ASSERT_EQ(0, pool_count);

    ib_mpool_freeable_destroy(mp);

    ASSERT_EQ(1, segment_count);
    ASSERT_EQ(1, pool_count);
}

TEST(MpoolFreeable, AllocSize0) {
    ib_mpool_freeable_t *mp;
    void                *seg1;
    void                *seg2;
    void                *null = NULL;

    ASSERT_EQ(IB_OK, ib_mpool_freeable_create(&mp));

    seg1 = ib_mpool_freeable_alloc(mp, 0);
    seg2 = ib_mpool_freeable_alloc(mp, 0);

    ASSERT_EQ(
        IB_EINVAL,
        ib_mpool_freeable_alloc_register_cleanup(
            mp,
            seg1,
            inc_int_callback,
            NULL
        )
    );

    ASSERT_NE(null, seg1);
    ASSERT_EQ(seg1, seg2);

    ib_mpool_freeable_free(mp, seg1);
    ib_mpool_freeable_free(mp, seg2);
    ib_mpool_freeable_destroy(mp);
}

TEST(MpoolFreeable, FreeNullOk) {
    ib_mpool_freeable_t *mp;

    ASSERT_EQ(IB_OK, ib_mpool_freeable_create(&mp));

    ASSERT_EQ(
        IB_EINVAL,
        ib_mpool_freeable_alloc_register_cleanup(
            mp,
            NULL,
            inc_int_callback,
            NULL
        )
    );

    ib_mpool_freeable_free(mp, NULL);

    ib_mpool_freeable_destroy(mp);
}

TEST(MpoolFreeable, ManySmallAllocs) {
    ib_mpool_freeable_t *mp;

    ASSERT_EQ(IB_OK, ib_mpool_freeable_create(&mp));

    for (int i = 0; i < 1024; ++i) {
        char *v = (char *)ib_mpool_freeable_alloc(mp, 100);
        ASSERT_TRUE(v != NULL);
        for (int j = 0; j < 100; ++j) {
            v[j] = 'q';
        }
    }

    ib_mpool_freeable_destroy(mp);
}