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
 * @brief IronBee --- Sting Assembly Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbee/mm_mpool.h>
#include <ironbee/string_assembly.h>
#include <ironbee/type_convert.h>

#include <ironbeepp/memory_pool.hpp>

#include "gtest/gtest.h"

using IronBee::ScopedMemoryPool;
using IronBee::MemoryPool;

using namespace std;

TEST(TestStringAssembly, Basic)
{
    ScopedMemoryPool smp;
    MemoryPool mp(smp);

    ib_status_t rc;
    ib_sa_t *sa;

    rc = ib_sa_begin(&sa);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(sa);

    rc = ib_sa_append(sa, "foo", 3);
    ASSERT_EQ(IB_OK, rc);
    rc = ib_sa_append(sa, "bar", 3);
    ASSERT_EQ(IB_OK, rc);
    rc = ib_sa_append(sa, "baz", 3);
    ASSERT_EQ(IB_OK, rc);

    const char *s;
    size_t s_length;

    rc = ib_sa_finish(&sa, &s, &s_length, ib_mm_mpool(mp.ib()));
    ASSERT_EQ(IB_OK, rc);

    EXPECT_EQ("foobarbaz", string(s, s_length));
    EXPECT_FALSE(sa);
}
