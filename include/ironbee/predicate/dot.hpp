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
 * @brief Predicate --- Output DAG to GraphViz dot.
 *
 * Defines to_dot() which outputs a dot graph of the DAG.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __PREDICATE__DOT__
#define __PREDICATE__DOT__

#include <ironbee/predicate/bfs.hpp>

namespace IronBee {
namespace Predicate {

/**
 * Default node decorator for to_dot().
 *
 * Labels null and literal nodes by Node::to_s() and call nodes by
 * Call::name().
 *
 * @sa to_dot()
 **/
struct DefaultNodeDecorator
{
    //! Call operator.
    std::string operator()(const node_cp& node) const;
};

//! Type of a node decorator.  See to_dot().
typedef boost::function<std::string(const node_cp&)> dot_node_decorator_t;

/**
 * Output DAG to GraphViz DOT format.
 *
 * @param[in] out            Stream to output to.
 * @param[in] begin          Beginning of sequence of roots of trees fully
 *                           defining the DAG.
 * @param[in] end            End of sequence begun at @a begin.
 * @param[in] node_decorator Function to provide attributes string for every
 *                           node in DAG.
 * @throw IronBee::einval if [begin, end) contains any singular nodes.
 **/
template <typename Iterator>
void to_dot(
    std::ostream& out,
    Iterator begin, Iterator end,
    dot_node_decorator_t node_decorator = DefaultNodeDecorator()
);

/**
 * Output a single root to GraphViz DOT format.
 *
 * @param[in] out            Stream to output to.
 * @param[in] node           Node to output.
 * @param[in] node_decorator Function to provide attributes string for every
 *                           node in DAG.
 * @throw IronBee::einval if [begin, end) contains any singular nodes.
 **/
void to_dot(
    std::ostream& out,
    const node_cp& node,
    dot_node_decorator_t node_decorator = DefaultNodeDecorator()
);

/// @cond internal
namespace Impl {

/**
 * Functional that to_dot() calls for every node.
 **/
struct dot_node_outputer
{
public:
    /**
     * Constructor.
     *
     * @param[in] out            @a out parameter to to_dot()
     * @param[in] node_decorator @a node_decorator parameter to to_dot().
     **/
    dot_node_outputer(
        std::ostream&        out,
        dot_node_decorator_t node_decorator
    );

    //! Output @a n.
    void operator()(const node_cp& node) const;

private:
    //! Stream to output to.
    std::ostream& m_out;
    //! Node decorator to use.
    dot_node_decorator_t m_node_decorator;
};

} // Impl
/// @endcond

// Implementation

template <typename Iterator>
void to_dot(
    std::ostream& out,
    Iterator begin, Iterator end,
    dot_node_decorator_t node_decorator
)
{
    boost::function_output_iterator<Impl::dot_node_outputer>
        out_iter(Impl::dot_node_outputer(out, node_decorator));

    out << "digraph G {" << std::endl;
    out << "  ordering = out;" << std::endl;
    bfs_down(begin, end, out_iter);
    out << "}" << std::endl;
}

} // Predicate
} // IronBee

#endif
