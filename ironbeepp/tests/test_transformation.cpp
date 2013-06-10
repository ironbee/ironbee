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
 * @brief IronBee++ Internals --- Transformation Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/transformation.hpp>

#include "fixture.hpp"

#include "../engine/engine_private.h"

#include "gtest/gtest.h"

using namespace IronBee;
using namespace std;

class TestTransformation : public ::testing::Test, public IBPPTestFixture
{
};

ConstField test_transform(
    ConstField output,
    ConstField expected_input,
    Engine     ib,
    MemoryPool mp,
    ConstField input
)
{
    EXPECT_TRUE(ib);
    EXPECT_TRUE(mp);

    EXPECT_EQ(expected_input, input);

    return output;
}


TEST_F(TestTransformation, basic)
{
    MemoryPool pool = m_engine.main_memory_pool();
    ConstField output = Field::create_null_string(
        pool,
        "foo", 3,
        "Hello World"
    );
    ConstField input = Field::create_null_string(
        pool,
        "foo", 3,
        "FooBarBaz"
    );
    ConstTransformation tfn = ConstTransformation::create(
        pool,
        "test",
        true,
        boost::bind(test_transform, output, input, _1, _2, _3)
    );

    ASSERT_NO_THROW(tfn.register_with(m_engine));

    ConstTransformation other_tfn =
        ConstTransformation::lookup(m_engine, "test");
    EXPECT_EQ(tfn, other_tfn);

    ConstField actual_output = tfn.execute(m_engine, pool, input);
    EXPECT_EQ(output, actual_output);
}
