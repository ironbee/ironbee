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
 * @brief Predicate --- Transform Graph Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <predicate/transform_graph.hpp>

#include <predicate/standard_boolean.hpp>
#include <predicate/parse.hpp>
#include <predicate/merge_graph.hpp>
#include "parse_fixture.hpp"

#include "gtest/gtest.h"

using namespace IronBee::Predicate;
using namespace std;

class TestTransformGraph :
    public ::testing::Test,
    public ParseFixture
{
protected:
    void SetUp()
    {
        Standard::load_boolean(factory());
        factory().add("A", &create);
        factory().add("B", &create);
    }

    bool transform_graph_once(MergeGraph& g) const
    {
        Reporter reporter;
        bool result = false;

        result = transform_graph(reporter, g, m_factory);
        if (reporter.num_errors() || reporter.num_warnings()) {
            throw runtime_error("Expected no errors/warnings.");
        }

        return result;
    }

    void transform_graph_completely(MergeGraph& g) const
    {
        bool result = true;
        while (result) {
            result = transform_graph_once(g);
        }
    }
};

TEST_F(TestTransformGraph, Simple)
{
    MergeGraph g;
    node_p a = parse("(not (not (false)))");
    size_t a_i = g.add_root(a);

    EXPECT_TRUE(g.write_validation_report(cerr));
    EXPECT_TRUE(transform_graph_once(g));
    EXPECT_TRUE(g.write_validation_report(cerr));
    EXPECT_EQ("[]", g.root(a_i)->to_s());
    EXPECT_FALSE(transform_graph_once(g));
    EXPECT_TRUE(g.write_validation_report(cerr));
}
