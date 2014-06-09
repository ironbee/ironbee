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
/// @brief IronBee --- Vector Test Functions
///
/// @author Sam Baskinger <sbaskinger@qualys.com>
//////////////////////////////////////////////////////////////////////////////
#include "ironbee_config_auto.h"

#include <ironbee/mm_mpool.h>
#include <ironbee/queue.h>

#include "gtest/gtest.h"

#include <stdexcept>

/* Lots of string constants to push and pop from the queue. */
namespace {
    const char *s01 = "s01";
    const char *s02 = "s02";
    const char *s03 = "s03";
    const char *s04 = "s04";
    const char *s05 = "s05";
    const char *s06 = "s06";
    const char *s07 = "s07";
    const char *s08 = "s08";
    const char *s09 = "s09";
    const char *s10 = "s10";

    const char *s[] = {s01, s02, s03, s04, s05, s06, s07, s08, s09, s10, NULL};
}

class QueueTest : public ::testing::Test {
public:
    virtual void SetUp() {
        ASSERT_EQ(IB_OK, ib_mpool_create(&m_mp, "QueueTest", NULL));
        ASSERT_EQ(IB_OK, ib_queue_create(&m_q, ib_mm_mpool(m_mp), IB_QUEUE_NONE));
    }
    virtual void TearDown() {
        ib_mpool_release(m_mp);
    }
    virtual ~QueueTest() {
    }
protected:
    ib_mpool_t *m_mp;
    ib_queue_t *m_q;
};

TEST_F(QueueTest, Init) {
    ASSERT_FALSE(NULL == m_mp);
    ASSERT_FALSE(NULL == m_q);
}

TEST_F(QueueTest, SetGet) {
    void *v;
    ASSERT_EQ(IB_OK, ib_queue_push_back(m_q, const_cast<char *>(s01)));
    ASSERT_EQ(1U, ib_queue_size(m_q));

    ASSERT_EQ(IB_OK, ib_queue_push_back(m_q, const_cast<char *>(s02)));
    ASSERT_EQ(2U, ib_queue_size(m_q));

    ASSERT_EQ(IB_OK, ib_queue_peek(m_q, &v));
    ASSERT_EQ(s01, v);

    ASSERT_EQ(IB_OK, ib_queue_get(m_q, 0, &v));
    ASSERT_EQ(s01, v);

    ASSERT_EQ(IB_OK, ib_queue_get(m_q, 1, &v));
    ASSERT_EQ(s02, v);
}

TEST_F(QueueTest, PushBack) {
    ASSERT_EQ(IB_OK, ib_queue_reserve(m_q, 100));

    for (int i = 0; s[i] != NULL; ++i) {
        ASSERT_EQ(IB_OK, ib_queue_push_back(m_q, const_cast<char *>(s[i])));
    }

    for (int i = 0; s[i] != NULL; ++i) {
        void *v;
        ASSERT_EQ(IB_OK, ib_queue_get(m_q, i, &v));
        ASSERT_EQ(s[i], v);
    }
}

TEST_F(QueueTest, PushBackResize) {
    ASSERT_EQ(IB_OK, ib_queue_reserve(m_q, 2));

    for (int i = 0; s[i] != NULL; ++i) {
        ASSERT_EQ(IB_OK, ib_queue_push_back(m_q, const_cast<char *>(s[i])));

        void *v;
        ASSERT_EQ(IB_OK, ib_queue_get(m_q, i, &v));
        ASSERT_STREQ(s[i], reinterpret_cast<char *>(v));
    }

    for (int i = 0; s[i] != NULL; ++i) {
        void *v;
        ASSERT_EQ(IB_OK, ib_queue_get(m_q, i, &v));
        ASSERT_STREQ(s[i], reinterpret_cast<char *>(v));
    }
}

TEST_F(QueueTest, PushBackOffset) {
    ASSERT_EQ(IB_OK, ib_queue_reserve(m_q, 4));

    /* Move the head offset by pop_front. */
    void *v;
    ASSERT_EQ(IB_OK, ib_queue_push_back(m_q, NULL));
    ASSERT_EQ(IB_OK, ib_queue_push_back(m_q, NULL));
    ASSERT_EQ(IB_OK, ib_queue_push_back(m_q, const_cast<char *>(s01)));
    ASSERT_EQ(IB_OK, ib_queue_push_back(m_q, const_cast<char *>(s02)));
    ASSERT_EQ(IB_OK, ib_queue_pop_front(m_q, &v));
    ASSERT_EQ(IB_OK, ib_queue_pop_front(m_q, &v));

    /* Head is now at offset 2. */

    ASSERT_EQ(IB_OK, ib_queue_get(m_q, 0, &v));
    ASSERT_STREQ(s01, reinterpret_cast<char *>(v));

    ASSERT_EQ(IB_OK, ib_queue_get(m_q, 1, &v));
    ASSERT_STREQ(s02, reinterpret_cast<char *>(v));

    /* Push the rest. */
    for (int i = 2; s[i] != NULL; ++i) {
        ASSERT_EQ(IB_OK, ib_queue_push_back(m_q, const_cast<char *>(s[i])));
        ASSERT_EQ(IB_OK, ib_queue_get(m_q, i, &v));
        ASSERT_STREQ(s[i], reinterpret_cast<char *>(v));
    }

    for (int i = 0; s[i] != NULL; ++i) {
        ASSERT_EQ(IB_OK, ib_queue_get(m_q, i, &v));
        ASSERT_STREQ(s[i], reinterpret_cast<char *>(v));
    }
}

TEST_F(QueueTest, GetEinval) {
    void *v;
    ASSERT_EQ(IB_EINVAL, ib_queue_get(m_q, 100, &v));
}

TEST_F(QueueTest, SetEinval) {
    void *v;
    ASSERT_EQ(IB_EINVAL, ib_queue_set(m_q, 100, &v));
}

TEST_F(QueueTest, PopFrontEmpty) {
    void *v;
    ASSERT_EQ(IB_ENOENT, ib_queue_pop_front(m_q, &v));
}

TEST_F(QueueTest, PopBackEmpty) {
    void *v;
    ASSERT_EQ(IB_ENOENT, ib_queue_pop_back(m_q, &v));
}
