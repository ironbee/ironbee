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
 * @brief Predicate --- Standard Test Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbee/predicate/reporter.hpp>
#include <ironbee/predicate/merge_graph.hpp>
#include <ironbee/predicate/pre_eval_graph.hpp>
#include <ironbee/predicate/validate_graph.hpp>
#include <ironbee/predicate/value.hpp>

#include "standard_test.hpp"

#include <boost/iterator/transform_iterator.hpp>

#include <sstream>

using namespace std;
using namespace IronBee::Predicate;

node_p StandardTest::parse(const std::string& text) const
{
    size_t i = 0;
    return parse_call(text, i, factory());
}

Value StandardTest::eval(node_p n)
{
    MergeGraph g;
    Reporter r;

    size_t i = g.add_root(n);

    validate_graph(VALIDATE_PRE, r, g);
    if (r.num_errors() > 0 || r.num_warnings() > 0) {
        r.write_report(cout);
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << IronBee::errinfo_what(
                "pre_transform() failed."
            )
        );
    }
    pre_eval_graph(r, g, m_engine.main_context());
    if (r.num_errors() > 0 || r.num_warnings() > 0) {
        r.write_report(cout);
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << IronBee::errinfo_what(
                "pre_eval() failed."
            )
        );
    }

    size_t index_limit;
    vector<node_cp> traversal;
    bfs_down(g.root(i), make_indexer(index_limit, traversal));
    GraphEvalState ges(index_limit);
    bfs_down(g.root(i), make_initializer(ges, m_transaction));
    ges.eval(g.root(i), m_transaction);
    return ges.value(g.root(i)->index());
}

string StandardTest::eval(
    const std::string& text
)
{
    return eval(parse(text)).to_s();
}

node_p StandardTest::transform(node_p n) const
{
    MergeGraph G;
    Reporter r;
    size_t i = G.add_root(n);

    validate_graph(VALIDATE_PRE, r, G);
    if (r.num_errors() > 0 || r.num_warnings() > 0) {
        r.write_report(cout);
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << IronBee::errinfo_what(
                "pre_transform() failed."
            )
        );
    }

    n->transform(G, factory(), m_engine.main_context(), NodeReporter(r, n));
    if (r.num_warnings() || r.num_errors()) {
        throw runtime_error("Warnings/Errors during transform.");
    }
    return G.root(i);
}

string StandardTest::transform(const std::string& s) const
{
    return transform(parse(s))->to_s();
}
