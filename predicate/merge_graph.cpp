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
 * @brief Predicate --- MergeGraph Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "merge_graph.hpp"

#include "bfs.hpp"
#include "dot.hpp"

#include <sstream>

#include <boost/bind.hpp>
#include <boost/function_output_iterator.hpp>
#include <boost/lexical_cast.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {
namespace DAG {

size_t MergeGraph::add_root(node_p& root)
{
    if (! root) {
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << errinfo_what(
                "Cannot add singular root."
            )
        );
    }
    if (! root->parents().empty()) {
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << errinfo_what(
                "Root has parents."
            )
        );

    }

    merge_tree(root); // May change root.

    m_roots.push_back(root);
    size_t index = m_roots.size() - 1;

    m_root_indices[root] = index;
    return index;
}

const node_p& MergeGraph::root(size_t index) const
{
    if (index >= m_roots.size()) {
        BOOST_THROW_EXCEPTION(
            IronBee::enoent() << errinfo_what(
                "Root index too large."
            )
        );
    }

    return m_roots[index];
}

size_t MergeGraph::root_index(const node_cp& root) const
{
    if (! root) {
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << errinfo_what(
                "Cannot search for singular root."
            )
        );
    }
    node_cp known_root = known(root);
    if (! known_root) {
        known_root = root;
    }
    root_indices_t::const_iterator i = m_root_indices.find(known_root);
    if (i == m_root_indices.end()) {
        BOOST_THROW_EXCEPTION(
            IronBee::enoent() << errinfo_what(
                "No such root."
            )
        );
    }
    return i->second;
}

pair<bool, node_p> MergeGraph::learn(const node_p& which)
{
    if (! which) {
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << errinfo_what(
                "Cannot add singular node."
            )
        );
    }
    bool is_new;
    node_by_sexpr_t::iterator i;

    boost::tie(i, is_new) =
        m_node_by_sexpr.insert(make_pair(which->to_s(), which));
    return make_pair(is_new, i->second);
}

bool MergeGraph::unlearn(const node_cp& which)
{
    node_by_sexpr_t::iterator i = m_node_by_sexpr.find(which->to_s());
    if (i != m_node_by_sexpr.end()) {
        m_node_by_sexpr.erase(i);
        return true;
    }
    return false;
}

node_p MergeGraph::known(const node_cp& node) const
{
    if (! node) {
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << errinfo_what(
                "Cannot add singular node."
            )
        );
    }
    node_by_sexpr_t::const_iterator i =
        m_node_by_sexpr.find(node->to_s());
    if (i == m_node_by_sexpr.end()) {
        return node_p();
    }
    else {
        return i->second;
    }
}

void MergeGraph::replace(const node_cp& which, node_p& with)
{
    node_p known_which = known(which);
    if (! known_which) {
        BOOST_THROW_EXCEPTION(
            IronBee::enoent() << errinfo_what(
                "No such subexpression."
            )
        );
    }

    // If with is known, this will change with to point to it.  Otherwise,
    // this will merge in the tree based at with.
    merge_tree(with);

    // Unlearn all subexpressions of known_which and parents.
    bfs_up(
        known_which,
        boost::make_function_output_iterator(
            boost::bind(&MergeGraph::unlearn, this, _1)
        )
    );

    // Replace known_which with with in all parents of known_which.
    // Doing so will update the sexprs of all ancestors of with.
    // As we are holding a node_p to known_which, it and its descendants
    // will stay around long enough for us to unlearn them as necessary
    // later on.
    BOOST_FOREACH(const weak_node_p& weak_parent, known_which->parents()) {
        weak_parent.lock()->replace_child(known_which, with);
    }

    // Learn all new ancestor sexprs.
    bfs_up(
        with,
        boost::make_function_output_iterator(
            boost::bind(&MergeGraph::learn, this, _1)
        )
    );

    // Follow descendants of known_which so long as single parent.
    // When we hit a multiple parent child, we need to remove it from the
    // parent we came from (the desendant of known_which), but should not
    // unlearn its sexpr (it's still in the graph from another parent)
    // and can stop our descent.
    // When known_which has know common subexpressions with other parts of
    // the graph, this is equivalent to unlearning all children.
    // Note that we do not check for multiple visits.  It is important, if a
    // child is reached via multiple paths, to handle all such paths.  E.g.,
    // a child with two paths will, the first time, be removed from one of its
    // parents and the second time unlearn (as it now has a single parent).
    list<node_p> todo;
    todo.push_back(known_which);
    while (! todo.empty()) {
        node_p parent = todo.front();
        todo.pop_front();
        BOOST_FOREACH(const node_p& child, parent->children()) {
            if (child->parents().size() == 1) {
                unlearn(child);
                todo.push_back(child);
            }
            else {
                parent->remove_child(child);
            }
        }
    }

    // If replacing a root, need to update root datastructures, preserving
    // existing index.
    root_indices_t::iterator root_index_i =
        m_root_indices.find(known_which);
    if (root_index_i != m_root_indices.end()) {
        size_t index = root_index_i->second;
        m_root_indices.erase(root_index_i);
        m_roots[index] = with;
        m_root_indices[with] = index;
    }

    // At this point, we're done.  Once any external references to known_which
    // are gone, its shared count will go to 0 and it will be freed, reducing
    // any childrens shared count, and so forth, until it and all children
    // not still part of the graph are freed.
}

void MergeGraph::merge_tree(node_p& which)
{
    if (! which) {
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << errinfo_what(
                "Cannot merge singular node."
            )
        );
    }
    bool new_which;
    node_p known_which;
    boost::tie(new_which, known_which) = learn(which);

    if (! new_which) {
        assert(which != known_which);
        which = known_which;
    }
    else {
        list<node_p> todo;
        todo.push_back(which);

        while (! todo.empty()) {
            node_p parent = todo.front();
            todo.pop_front();
            BOOST_FOREACH(const node_p& child, parent->children()) {
                bool new_child;
                node_p known_child;
                boost::tie(new_child, known_child) = learn(child);
                if (! new_child) {
                    assert(child != known_child);
                    parent->replace_child(child, known_child);
                }
                else {
                    todo.push_back(child);
                }
            }
        }
    }
}

namespace {

string debug_node_decorator(const node_cp& node)
{
    stringstream r;
    r << "label=\"" << node << "\\n"
      << node->to_s()
      << "\"";
    return r.str();
}

}

void MergeGraph::write_debug_report(std::ostream& out)
{
    out << "node_by_sexpr: " << endl;
    BOOST_FOREACH(node_by_sexpr_t::const_reference v, m_node_by_sexpr) {
        out << v.first << " -> " << v.second->to_s() << " @ " << v.second << endl;
        if (v.first != v.second->to_s()) {
            out << "  ERROR: Mismatch." << endl;
        }
    }

    out << "root_indices: " << endl;
    BOOST_FOREACH(root_indices_t::const_reference v, m_root_indices) {
        out << v.first->to_s() << " @ " << v.first << " -> " << v.second << endl;
    }

    out << endl << "Graph: " << endl;
    to_dot(out, m_roots.begin(), m_roots.end(), debug_node_decorator);
}

namespace {

class validate_node
{
public:
    validate_node(const MergeGraph& g, ostream& out, bool& has_no_error) :
        m_g(g),
        m_out(out),
        m_has_no_error(has_no_error)
    {
        // nop
    }

    void operator()(const node_cp& node)
    {
        if (! node) {
            error(node, "singular");
            return;
        }

        BOOST_FOREACH(const weak_node_p& weak_parent, node->parents()) {
            node_cp parent = weak_parent.lock();
            check_is_known(parent);
            node_list_t::const_iterator i =
                find(
                    parent->children().begin(), parent->children().end(),
                    node
                );
            if (i == parent->children().end()) {
                error(node,
                    "not child of parent " +
                    boost::lexical_cast<string>(parent)
                );
            }
        }
    }

private:
    void check_is_known(const node_cp& node)
    {
        node_cp known_node = m_g.known(node);
        if (known_node != node) {
            error(node,
                "known node " +
                boost::lexical_cast<string>(known_node) + " != node " +
                boost::lexical_cast<string>(node)
            );
        }
    }
    void error(const node_cp& node, const string& msg)
    {
        m_has_no_error = false;
        m_out << "ERROR[node=" << node << "]: " << msg << endl;
    }

    const MergeGraph& m_g;
    ostream& m_out;
    bool&    m_has_no_error;
};

}

bool MergeGraph::write_validation_report(std::ostream& out)
{
    set<node_cp> visited;
    bool has_no_error = true;

    BOOST_FOREACH(const node_cp root, m_roots) {
        bfs_down(
            root,
            boost::make_function_output_iterator(
                validate_node(*this, out, has_no_error)
            ),
            visited
        );

        root_indices_t::const_iterator i = m_root_indices.find(root);
        if (i == m_root_indices.end()) {
            out << "ERROR: Root " << root->to_s() << " @ " << root
                << " not in indices." << endl;
            has_no_error = false;
        }
        else if (m_roots[i->second] != root) {
            out << "ERROR: Root " << root->to_s() << " @ " << root
                << " has index " << i->second << " which is root "
                << m_roots[i->second]->to_s() << " @ " << m_roots[i->second]
                << endl;
            has_no_error = false;
        }
    }

    BOOST_FOREACH(node_by_sexpr_t::const_reference v, m_node_by_sexpr) {
        if (! v.second) {
            out << "ERROR: singular node for sexpr " << v.first << endl;
            has_no_error = false;
        }
        else if (v.first != v.second->to_s()) {
            out << "ERROR: sexpr " << v.first
                << " does not match sexpr of node " << v.second->to_s()
                << " @ " << v.second << endl;
            has_no_error = false;
        }
    }

    BOOST_FOREACH(root_indices_t::const_reference v, m_root_indices) {
        if (m_roots[v.second] != v.first) {
            out << "ERROR: Root index " << v.second << " should be "
                << v.first->to_s() << " @ " << v.first
                << " but is " << m_roots[v.second]->to_s() << " @ "
                << m_roots[v.second] << endl;
        }
    }

    return has_no_error;
}

} // DAG
} // Predicate
} // IronBee
