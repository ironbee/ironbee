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
 * @brief IronBee++ Internals --- Data Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/data.hpp>

#include <ironbeepp/memory_pool_lite.hpp>
#include <ironbeepp/memory_manager.hpp>

#include <ironbee/types.h>

#include "gtest/gtest.h"

#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

struct destruction_registerer
{
    explicit destruction_registerer(bool* flag) : m_flag(flag) {}
    ~destruction_registerer() { *m_flag = true; }

    bool* m_flag;
};

typedef boost::shared_ptr<destruction_registerer> destruction_registerer_p;

TEST(TestData, basic)
{
    using namespace IronBee;

    MemoryPoolLite mp = MemoryPoolLite::create();
    MemoryManager mm(mp);

    bool flag = false;
    destruction_registerer_p it =
        boost::make_shared<destruction_registerer>(&flag);

    void* data;

    data = value_to_data(it, mm.ib());
    ASSERT_TRUE(data);

    destruction_registerer_p other;

    ASSERT_NO_THROW(
        other = data_to_value<destruction_registerer_p>(data)
    );

    ASSERT_EQ(it, other);
    ASSERT_EQ(3, other.use_count());
    ASSERT_EQ(3, it.use_count());
    other.reset();
    ASSERT_EQ(2, it.use_count());
    it.reset();

    ASSERT_THROW(data_to_value<int>(data), IronBee::einval);

    ASSERT_FALSE(flag);

    mp.destroy();

    ASSERT_TRUE(flag);
}

TEST(TestData, NoPool)
{
    using namespace IronBee;

    bool flag = false;
    destruction_registerer_p it =
        boost::make_shared<destruction_registerer>(&flag);

    void* data;

    data = value_to_data(it);
    ASSERT_TRUE(data);

    destruction_registerer_p other;

    ASSERT_NO_THROW(
        other = data_to_value<destruction_registerer_p>(data)
    );

    ASSERT_EQ(it, other);

    IronBee::Internal::ibpp_data_cleanup(data);
}