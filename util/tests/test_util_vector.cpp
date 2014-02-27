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

#include <ironbee/vector.h>
#include <ironbee/mm.h>
#include <ironbee/mm_mpool.h>
#include <ironbee/util.h>

#include "gtest/gtest.h"

#include <stdexcept>

class VectorTest : public ::testing::Test {

public:
    virtual void SetUp(){
        ib_status_t rc;

        rc = ib_mpool_create(&m_mp, "Main", NULL);
        if (rc != IB_OK) {
            throw std::runtime_error("Failed to create main memory pool");
        }

        rc = ib_vector_create(&m_vector, ib_mm_mpool(m_mp), 0);
        if (rc != IB_OK) {
            throw std::runtime_error("Failed to create test vector.");
        }

    }

    virtual void TearDown(){
        ib_mpool_destroy(m_mp);
    }
protected:
    ib_mpool_t  *m_mp;
    ib_vector_t *m_vector;
};

TEST_F(VectorTest, Append) {
    ASSERT_EQ(
        IB_OK,
        ib_vector_append(m_vector, "hi", 2));
    ASSERT_EQ(
        IB_OK,
        ib_vector_append(m_vector, "!", 1));
    ASSERT_EQ(3U, m_vector->len);
    ASSERT_EQ(4U, m_vector->size);
    ASSERT_EQ(
        std::string("hi!"),
        std::string(reinterpret_cast<char *>(m_vector->data), 3)
    );
}

TEST_F(VectorTest, Truncate) {
    ASSERT_EQ(
        IB_OK,
        ib_vector_append(m_vector, "hi", 2));
    ASSERT_EQ(IB_OK, ib_vector_truncate(m_vector, 0));
    ASSERT_EQ(0U, m_vector->len);
    ASSERT_EQ(0U, m_vector->size);
    ASSERT_EQ(
        IB_OK,
        ib_vector_append(m_vector, "hi", 2));
}

TEST_F(VectorTest, Resize) {
    ASSERT_EQ(
        IB_OK,
        ib_vector_append(m_vector, "hi", 2));
    ASSERT_EQ(IB_OK, ib_vector_resize(m_vector, 0));
    ASSERT_EQ(0U, m_vector->len);
    ASSERT_EQ(0U, m_vector->size);
    ASSERT_EQ(
        IB_OK,
        ib_vector_append(m_vector, "hi", 2));
}

/////////////////////////////////////////////////////////////////////////////
// Test many sizes that we should not allocate.
/////////////////////////////////////////////////////////////////////////////

class VectorAppendFailsTest :
   public VectorTest,
   public ::testing::WithParamInterface<size_t>
{ };

TEST_P(VectorAppendFailsTest, IB_EINVAL) {
    const char *c = "This buffer is never read.";
    ASSERT_EQ(IB_EINVAL, ib_vector_append(m_vector, c, GetParam()));
}

INSTANTIATE_TEST_CASE_P(
    TooBig,
    VectorAppendFailsTest,
    ::testing::Values(
        static_cast<size_t>(-1),
        static_cast<size_t>(-8096),
        static_cast<size_t>(~0U),
        static_cast<size_t>(((~0U) >> 1U)+1U)
    )
);
