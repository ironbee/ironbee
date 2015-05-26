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
 * @brief Predicate --- Eval
 *
 * Node evaluation support.  Works closely with dag.hpp.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __PREDICATE__EVAL__
#define __PREDICATE__EVAL__

#include <ironbee/predicate/dag.hpp>

#include <boost/function_output_iterator.hpp>

#include <stack>
#include <vector>

namespace IronBee {
namespace Predicate {

/**
 * Evaluation state for a single node.
 *
 * This class represents the evaluation state for a single node.  It provides
 * a variety of routines for modifying that state and is the main API for
 * implementations of Node::eval_calculate() and Node::eval_initialize().
 *
 * Nodes have four methods available to them for setting values and finished
 * state:
 *
 * 1. *Local List Values* -- A node may setup its own ValueList and add values
 *    to it.  It should call setup_local_list() to allocate the initial list
 *    and then use append_to_list() and finish() to add values to the list and
 *    finish itself, as appropriate.
 * 2. *Direct* -- A node may directly set its value and finish with
 *    finish(). As a shortcut for boolean  nodes, finish_true() and
 *    finish_false() will setup the appropriate values and finish the node in
 *    a single call.
 * 3. *Forwarded* -- A node may forward itself to another node by calling
 *    forward(), taking on the values and finish state of that node.  This is
 *    useful for nodes that *conditionally* take on the values of a child.
 *    Nodes that *unconditionally* take on the values of a child should
 *    transform into that child instead of using the forwarding mechanism.  It
 *    is possible to forward to nodes that in turn forward to other nodes.
 *    Such chains should be kept short.  Once a node is forwarding, it will no
 *    longer be calculated.
 * 4. *Aliased* -- A list node may directly alias another value by calling
 *    alias().  This is primarily useful when a node wants to take on the
 *    values of a list external to Predicate.  Aliasing should only be
 *    done with lists that are known to behave well: they should only
 *    append values and only do so at different phases, not within a single
 *    phase.  The node is still calculated so that it can finish itself
 *    appropriately via finish().
 **/
class NodeEvalState
{
public:
    //! Constructor.
    NodeEvalState();

    /**
     * @name Value Modifiers
     * Methods to modify value.  Should only be called from
     * Node::eval_calculate() and Node::eval_initialize().
     **/
    ///@{

    /**
     * Setup for local value.
     *
     * This must be called to setup the state for a local (unaliased) list
     * value.  Must be called before append_to_list().
     *
     * This method does nothing on subsequent calls.
     *
     * @param[in] mm Memory manager; determines lifetime of value.
     **/
    void setup_local_list(MemoryManager mm);

    /**
     * Setup for local values with name.
     *
     * See setup_local_list(EvalContext).
     *
     * @param[in] mm          Memory manager; determines lifetime of value.
     * @param[in] name        Name.
     * @param[in] name_length Length of @a name.
     **/
    void setup_local_list(
        MemoryManager mm,
        const char*   name,
        size_t        name_length
    );

    /**
     * Add to a list.
     *
     * @sa finished()
     * @sa values()
     * @sa finish()
     *
     * @throw einval if called on a finished() node.
     **/
    void append_to_list(Value v);

    /**
     * Mark node as finished.  Primarily for use with lists.
     *
     * @sa finished()
     * @sa append_to_list()
     *
     * @throw einval if called on a finished() node.
     **/
    void finish();

    /**
     * Mark node as finished with value.
     *
     * @throw einval if called on a finished() node.
     * @throw einval if called on a node that already has a value.
     **/
    void finish(Value v);

    /**
     * Forward behavior to another node.
     *
     * May only be called if this node is unfinished and valueless.  All
     * calls to finished() and value() will be forwarded to the other node
     * until the next reset.  This nodes calculate will not be called.
     *
     * @throw einval if called on a finished() node.
     * @throw einval if called on a node with a value.
     * @throw einval if called on a node already being forwarded.
     **/
    void forward(const node_p& other);

    /**
     * Alias a value.
     *
     * May only be called if this node is unfinished and valueless.  Sets
     * value to an alias of the given list.  It is up to the caller to
     * guarantee that the list only grows and to call finish once the list is
     * done growing.
     *
     * Once a node is aliased, it unlikely there is any more to do with the
     * value besides finish.  Thus, if you call alias(), be sure to check if
     * already aliased() in subsequent calls.
     *
     * @throw einval if called on a finished() node.
     * @throw einval if called on a node with a value.
     * @throw einval if called on a forwarded node.
     **/
    void alias(Value list);

    /**
     * Finish node as true.
     *
     * Convenience method for finishing the current node with a truthy value.
     **/
    void finish_true(EvalContext eval_context);

    /**
     * Set last phase evaluated at.
     **/
    void set_phase(ib_rule_phase_num_t phase);

    ///@}

    /**
     * @name Value Queries
     * Methods to query the current values of the node.
     **/
    ///@{

    // All queries intentionally inlined.

    /**
     * Is node finished?
     *
     * @warning Not relevant if forwarding. See GraphEvalState::is_finished().
     **/
    bool is_finished() const
    {
        return m_finished;
    }

    /**
     * Is node forwarding?
     **/
    bool is_forwarding() const
    {
        // Implicit cast to bool.
        return bool(m_forward);
    }

    /**
     * Is node aliased?
     *
     * Only meaningful for unfinished nodes.  Finished nodes cannot
     * distinguish between aliased and non-aliased.
     **/
    bool is_aliased() const
    {
        return m_value && ! m_local_values;
    }

    /**
     * What is node forwarded to?
     **/
    const node_p& forwarded_to() const
    {
        return m_forward;
    }

    /**
     * Last phase evaluated at.
     **/
    ib_rule_phase_num_t phase() const
    {
        return m_phase;
    }

    /**
     * Value.
     *
     * @warning Not relevant if forwarding. See GraphEvalState::values().
     **/
    Value value() const
    {
        return m_value;
    }

    ///@}

    /**
     * @name Node State
     * Methods to access node state.  The subclass of a Call may need to
     * maintain state during an evaluation.  That state is stored in this
     * class and may be accessed via a boost::any.  It is good practice to
     * setup state in Node::eval_initialize().
     **/
    ///@{

    //! Access state.
    boost::any& state()
    {
        return m_state;
    }

    ///@}

private:
    //! What node forwarding to.
    node_p m_forward;
    //! Is node finished.
    bool m_finished;
    //! Value.
    Value m_value;
    //! Mutable local list value.
    List<Value> m_local_values;
    //! Node specific state.
    boost::any m_state;
    //! Last phase evaluated at.
    ib_rule_phase_num_t m_phase;
};

/**
 * Raw profiling information for calls to GraphEvalState::eval().
 */
struct GraphEvalProfileData {

    //! Node ID.
    uint32_t      m_node_id;

    //! Relative clock time in microseconds.
    ib_time_t     m_eval_start;

    //! Relative clock time in microseconds.
    ib_time_t     m_eval_finish;

    /**
     * Duration in microseconds that the child nodes took.
     *
     * When a GraphEvalProfileData node's mark_finish() routine is
     * called, and it has a parent node defined, it will
     * add it's duration() value to the parent's m_child_duration time.
     */
    uint32_t     m_child_duration;

    /**
     * NULL of no parent or pointing to a parent data value.
     */
    GraphEvalProfileData* m_parent;

    explicit GraphEvalProfileData(uint32_t node_id);

    GraphEvalProfileData(
        uint32_t              node_id,
        GraphEvalProfileData* parent
    );

    //! Set m_eval_start to the current relative clock time.
    void mark_start();

    //! Set m_eval_finish to the current relative clock time.
    void mark_finish();

    //! Return the duration (finish - start) in microseconds.
    uint32_t duration() const;

    //! Return the parent pointer. This may be NULL if there is no parent.
    GraphEvalProfileData* parent() const;

    /**
     * Return the duration minus children durations.
     * (finish - start - m_child_duration).
     */
    uint32_t self_duration() const;

    /**
     * A unique id that maps a node to is expression.
     */
    uint32_t node_id() const;
};

/**
 * Evaluation state of an entire graph.
 *
 * This class maintains the state of an entire graph via a vector of
 * NodeEvalState indexed by node index (see Node::index()).  It provides a
 * evaluation oriented API to access and manipulate this state.
 *
 * The evaluation life cycle is:
 * 1. Constrict a GraphEvalState.
 * 2. Call initialize() on every node.
 * 2. Call eval() as necessary to force evaluation of a node.  Values may
 *    only change between phases, so subsequent calls to eval() within the
 *    same phase is equivalent to values().
 * 3. Use values() and is_finished() as necessary.  Both of these are only
 *    updated by eval(), so it is generally advisable to call eval() at each
 *    phase before any calls to values() or is_finished().
 **/
class GraphEvalState
{
public:
    /**
     * Constructor.
     *
     * @param[in] index_limit All indices of nodes must be below this.
     **/
    explicit
    GraphEvalState(size_t index_limit);

    /**
     * @name Direct accessors.
     * Routines to directly access eval state.
     *
     * These routines do not understand forwarding.  They are primarily for
     * use by nodes that wish to directly access their own state.
     **/
    ///@{

    // Intentionally inline.

    /**
     * Direct access to node evaluation state.
     *
     * @warning This method does not follow forwarded state.
     *
     * @sa final()
     *
     * @param[in] index Index of node to fetch evaluation state for.
     * @return Evaluation state of node with index @a index.
     **/
    NodeEvalState& operator[](size_t index)
    {
        return m_vector[index];
    }

    /**
     * Direct access to node evaluation state.
     *
     * Const version of previous.
     **/
    const NodeEvalState& operator[](size_t index) const
    {
        return m_vector[index];
    }

    ///@}

    /**
     * @name Smart accessors.
     * Routines to access eval state or portions of it.
     *
     * These routines fetch eval state or portions of it by index.  All of
     * them understand forwarding and return values for the final node in a
     * forwarding chain.
     *
     * These routines do not update value, so until node is finished, call
     * eval() at every phase before using these methods.
     **/
    ///@{

    /**
     * Fetch (const) node eval state for a given index.
     *
     * @param[in] index Index of node to fetch eval state for.
     * @return Eval state of node with index @a index.
     **/
    const NodeEvalState& final(size_t index) const;

    /**
     * Value of node.
     *
     * Iterators from list values should never be invalidated.
     *
     * @sa empty()
     *
     * @param[in] index Index of node to fetch values of.
     * @return Values of node with index @a index.
     **/
    Value value(size_t index) const;

    /**
     * Is node finished?
     *
     * Finished nodes guarantee that values() will not changed until the next
     * reset.  Unfinished nodes may add additional values (but will not change
     * or remove existing) values if eval() is called again; in particular,
     * if the Context changes.
     *
     * @param[in] index Index of node to find status of.
     * @return True iff node with index @a index is finished.
     **/
    bool is_finished(size_t index) const;

    /**
     * Last phase evaluated for node.
     *
     * The last phase eval() was called for this node or IB_PHASE_NONE if
     * eval has never been called.  Node::eval_calculate() is only called once
     * per phase.
     *
     * @param[in] index Index of node to find phase of.
     * @return Last phase node was evaluated at.
     **/
    ib_rule_phase_num_t phase(size_t index) const;

    ///@}

    /**
     * Initialize node.
     *
     * @param[in] node    Node to initialize
     * @param[in] context Evaluation context.
     **/
    void initialize(const node_cp& node, EvalContext context);

    /**
     * Evaluate node.
     *
     * This method understands forwarding and will act on the final node of a
     * forwarding chain.
     *
     * If node is finished or current phase is identical to phase during
     * previous eval() call, this is equivalent to values().  Otherwise, will
     * call Node::eval_calculate() to update value.
     *
     * @param[in] node    Node to evaluate.
     * @param[in] context Evaluation context.
     **/
    void eval(const node_cp& node, EvalContext context);

    /**
     * @name Profiling
     * Methods to access and control graph profiling information.
     *
     * Profiling may be turned on or off per GraphEvalState.
     * Until the profiling information is picked up by the user,
     * it is appended to a list of profiling records.
     **/
    ///@{

    typedef std::list<GraphEvalProfileData> profiler_data_list_t;

    //! Fetch the list of profiling data.
    profiler_data_list_t& profiler_data();

    //! Fetch the list of profiling data.
    const profiler_data_list_t& profiler_data() const;

    //! Clear profiling data.
    void profiler_clear();

    //! Enable or disable profiling.
    void profiler_enabled(bool enabled);

    /**
     * Mark the start of @a node 's evaluation for profiling.
     *
     * This records the relative start time and the node name
     * in the profiling data list.
     *
     * @param[in] node The node we are profiling.
     */
    GraphEvalProfileData& profiler_mark(node_cp node);

    //! Record finish info for the last node profiler_mark() operated on.
    void profiler_record(GraphEvalProfileData& data);

    ///@}

private:
    typedef std::vector<NodeEvalState> vector_t;
    vector_t m_vector;

    //! If true, eval() profiles node evaluation.
    bool m_profile;

    //! List of all node profiling execution timings.
    profiler_data_list_t m_profile_data;

    /**
     * At the start of a call eval(), this points at the parent profile data.
     *
     * If profiling is turned off, this is always NULL.
     *
     * When a root node is being evaluated, this will be initially NULL.
     *
     * Before eval_calculate() is called, and if profiling is enabled,
     * this will be set to point to the profiling data for that node
     * in this GraphEvalState.
     *
     * When eval_calculate() returns this is set to the previous value.
     *
     * This allows for creating GraphEvalProfileData objects that reference
     * their parent node, allowing for the child nodes to report
     * how much time they took in an evaluation. This allows the
     * parent node to compute how much time *it* took, in contrast
     * to the total time the parent node plus its children took.
     */
    GraphEvalProfileData *m_parent_profile_data;
};

/// @cond Internal
namespace Impl {

/**
 * Helper functional for make_indexer().
 **/
template<typename LIST>
class make_indexer_helper_t
{
public:
    /**
     * Constructor.
     *
     * @param[in] index_limit Current index limit; updated at each call.
     **/
    explicit
    make_indexer_helper_t(size_t& index_limit, LIST& traversal);

    //! Call.
    void operator()(const node_p& node);

private:
    size_t& m_index_limit;
    LIST&   m_traversal;
};

/**
 * Helper functional for make_initializer().
 **/
class make_initializer_helper_t
{
public:
    /**
     * Constructor.
     *
     * @param[in] graph_eval_state Graph evaluation state.
     * @param[in] context          Evaluation context.
     **/
    explicit
    make_initializer_helper_t(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    );

    //! Call.
    void operator()(const node_cp& node);

private:
    GraphEvalState& m_graph_eval_state;
    EvalContext     m_context;
};

#ifndef DOXYGEN_SKIP

template<typename LIST>
make_indexer_helper_t<LIST>::make_indexer_helper_t(
    size_t& index_limit,
    LIST&   traversal
) :
    m_index_limit(index_limit),
    m_traversal(traversal)
{
    // nop
}

template<typename LIST>
void make_indexer_helper_t<LIST>::operator()(const node_p& node)
{
    node->set_index(m_index_limit);
    ++m_index_limit;
    m_traversal.push_back(node);
}

#endif
}
/// @endcond

/**
 * Output iterator for use with bfs_down() to index a graph.
 *
 * This will also record the traversal in the provided @a traversal
 * object that supports `push_back(node)`.
 *
 * Example:
 * @code
 * size_t index_limit;
 * vector<node_cp> traversal;
 * bfs_down(
 *    graph.roots().first, graph.roots().second,
 *    make_indexer(index_limit, traversal)
 * );
 * @endcode
 *
 * @param[out] index_limit Where to store index limit.
 * @param[out] traversal Container supporting `push_back(node)` to
 *             record the nodes we visit.
 *
 * @return Output iterator.
 **/
template<typename LIST>
boost::function_output_iterator<Impl::make_indexer_helper_t<LIST> >
make_indexer(size_t& index_limit, LIST& traversal);

template<typename LIST>
boost::function_output_iterator<Impl::make_indexer_helper_t<LIST> >
make_indexer(size_t& index_limit, LIST& traversal)
{
    index_limit = 0;
    return boost::function_output_iterator<Impl::make_indexer_helper_t<LIST> >(
        Impl::make_indexer_helper_t<LIST>(index_limit, traversal)
    );
}

/**
 * Output iterator for use with bfs_down() to initialize a graph.
 *
 * Example:
 * @code
 * bfs_down(
 *    graph.roots().first, graph.roots().second,
 *    make_initializer(graph_eval_state, context)
 * );
 * @endcode
 *
 * @param[in] graph_eval_state Graph evaluation state.
 * @param[in] context          Evaluation context.
 * @return Output iterator.
 **/
boost::function_output_iterator<Impl::make_initializer_helper_t>
make_initializer(GraphEvalState& graph_eval_state, EvalContext context);

} // Predicate
} // IronBee

#endif
