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
 * @brief Predicate --- Standard Development Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include "standard_test.hpp"

using namespace IronBee::Predicate;
using namespace std;

class TestStandardDevelopment :
    public StandardTest
{
};

TEST_F(TestStandardDevelopment, p)
{
    EXPECT_EQ("foo", eval_s("(p 'a' 5 'foo')"));
    EXPECT_TRUE(eval_bool("(p 'foo' 5 (cat 'foo' 5))"));

    EXPECT_THROW(eval_bool("(p)"), IronBee::einval);
}

TEST_F(TestStandardDevelopment, sequence)
{
    EXPECT_FALSE(eval_bool("(isFinished (sequence 1))"));

    {
        node_p n = parse("(sequence 1 3)");
        ValueList v;

        n->reset();
        v = eval(n);
        ASSERT_EQ(1UL, v.size());
        ASSERT_FALSE(n->is_finished());
        v = eval(n);
        ASSERT_EQ(2UL, v.size());
        ASSERT_FALSE(n->is_finished());
        v = eval(n);
        ASSERT_EQ(3UL, v.size());
        ASSERT_TRUE(n->is_finished());

        ValueList::const_iterator i = v.begin();
        EXPECT_EQ(1, i->value_as_number());
        ++i;
        EXPECT_EQ(2, i->value_as_number());
        ++i;
        EXPECT_EQ(3, i->value_as_number());
    }

    {
        node_p n = parse("(sequence 3 1 -1)");
        ValueList v;

        n->reset();
        v = eval(n);
        ASSERT_EQ(1UL, v.size());
        ASSERT_FALSE(n->is_finished());
        v = eval(n);
        ASSERT_EQ(2UL, v.size());
        ASSERT_FALSE(n->is_finished());
        v = eval(n);
        ASSERT_EQ(3UL, v.size());
        ASSERT_TRUE(n->is_finished());

        ValueList::const_iterator i = v.begin();
        EXPECT_EQ(3, i->value_as_number());
        ++i;
        EXPECT_EQ(2, i->value_as_number());
        ++i;
        EXPECT_EQ(1, i->value_as_number());
    }

    {
        node_p n = parse("(sequence 1 5 2)");
        ValueList v;

        n->reset();
        v = eval(n);
        ASSERT_EQ(1UL, v.size());
        ASSERT_FALSE(n->is_finished());
        v = eval(n);
        ASSERT_EQ(2UL, v.size());
        ASSERT_FALSE(n->is_finished());
        v = eval(n);
        ASSERT_EQ(3UL, v.size());
        ASSERT_TRUE(n->is_finished());

        ValueList::const_iterator i = v.begin();
        EXPECT_EQ(1, i->value_as_number());
        ++i;
        EXPECT_EQ(3, i->value_as_number());
        ++i;
        EXPECT_EQ(5, i->value_as_number());
    }

    {
        node_p n = parse("(sequence 1)");
        ValueList v;

        n->reset();
        v = eval(n);
        ASSERT_EQ(1UL, v.size());
        ASSERT_FALSE(n->is_finished());
        v = eval(n);
        ASSERT_EQ(2UL, v.size());
        ASSERT_FALSE(n->is_finished());
        v = eval(n);
        ASSERT_EQ(3UL, v.size());
        ASSERT_FALSE(n->is_finished());

        ValueList::const_iterator i = v.begin();
        EXPECT_EQ(1, i->value_as_number());
        ++i;
        EXPECT_EQ(2, i->value_as_number());
        ++i;
        EXPECT_EQ(3, i->value_as_number());
    }

    EXPECT_THROW(eval_bool("(sequence)"), IronBee::einval);
    EXPECT_THROW(eval_bool("(sequence 1 2 3 4)"), IronBee::einval);
    EXPECT_THROW(eval_bool("(sequence 'a')"), IronBee::einval);
    EXPECT_THROW(eval_bool("(sequence 1 'a')"), IronBee::einval);
    EXPECT_THROW(eval_bool("(sequence 1 1 'a')"), IronBee::einval);
}

TEST_F(TestStandardDevelopment, identity)
{
    EXPECT_EQ("foo", eval_s("(identity 'foo')"));
    EXPECT_FALSE(eval_bool("(isFinished (identity (sequence 1)))"));

    EXPECT_THROW(eval_bool("(identity)"), IronBee::einval);
    EXPECT_THROW(eval_bool("(identity 'a' 'b')"), IronBee::einval);
}
