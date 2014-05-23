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

#include <predicate/merge_graph.hpp>

#include <predicate/bfs.hpp>
#include <predicate/dot.hpp>

#include <boost/bind.hpp>
#include <boost/function_output_iterator.hpp>
#include <boost/lexical_cast.hpp>

#include <sstream>

using namespace std;

namespace IronBee {
namespace Predicate {

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

    m_root_indices[root].insert(index);
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

const MergeGraph::indices_t& MergeGraph::root_indices(const node_cp& root) const
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

bool MergeGraph::is_root(const node_cp& node) const
{
    if (! node) {
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << errinfo_what(
                "Cannot search for singular node."
            )
        );
    }
    return m_root_indices.find(node) != m_root_indices.end();
}

pair<bool, node_p> MergeGraph::learn(const node_p& which)
{
    if (! which) {
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << errinfo_what(
                "Cannot learn singular node."
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
                "Cannot know singular node."
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

void MergeGraph::remove_tree(const node_p& which)
{
    if (! which) {
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << errinfo_what(
                "Singular which."
            )
        );
    }

    // Follow descendants of which so long as single parent.
    // When we hit a multiple parent child, we need to remove it from the
    // parent we came from (the descendant of which), but should not
    // unlearn its sexpr (it's still in the graph from another parent)
    // and can stop our descent.
    // When which has know common subexpressions with other parts of
    // the graph, this is equivalent to unlearning all children.
    // Note that we do not check for multiple visits.  It is important, if a
    // child is reached via multiple paths, to handle all such paths.  E.g.,
    // a child with two paths will, the first time, be removed from one of its
    // parents and the second time unlearn (as it now has a single parent).
    list<node_p> todo;
    todo.push_back(which);
    while (! todo.empty()) {
        node_p parent = todo.front();
        todo.pop_front();
        m_origins.erase(parent);
        node_list_t children = parent->children();
        BOOST_FOREACH(const node_p& child, children) {
            if (child->parents().size() == 1) {
                if (! is_root(child)) {
                    unlearn(child);
                    m_transform_record.insert(make_pair(child, node_p()));
                    todo.push_back(child);
                }
            }
            parent->remove_child(child);
        }
    }
}

void MergeGraph::update_root(const node_p& which, const node_p& with)
{
    // If replacing a root, need to update root datastructures,
    // preserving existing index.
    root_indices_t::iterator root_indices_i =
        m_root_indices.find(which);
    if (root_indices_i != m_root_indices.end()) {
        indices_t indices;
        indices.swap(root_indices_i->second);
        m_root_indices.erase(root_indices_i);
        BOOST_FOREACH(size_t index, indices) {
            m_roots[index] = with;
        }
        copy(
            indices.begin(), indices.end(),
            inserter(m_root_indices[with], m_root_indices[with].begin())
        );
    }
}

void MergeGraph::knit(const node_p& from)
{
    list<node_p> todo;

    // Start with parents of from; from itself has already been learned
    // as part of replace().
    BOOST_FOREACH(const weak_node_p& parent, from->parents()) {
        todo.push_back(parent.lock());
    }

    while (! todo.empty()) {
        node_p n = todo.front();
        todo.pop_front();

        node_p known_n = known(n);

        // Possible we already reached this node or that we have arrived at
        // the known copy a complex case.
        if (known_n && known_n == n) {
            continue;
        }

        // Setup next level of BFS.  Note: Not using known_n's parents.
        BOOST_FOREACH(const weak_node_p& parent, n->parents()) {
            todo.push_back(parent.lock());
        }

        if (! known_n) {
            // Simple case -- learn n and keep going up.
            learn(n);
        }
        else {
            // Complex case -- merge n and its known version.

            // Make sure children are identical.
            if (
                n->children().size() != known_n->children().size() ||
                ! equal(
                    n->children().begin(), n->children().end(),
                    known_n->children().begin()
                )
            ) {
                BOOST_THROW_EXCEPTION(
                    IronBee::eother() << errinfo_what(
                        "Insanity.  Please report as bug: "
                        "Unequal children while knitting."
                    )
                );
            }

            // Update transform record.
            m_transform_record.insert(make_pair(n, known_n));

            // Update origin information.
            list<string> n_origins = origins(n);
            BOOST_FOREACH(const std::string& origin, n_origins) {
                add_origin(known_n, origin);
            }

            // Replace n with known_n in all parents.
            weak_node_list_t parents = n->parents();
            BOOST_FOREACH(const weak_node_p& weak_parent, parents) {
                weak_parent.lock()->replace_child(n, known_n);
            }
            assert(n->parents().empty());

            // Remove all children of n to break it free of the MergeGraph.
            node_list_t to_remove = n->children();
            BOOST_FOREACH(const node_p& child, to_remove) {
                n->remove_child(child);
            }
            assert(n->children().empty());

            update_root(n, known_n);
        }
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

    // Grab origins of which.
    const list<string> which_origins = origins(known_which);

    // Unlearn all subexpressions of known_which and ancestors.
    bfs_up(
        known_which,
        boost::make_function_output_iterator(
            boost::bind(&MergeGraph::unlearn, this, _1)
        )
    );

    // If with is known, this will change with to point to it.  Otherwise,
    // this will merge in the tree based at with.
    merge_tree(with);

    // Replace known_which with with in all parents of known_which.
    // Doing so will update the sexprs of all ancestors of with.
    // As we are holding a node_p to known_which, it and its descendants
    // will stay around long enough for us to unlearn them as necessary
    // later on.
    weak_node_list_t parents = known_which->parents();
    BOOST_FOREACH(const weak_node_p& weak_parent, parents) {
        weak_parent.lock()->replace_child(known_which, with);
    }

    update_root(known_which, with);

    // Remove known_which and unshared children.
    remove_tree(known_which);

    // Knit ancestors back together.  See knit().
    knit(with);

    // Update transform record.
    m_transform_record.insert(make_pair(which, with));

    // Update origin information.
    BOOST_FOREACH(const std::string& origin, which_origins) {
        add_origin(with, origin);
    }

    // At this point, we're done.  Once any external references to known_which
    // are gone, its shared count will go to 0 and it will be freed, reducing
    // any children's shared count, and so forth, until it and all children
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

string debug_node_decorator(const MergeGraph& g, const node_cp& node)
{
    stringstream r;
    bool isroot = true;
    const MergeGraph::indices_t* indices;
    try {
        indices = &g.root_indices(node);
    }
    catch (IronBee::enoent) {
        isroot = false;
    }
    if (isroot) {
        r << "shape=box,label=\"root" ;
        BOOST_FOREACH(size_t index, *indices) {
            r << " " << index;
        }
        r << "\\n";
    }
    else {
        r << "label=\"";
    }
    r << node << "\\n" << node->to_s() << "\"";
    return r.str();
}

}

void MergeGraph::write_debug_report(std::ostream& out) const
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
        out << v.first->to_s() << " @ " << v.first << " ->";
        BOOST_FOREACH(size_t index, v.second) {
            out << " " << index;
        }
        out << endl;
    }

    out << "origins: " << endl;
    BOOST_FOREACH(origins_t::const_reference v, m_origins) {
        out << v.first->to_s() << ":" << endl;
        BOOST_FOREACH(const string& origin, v.second) {
            out << "  " << origin << endl;
        }
    }

    out << endl << "Graph: " << endl;
    to_dot(
        out,
        m_roots.begin(), m_roots.end(),
        boost::bind(debug_node_decorator, boost::ref(*this), _1)
    );
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
            if (! parent) {
                error(node, "singular parent");
                return;
            }
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

bool MergeGraph::write_validation_report(std::ostream& out) const
{
    bool has_no_error = true;

    bfs_down(
        m_roots.begin(), m_roots.end(),
        boost::make_function_output_iterator(
            validate_node(*this, out, has_no_error)
        )
    );

    BOOST_FOREACH(const node_cp root, m_roots) {
        root_indices_t::const_iterator i = m_root_indices.find(root);
        if (i == m_root_indices.end()) {
            out << "ERROR: Root " << root->to_s() << " @ " << root
                << " not in indices." << endl;
            has_no_error = false;
        }
        else {
            BOOST_FOREACH(size_t index, i->second) {
                if (m_roots[index] != root) {
                    out << "ERROR: Root " << root->to_s() << " @ " << root
                        << " has index " << index << " which is root "
                        << m_roots[index]->to_s() << " @ " << m_roots[index]
                        << endl;
                    has_no_error = false;
                }
            }
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
        BOOST_FOREACH(size_t index, v.second) {
            if (m_roots[index] != v.first) {
                out << "ERROR: Root index " << index << " should be "
                    << v.first->to_s() << " @ " << v.first
                    << " but is " << m_roots[index]->to_s() << " @ "
                    << m_roots[index] << endl;
                has_no_error = false;
            }
        }
    }

    return has_no_error;
}

node_p MergeGraph::find_transform(const node_cp& source) const
{
    transform_record_t::const_iterator i;
    node_p result;

    i = m_transform_record.find(source);
    if (i == m_transform_record.end()) {
        BOOST_THROW_EXCEPTION(
            IronBee::enoent() << errinfo_what(
                "Unknown node."
            )
        );
    }

    while (i != m_transform_record.end() && i->second) {
        result = i->second;
        i = m_transform_record.find(result);
    }

    return result;
}

void MergeGraph::add_origin(const node_cp& which, const std::string& origin)
{
    node_p known_which = known(which);
    if (! known_which) {
        BOOST_THROW_EXCEPTION(
            IronBee::enoent() << errinfo_what(
                "Unknown node while adding origin: " + which->to_s()
            )
        );
    }

    m_origins[known_which].push_back(origin);
}

const std::list<std::string>& MergeGraph::origins(const node_cp& which) const
{
    static const std::list<std::string> c_empty_list;

    node_p known_which = known(which);
    if (! known_which) {
        BOOST_THROW_EXCEPTION(
            IronBee::enoent() << errinfo_what(
                "Unknown node while finding origins: " + which->to_s()
            )
        );
    }

    origins_t::const_iterator i = m_origins.find(known_which);
    if (i == m_origins.end()) {
        return c_empty_list;
    }
    else {
        return i->second;
    }
}

} // Predicate
} // IronBee
