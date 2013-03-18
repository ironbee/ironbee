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
 * @brief Predicate --- Breadth First Search
 *
 * Defines routines to do breadth first searches of ancestors or descendants
 * of nodes.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __PREDICATE__BFS__
#define __PREDICATE__BFS__

#include "dag.hpp"

#include <list>
#include <set>

#include <boost/function_output_iterator.hpp>

namespace IronBee {
namespace Predicate {
namespace DAG {

/**
 * Breadth first search of all ancestors of @a which.
 *
 * This method will output @a which followed by every ancestor @a which in
 * breadth first order.   Ancestors that appear multiple times (possible in
 * DAG) are only output once.  Ancestors are written to @a out as
 * @ref node_cp; pass in a @ref node_p for @a which to get @ref node_p's out
 * instead.  To search for children of @a which, use bfs_down().
 *
 * @tparam OutputIterator Type of @a out.  Must be output iterator.
 * @param[in] which Base of ancestor tree.
 * @param[in] out   Output iterator to write ancestors to.
 * @throw IronBee::einval if @a which is singular.
 **/
template <typename OutputIterator>
void bfs_up(const node_cp& which, OutputIterator out);

/**
 * Breadth first search of all ancestors of @a which (mutable version).
 *
 * As previous, but @ref node_p's are output to @a out.
 *
 * @tparam OutputIterator Type of @a out.  Must be output iterator.
 * @param[in] which Base of ancestor tree.
 * @param[in] out   Output iterator to write ancestors to.
 * @throw IronBee::einval if @a which is singular.
 **/
template <typename OutputIterator>
void bfs_up(const node_p& which, OutputIterator out);

/**
 * Breadth first search of all ancestors of @a which (mutable version).
 *
 * As previous, but @ref node_p's are output to @a out.
 *
 * A shared visited set, @a visited, can be used to do multiple bfs runs over
 * a forest with shared nodes.
 *
 * @tparam OutputIterator Type of @a out.  Must be output iterator.
 * @param[in] which        Base of ancestor tree.
 * @param[in] out          Output iterator to write ancestors to.
 * @param[in, out] visited Set of nodes that have been visited; updated.
 * @throw IronBee::einval if @a which is singular.
 **/
template <typename OutputIterator>
void bfs_up(
    const node_p&      which,
    OutputIterator     out,
    std::set<node_cp>& visited
);

/**
 * Breadth first search of all ancestors of @a which.
 *
 * As previous, but @ref node_p's are output to @a out.
 *
 * A shared visited set, @a visited, can be used to do multiple bfs runs over
 * a forest with shared nodes.
 *
 * @tparam OutputIterator Type of @a out.  Must be output iterator.
 * @param[in] which        Base of ancestor tree.
 * @param[in] out          Output iterator to write ancestors to.
 * @param[in, out] visited Set of nodes that have been visited; updated.
 * @throw IronBee::einval if @a which is singular.
 **/
template <typename OutputIterator>
void bfs_up(
    const node_cp&     which,
    OutputIterator     out,
    std::set<node_cp>& visited
);

/**
 * Breadth first search of all descendants of @a which.
 *
 * As bfs_up() above, but searches children instead of ancestors.
 *
 * @tparam OutputIterator Type of @a out.  Must be output iterator.
 * @param[in] which Base of descndant tree.
 * @param[in] out   Output iterator to write descendants to.
 * @throw IronBee::einval if @a which is singular.
 **/
template <typename OutputIterator>
void bfs_down(const node_cp& which, OutputIterator out);

/**
 * Breadth first search of all descendants of @a which (mutable version).
 *
 * As previous but @ref node_p's are output to @a out.
 *
 * @tparam OutputIterator Type of @a out.  Must be output iterator.
 * @param[in] which Base of descndant tree.
 * @param[in] out   Output iterator to write descendants to.
 * @throw IronBee::einval if @a which is singular.
 **/
template <typename OutputIterator>
void bfs_down(const node_p& which, OutputIterator out);

/**
 * Breadth first search of all descendants of @a which (mutable version).
 *
 * As previous, but @ref node_p's are output to @a out.
 *
 * A shared visited set, @a visited, can be used to do multiple bfs runs over
 * a forest with shared nodes.
 *
 * @tparam OutputIterator Type of @a out.  Must be output iterator.
 * @param[in] which        Base of ancestor tree.
 * @param[in] out          Output iterator to write descendants to.
 * @param[in, out] visited Set of nodes that have been visited; updated.
 * @throw IronBee::einval if @a which is singular.
 **/
template <typename OutputIterator>
void bfs_down(
    const node_p&      which,
    OutputIterator     out,
    std::set<node_cp>& visited
);

/**
 * Breadth first search of all descendants of @a which.
 *
 * As previous, but @ref node_p's are output to @a out.
 *
 * A shared visited set, @a visited, can be used to do multiple bfs runs over
 * a forest with shared nodes.
 *
 * @tparam OutputIterator Type of @a out.  Must be output iterator.
 * @param[in] which        Base of ancestor tree.
 * @param[in] out          Output iterator to write descendants to.
 * @param[in, out] visited Set of nodes that have been visited; updated.
 * @throw IronBee::einval if @a which is singular.
 **/
template <typename OutputIterator>
void bfs_down(
    const node_cp&     which,
    OutputIterator     out,
    std::set<node_cp>& visited
);

// Implementation

/// @cond Impl
namespace Impl {

/**
 * Write a @ref node_cp to an output iterator as a @ref node_p.
 *
 * bfs() only needs @ref node_cp's until it actually writes them to the
 * iterator.  To handle the mutable versions of bfs_up() and bfs_donw(), the
 * non-mutable versions are used, passing the result through this helper to
 * remove the const.  To preserve const correctness, only the mutable
 * versions of bfs_up() and bfs_down() are allowed to instantiate this class.
 *
 * @tparam OutputIterator Type of output iterator to write @ref node_p to.
 **/
template <typename OutputIterator>
class bfs_deconst
{
private:
    template <typename O>
    friend void DAG::bfs_up(const node_p&, O, std::set<node_cp>&);
    template <typename O>
    friend void DAG::bfs_down(const node_p&, O, std::set<node_cp>&);

    /**
     * Constructor.
     *
     * @param[in] out Output iterator to write to.
     **/
    explicit
    bfs_deconst(OutputIterator out) : m_out(out) {}

public:
    /**
     * Cast @a node to a @ref node_p and write to output iterator.
     *
     * @tparam[in] node Node to write, as @ref node_p, to output iterator.
     **/
    void operator()(const node_cp& node)
    {
        *m_out++ = boost::const_pointer_cast<Node>(node);
    }

    //! Iterator to write to.
    OutputIterator m_out;
};

//! Tag indicating use of Node::parents().
struct bfs_up_tag {};
//! Tag indicating use of Node::children().
struct bfs_down_tag {};

//! Append @c node_cp's to parents of @a which to @a list.
void bfs_append_list(
    std::list<node_cp>& list,
    const node_cp&      which,
    bfs_up_tag
)
{
    BOOST_FOREACH(const DAG::weak_node_p& weak_parent, which->parents()) {
        list.push_back(weak_parent.lock());
    }
}

//! Append @c node_cp's to children of @a which to @a list.
void bfs_append_list(
    std::list<node_cp>& list,
    const node_cp&      which,
    bfs_down_tag
)
{
    std::copy(
        which->children().begin(), which->children().end(),
        std::back_inserter(list)
    );
}

/**
 * Generic breadth first search routine.
 *
 * Every bfs method ultimately calls this routine.
 *
 * @tparam OutputIterator Type of @a out.
 * @tparam Direction      Direction to search.  Must be @ref bfs_up_tag or
 *                        @ref bfs_down_tag.
 * @param[in] which Node to start search at.
 * @param[in] out   Output iterator to write to.
 * @param[in, out]  Set of which nodes have been visited to use/update.
 * @throw IronBee::einval if @a which is singular.
 **/
template <typename Direction, typename OutputIterator>
void bfs(
    const node_cp&     which,
    OutputIterator     out,
    std::set<node_cp>& visited
)
{
    if (! which) {
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << errinfo_what(
                "Cannot do breadth first search on singular node."
            )
        );
    }

    std::list<node_cp> todo;

    todo.push_back(which);
    while (! todo.empty()) {
        node_cp n = todo.front();
        todo.pop_front();

        if (visited.count(n)) {
            continue;
        }

        visited.insert(n);
        *out++ = n;

        bfs_append_list(todo, n, Direction());
    }
}

}
/// @endcond

template <typename OutputIterator>
void bfs_up(const node_cp& which, OutputIterator out)
{
    std::set<node_cp> visited;
    bfs_up(which, out, visited);
}

template <typename OutputIterator>
void bfs_up(const node_p& which, OutputIterator out)
{
    std::set<node_cp> visited;
    bfs_up(which, out, visited);
}

template <typename OutputIterator>
void bfs_up(
    const node_p&      which,
    OutputIterator     out,
    std::set<node_cp>& visited
)
{
    typedef Impl::bfs_deconst<OutputIterator> helper_t;
    bfs_up(
        node_cp(which),
        boost::function_output_iterator<helper_t>(helper_t(out)),
        visited
    );
}

template <typename OutputIterator>
void bfs_up(
    const node_cp&     which,
    OutputIterator     out,
    std::set<node_cp>& visited
)
{
    Impl::bfs<Impl::bfs_up_tag>(which, out, visited);
}

template <typename OutputIterator>
void bfs_down(const node_cp& which, OutputIterator out)
{
    std::set<node_cp> visited;
    bfs_down(which, out, visited);
}

template <typename OutputIterator>
void bfs_down(const node_p& which, OutputIterator out)
{
    std::set<node_cp> visited;
    bfs_down(which, out, visited);
}

template <typename OutputIterator>
void bfs_down(
    const node_p&      which,
    OutputIterator     out,
    std::set<node_cp>& visited
)
{
    typedef Impl::bfs_deconst<OutputIterator> helper_t;
    bfs_down(
        node_cp(which),
        boost::function_output_iterator<helper_t>(helper_t(out)),
        visited
    );
}

template <typename OutputIterator>
void bfs_down(
    const node_cp&     which,
    OutputIterator     out,
    std::set<node_cp>& visited
)
{
    Impl::bfs<Impl::bfs_down_tag>(which, out, visited);
}

} // DAG
} // Predicate
} // IronBee

#endif
