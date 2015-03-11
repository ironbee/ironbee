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

#include <ironbee/predicate/dag.hpp>

#include <boost/function_output_iterator.hpp>
#include <boost/type_traits/is_const.hpp>

#include <list>
#include <set>
#include <vector>

namespace IronBee {
namespace Predicate {

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
 * Breadth first search of all ancestors of a set of nodes.
 *
 * If iterator has a @ref node_cp value_type then, @ref node_cp's will be
 * written to the output iterator, otherwise, @ref node_p's will be.
 *
 * @tparam InputIterator  Type of @a begin and @a in.  Must be input iterator.
 * @tparam OutputIterator Type of @a out.  Must be output iterator.
 * @param[in] begin Beginning of input sequence.
 * @param[in] end   End of input sequence.
 * @param[in] out   Output iterator to write ancestors to.
 * @throw IronBee::einval if any node is singular.
 **/
template <typename InputIterator, typename OutputIterator>
void bfs_up(
    InputIterator  begin,
    InputIterator  end,
    OutputIterator out
);

/**
 * Breadth first search of all descendants of @a which.
 *
 * As bfs_up() above, but searches children instead of ancestors.
 *
 * @tparam OutputIterator Type of @a out.  Must be output iterator.
 * @param[in] which Base of descendant tree.
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
 * @param[in] which Base of descendant tree.
 * @param[in] out   Output iterator to write descendants to.
 * @throw IronBee::einval if @a which is singular.
 **/
template <typename OutputIterator>
void bfs_down(const node_p& which, OutputIterator out);

/**
 * Breadth first search of all children of a set of nodes.
 *
 * If iterator has a @ref node_cp value_type then, @ref node_cp's will be
 * written to the output iterator, otherwise, @ref node_p's will be.
 *
 * @tparam InputIterator  Type of @a begin and @a in.  Must be input iterator.
 * @tparam OutputIterator Type of @a out.  Must be output iterator.
 * @param[in] begin Beginning of input sequence.
 * @param[in] end   End of input sequence.
 * @param[in] out   Output iterator to write children to.
 * @throw IronBee::einval if any node is singular.
 **/
template <typename InputIterator, typename OutputIterator>
void bfs_down(
    InputIterator  begin,
    InputIterator  end,
    OutputIterator out
);

// Implementation

/// @cond internal
namespace Impl {

/**
 * Write a @ref node_cp to an output iterator as a @ref node_p if needed.
 *
 * bfs() only needs @ref node_cp's until it actually writes them to the
 * iterator.  To handle the mutable versions of bfs_up() and bfs_down(), the
 * non-mutable versions are used, passing the result through this helper to
 * remove the const.  To preserve const correctness, only the mutable
 * versions of bfs_up() and bfs_down() are allowed to instantiate this class.
 *
 * @tparam NeedDeconst if true, remove const, otherwise pass through.
 * @tparam OutputIterator Type of output iterator to write @ref node_p to.
 **/
template <bool NeedDeconst, typename OutputIterator>
class bfs_deconst
{
private:
    template <typename I, typename O>
    friend void Predicate::bfs_up(I, I, O);
    template <typename I, typename O>
    friend void Predicate::bfs_down(I, I, O);

    /**
     * Constructor.
     *
     * @param[in] out Output iterator to write to.
     **/
    explicit
    bfs_deconst(OutputIterator out) : m_out(out) {}

    //! Iterator to write to.
    OutputIterator m_out;

public:
    /**
     * Pass through.
     *
     * @param[in] node Node to write to output iterator.
     **/
    void operator()(const node_cp& node)
    {
        *m_out++ = node;
    }
};

/**
 * Write a @ref node_cp to an output iterator as a @ref node_p.
 *
 * bfs() only needs @ref node_cp's until it actually writes them to the
 * iterator.  To handle the mutable versions of bfs_up() and bfs_down(), the
 * non-mutable versions are used, passing the result through this helper to
 * remove the const.  To preserve const correctness, only the mutable
 * versions of bfs_up() and bfs_down() are allowed to instantiate this class.
 *
 * @tparam OutputIterator Type of output iterator to write @ref node_p to.
 **/
template <typename OutputIterator>
class bfs_deconst<true, OutputIterator>
{
private:
    template <typename I, typename O>
    friend void Predicate::bfs_up(I, I, O);
    template <typename I, typename O>
    friend void Predicate::bfs_down(I, I, O);

    /**
     * Constructor.
     *
     * @param[in] out Output iterator to write to.
     **/
    explicit
    bfs_deconst(OutputIterator out) : m_out(out) {}

    //! Iterator to write to.
    OutputIterator m_out;

public:
    /**
     * Cast @a node to a @ref node_p and write to output iterator.
     *
     * @param[in] node Node to write, as @ref node_p, to output iterator.
     **/
    void operator()(const node_cp& node)
    {
        *m_out++ = boost::const_pointer_cast<Node>(node);
    }
};

//! Tag indicating use of Node::parents().
struct bfs_up_tag {};
//! Tag indicating use of Node::children().
struct bfs_down_tag {};

//! Append @c node_cp's to parents of @a which to @a list.
void bfs_append_list(
    node_clist_t& list,
    const node_cp&      which,
    bfs_up_tag
);

//! Append @c node_cp's to children of @a which to @a list.
void bfs_append_list(
    node_clist_t& list,
    const node_cp&      which,
    bfs_down_tag
);

/**
 * Generic breadth first search routine.
 *
 * Every bfs method ultimately calls this routine.
 *
 * @tparam Direction      Direction to search.  Must be @ref bfs_up_tag or
 *                        @ref bfs_down_tag.
 * @tparam InputIterator Type of @a begin and @a end.
 * @tparam OutputIterator Type of @a out.
 * @param[in] begin Beginning of nodes to start from.
 * @param[in] end   End of nodes to start from.
 * @param[in] out   Where to write nodes to as visited.
 * @throw IronBee::einval if any node is singular.
 **/
template <typename Direction, typename InputIterator, typename OutputIterator>
void bfs(
    InputIterator  begin,
    InputIterator  end,
    OutputIterator out
)
{
    int mark = rand();

    node_clist_t todo(begin, end);
    while (! todo.empty()) {
        node_cp n = todo.front();
        todo.pop_front();

        if (! n) {
            BOOST_THROW_EXCEPTION(
                IronBee::einval() << errinfo_what(
                    "Cannot do breadth first search on singular node."
                )
            );
        }

        /* Mark the node. If return is false, we've visited it before.
         * That is to say, the mark is already set. */
        if (! n->mark(mark)) {
            continue;
        }

        *out++ = n;

        bfs_append_list(todo, n, Direction());
    }
}

}
/// @endcond

template <typename OutputIterator>
void bfs_up(const node_cp& which, OutputIterator out)
{
    std::vector<node_cp> input;
    input.push_back(which);
    bfs_up(input.begin(), input.end(), out);
}

template <typename OutputIterator>
void bfs_up(const node_p& which, OutputIterator out)
{
    std::vector<node_p> input;
    input.push_back(which);
    bfs_up(input.begin(), input.end(), out);
}

template <typename InputIterator, typename OutputIterator>
void bfs_up(
    InputIterator  begin,
    InputIterator  end,
    OutputIterator out
)
{
    typedef Impl::bfs_deconst<
        ! boost::is_const<
            typename InputIterator::value_type::element_type
        >::value,
        OutputIterator
    > helper_t;
    Impl::bfs<Impl::bfs_up_tag>(
        begin, end,
        boost::function_output_iterator<helper_t>(helper_t(out))
    );
}

template <typename OutputIterator>
void bfs_down(const node_cp& which, OutputIterator out)
{
    std::vector<node_cp> input;
    input.push_back(which);
    bfs_down(input.begin(), input.end(), out);
}

template <typename OutputIterator>
void bfs_down(const node_p& which, OutputIterator out)
{
    std::vector<node_p> input;
    input.push_back(which);
    bfs_down(input.begin(), input.end(), out);
}

template <typename InputIterator, typename OutputIterator>
void bfs_down(
    InputIterator  begin,
    InputIterator  end,
    OutputIterator out
)
{
    typedef Impl::bfs_deconst<
        ! boost::is_const<
            typename InputIterator::value_type::element_type
        >::value,
        OutputIterator
    > helper_t;
    Impl::bfs<Impl::bfs_down_tag>(
        begin, end,
        boost::function_output_iterator<helper_t>(helper_t(out))
    );
}

} // Predicate
} // IronBee

#endif
