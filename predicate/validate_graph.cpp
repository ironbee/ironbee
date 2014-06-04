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
 * @brief Predicate --- Validate Graph Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbee/predicate/validate_graph.hpp>

#include <ironbee/predicate/bfs.hpp>
#include <ironbee/predicate/leaves.hpp>
#include <ironbee/predicate/merge_graph.hpp>

#include <boost/bind.hpp>
#include <boost/function_output_iterator.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {

namespace  {

//! Call pre_transform on node.
class validate_graph_helper
{
public:
    validate_graph_helper(validation_e which, reporter_t reporter) :
        m_which(which),
        m_reporter(reporter)
    {
        // nop
    }

    void operator()(const node_cp& n)
    {
        if (m_which == VALIDATE_PRE) {
            n->pre_transform(NodeReporter(m_reporter, n));
        }
        else {
            n->post_transform(NodeReporter(m_reporter, n));
        }
    }

private:
    validation_e m_which;
    reporter_t   m_reporter;
};

}

void validate_graph(
    validation_e      which,
    reporter_t        reporter,
    const MergeGraph& graph
)
{
    node_list_t leaves;

    // find_leaves guarantees no duplicates in output.
    find_leaves(
        graph.roots().first, graph.roots().second,
        back_inserter(leaves)
    );

    bfs_up(
        leaves.begin(), leaves.end(),
        boost::make_function_output_iterator(
            validate_graph_helper(which, reporter)
        )
    );
}

} // Predicate
} // IronBee
