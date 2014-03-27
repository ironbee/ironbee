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
 * @brief Predicate --- Validate Graph Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <predicate/validate_graph.hpp>

#include <predicate/standard_boolean.hpp>
#include <predicate/parse.hpp>
#include <predicate/merge_graph.hpp>
#include "parse_fixture.hpp"

#include "gtest/gtest.h"

using namespace IronBee::Predicate;
using namespace std;

class TestValidateGraph :
    public ::testing::Test,
    public ParseFixture
{
protected:
    void SetUp()
    {
        Standard::load_boolean(factory());
    }

    //! Returns true iff graph was valid.
    bool validate(validation_e which, MergeGraph& g) const
    {
        Reporter reporter;

        validate_graph(which, reporter, g);
        if (reporter.num_errors() || reporter.num_warnings()) {
            return false;
        }

        return true;
    }
};

TEST_F(TestValidateGraph, Simple)
{
    MergeGraph g;
    node_p n;

    n = parse("(and (or) (true))");
    g.add_root(n);

    {
        Reporter r;
        NodeReporter nr(r, n);
        n->pre_transform(nr);
        EXPECT_EQ(0UL, r.num_warnings());
        EXPECT_EQ(0UL, r.num_errors());
    }
    EXPECT_FALSE(validate(VALIDATE_PRE, g));
}
