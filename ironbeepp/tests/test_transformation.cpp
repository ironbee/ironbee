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

#include <ironbeepp/test_fixture.hpp>

#include "../engine/engine_private.h"

#include "gtest/gtest.h"

using namespace IronBee;
using namespace std;

class TestTransformation : public ::testing::Test, public TestFixture
{
};

ConstField test_transform(
    ConstField output,
    ConstField expected_input,
    MemoryManager mm,
    ConstField input
)
{
    EXPECT_TRUE(mm);

    EXPECT_EQ(expected_input, input);

    return output;
}


TEST_F(TestTransformation, basic)
{
    MemoryManager mm = m_engine.main_memory_mm();
    ConstField output = Field::create_null_string(
        mm,
        "foo", 3,
        "Hello World"
    );
    ConstField input = Field::create_null_string(
        mm,
        "foo", 3,
        "FooBarBaz"
    );
    ConstTransformation tfn = ConstTransformation::create(
        mm,
        "test",
        true,
        boost::bind(test_transform, output, input, _1, _2)
    );

    ASSERT_NO_THROW(tfn.register_with(m_engine));

    ConstTransformation other_tfn =
        ConstTransformation::lookup(m_engine, "test");
    EXPECT_EQ(tfn, other_tfn);

    ConstField actual_output = tfn.execute(mm, input);
    EXPECT_EQ(output, actual_output);
}
