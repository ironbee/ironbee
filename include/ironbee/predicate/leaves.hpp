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
 * @brief Predicate --- Leaf related routines.
 *
 * Defines routines to find leaves.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __PREDICATE__LEAVES__
#define __PREDICATE__LEAVES__

#include <ironbee/predicate/bfs.hpp>

namespace IronBee {
namespace Predicate {

//! Is node a list?
struct is_leaf
{
    //! True iff @a node is has no parents.
    bool operator()(const node_cp& node) const
    {
        return node->children().empty();
    }

    //! Overload to avoid unnecessary shared_ptr copies.
    bool operator()(const node_p& node) const
    {
        return node->children().empty();
    }
};

//! Output if leaf.
template <typename OutputIterator>
class output_if_leaf
{
public:
    //! Constructor.
    explicit
    output_if_leaf(OutputIterator out) : m_out(out) {}

    //! Append node if it is a leaf.
    void operator()(const node_cp& node)
    {
        if (is_leaf()(node)) {
            *m_out++ = node;
        }
    }

    //! Overload to avoid unnecessary copies.
    void operator()(const node_p& node)
    {
        if (is_leaf()(node)) {
            *m_out++ = node;
        }
    }

private:
    OutputIterator m_out;
};

//! Make append if leaf.
template <typename OutputIterator>
output_if_leaf<OutputIterator> make_output_if_leaf(OutputIterator out);

/**
 * Find every leaf of a graph.
 *
 * This function utilizes the bfs function and as such writes @ref node_p
 * or @ref node_cp to @a out according to the type of the inputs.
 *
 * @tparam InputIterator  Type of @a begin and @a end.
 * @tparam OutputIterator Type of @a out.
 * @param[in] begin Beginning of input sequence.
 * @param[in] end   End of input sequence.
 * @param[in] out   Output iterator to write leaves to.
 *
 * @sa bfs_up()
 **/
template <typename InputIterator, typename OutputIterator>
void find_leaves(
    InputIterator  begin,
    InputIterator  end,
    OutputIterator out
);

// Implementation

template <typename InputIterator, typename OutputIterator>
void find_leaves(
    InputIterator  begin,
    InputIterator  end,
    OutputIterator out
)
{
    bfs_down(
        begin, end,
        boost::make_function_output_iterator(make_output_if_leaf(out))
    );
}

//! Make append if leaf.
template <typename OutputIterator>
output_if_leaf<OutputIterator> make_output_if_leaf(OutputIterator out)
{
    return output_if_leaf<OutputIterator>(out);
}

} // Predicate
} // IronBee

#endif
