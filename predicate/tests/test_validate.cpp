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
 * @brief Predicate --- Validation Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include "../validate.hpp"

#include "gtest/gtest.h"

using namespace IronBee::Predicate;
using namespace std;

template <size_t N, class Chain = Validate::Base>
struct WarnNTimes : public Chain
{
    virtual void validate(NodeReporter& node_reporter) const
    {
        for (size_t i = 0; i < N; ++i) {
            node_reporter.warn("warning");
        }
        Chain::validate(node_reporter);
    }
};

struct SimpleTest :
    public Validate::Call<SimpleTest>,
    public Validate::NChildren<1,
           WarnNTimes<1,
           WarnNTimes<2
           > > >
{
    typedef Validate::Call<SimpleTest> parent;

    SimpleTest() :
        pre_transform_called(false),
        post_transform_called(false)
    {
        // nop
    }

    virtual string name() const
    {
        return "simple_test";
    }

    virtual void pre_transform(NodeReporter& reporter) const
    {
        pre_transform_called = true;
        parent::pre_transform(reporter);
    }

    virtual void post_transform(NodeReporter& reporter) const
    {
        post_transform_called = true;
        parent::post_transform(reporter);
    }

    mutable bool pre_transform_called;
    mutable bool post_transform_called;

protected:
    virtual Value calculate(Context c)
    {
        return Value();
    }
};

TEST(TestValidate, Simple)
{
    boost::shared_ptr<SimpleTest> n(new SimpleTest());

    {
        Reporter reporter;
        NodeReporter node_reporter(reporter, n);

        ASSERT_EQ(0UL, reporter.num_errors());
        ASSERT_EQ(0UL, reporter.num_warnings());

        n->pre_transform(node_reporter);
        EXPECT_EQ(1UL, reporter.num_errors());
        EXPECT_EQ(3UL, reporter.num_warnings());
        EXPECT_TRUE(n->pre_transform_called);
        EXPECT_FALSE(n->post_transform_called);
    }

    n->add_child(node_p(new Null()));

    {
        Reporter reporter;
        NodeReporter node_reporter(reporter, n);

        ASSERT_EQ(0UL, reporter.num_errors());
        ASSERT_EQ(0UL, reporter.num_warnings());

        n->pre_transform_called = false;
        n->post_transform(node_reporter);
        EXPECT_EQ(0UL, reporter.num_errors());
        EXPECT_EQ(3UL, reporter.num_warnings());
        EXPECT_FALSE(n->pre_transform_called);
        EXPECT_TRUE(n->post_transform_called);
    }
}
