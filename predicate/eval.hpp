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

#include <predicate/dag.hpp>

#include <boost/function_output_iterator.hpp>

namespace IronBee {
namespace Predicate {

/**
 * Evaluation state for a single node.
 *
 * This class represents the evaluation state for a single node.  It provides
 * a variety of routines for modifying that state and is the main API for
 * implementations of Node::eval_calculate() and Node::eval_initialize().
 *
 * Nodes have three methods available to them for setting values and finished
 * state:
 *
 * 1. *Local Values* -- A node may setup its own ValueList and add values to
 *    it.  It should call setup_local_values() to allocate the initial list
 *    and then use add_value() and finish() to add values to the list and
 *    finish itself, as appropriate.  As a shortcut for boolean like nodes,
 *    finish_true() and finish_false() will setup the appropriate lists and
 *    finish the node in a single call.
 * 2. *Forwarded* -- A node may forward itself to another node by calling
 *    forward(), taking on the values and finish state of that node.  This is
 *    useful for nodes that *conditionally* take on the values of a child.
 *    Nodes that *unconditionally* take on the values of a child should
 *    transform into that child instead of using the forwarding mechanism.  It
 *    is possible to forward to nodes that in turn forward to other nodes.
 *    Such chains should be kept short.  Once a node is forwarding, it will no
 *    longer be calculated.
 * 3. *Aliased* -- A node may directly alias another ValueList by calling
 *    alias().  This is primarily useful when a node wants to take on the
 *    values of a ValueList external to Predicate.  Aliasing should only be
 *    done with ValueLists that are known to behave well: they should only
 *    append values and only do so at different phases, not within a single
 *    phase.  The node is still calculated so that it can finish itself
 *    appropriately via finish().
 *
 * Nodes are allowed to have a singular (null) value list while not finished.
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
     * Setup for local values.
     *
     * This must be called to setup the state for local (unaliased) values.
     * Must be called before add_value().
     *
     * This method does nothing on subsequent calls.
     *
     * @param[in] context Evaluation context; determines lifetime of values.
     **/
    void setup_local_values(EvalContext context);

    /**
     * Add a value.
     *
     * @sa finished()
     * @sa values()
     * @sa finish()
     *
     * @throw einval if called on a finished() node.
     **/
    void add_value(Value v);

    /**
     * Mark node as finished.
     *
     * @sa finished()
     * @sa add_value()
     *
     * @throw einval if called on a finished() node.
     **/
    void finish();

    /**
     * Forward behavior to another node.
     *
     * May only be called if this node is unfinished and has no values.  All
     * calls to finished() and values() will be forwarded to the other node
     * until the next reset.  This nodes calculate will not be called.
     *
     * @throw einval if called on a finished() node.
     * @throw einval if called on a node with non-empty values.
     * @throw einval if called on a node already being forwarded.
     **/
    void forward(const node_p& other);

    /**
     * Alias a list.
     *
     * May only be called if this node is unfinished and has no values.  Sets
     * values to an alias of the given list.  It is up to the caller to
     * guarantee that the list only grows and to call finish once the list is
     * done growing.
     *
     * Once a node is aliased, it unlikely there is any more to do with the
     * value besides finish.  Thus, if you call alias(), be sure to check if
     * already aliased() in subsequent calls.
     *
     * @throw einval if called on a finished() node.
     * @throw einval if called on a node with non-empty values.
     * @throw einval if called on a forwarded node.
     **/
    void alias(ValueList list);

    /**
     * Finish node as true.
     *
     * Convenience method for finishing the current node with a truthy value.
     **/
    void finish_true(EvalContext eval_context);

    /**
     * Finish node as false.
     *
     * Convenience method for finishing the current node with a falsy value.
     **/
    void finish_false(EvalContext eval_context);

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
        return m_forward;
    }

    /**
     * Is node aliased?
     **/
    bool is_aliased() const
    {
        return m_values && ! m_local_values;
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
     * Values.
     *
     * @warning Not relevant if forwarding. See GraphEvalState::values().
     **/
    ValueList values() const
    {
        return m_values;
    }

    ///@}

    /**
     * @name Node State
     * Methods to access node state.  The subclass of a Call may need to
     * maintain state during an evaluation.  That state is stored in this
     * class and may be accessed via a boost::any.  It is good practice to
     * setup state in Node::reset().
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
    //! Value list to use for values.
    ValueList m_values;
    //! Mutable local value list.
    List<Value> m_local_values;
    //! Node specific state.
    boost::any m_state;
    //! Last phase evaluated at.
    ib_rule_phase_num_t m_phase;
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
     * Values of node.
     *
     * The value list may be singular (NULL).  Once non-singular, iterators
     * from the list should never be invalidated.
     *
     * @sa empty()
     *
     * @param[in] index Index of node to fetch values of.
     * @return Values of node with index @a index.
     **/
    ValueList values(size_t index) const;

    /**
     * Number of values of a node.
     *
     * @param[in] index Index of node to count values of.
     * @return 0 if node is singular and number of values in valuelist
     *         otherwise.
     **/
    size_t size(size_t index) const;

    /**
     * True if node has no values.
     *
     * @param[in] index Index of node to check emptyness of.
     * @return true iff node has singular or empty value list.
     **/
    bool empty(size_t index) const;

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
    void initialize(const node_p& node, EvalContext context);

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
     * @warning @a node may have a singular (NULL) value list.
     *
     * @param[in] node    Node to evaluate.
     * @param[in] context Evaluation context.
     * @return Values of @a node.
     **/
    ValueList eval(const node_p& node, EvalContext context);

private:
    typedef std::vector<NodeEvalState> vector_t;
    vector_t m_vector;
};

/// @cond Impl
namespace Impl {

/**
 * Helper functional for make_indexer().
 **/
class make_indexer_helper_t
{
public:
    /**
     * Constructor.
     *
     * @param[in] index_limit Current index limit; updated at each call.
     **/
    explicit
    make_indexer_helper_t(size_t& index_limit);

    //! Call.
    void operator()(const node_p& node);

private:
    size_t& m_index_limit;
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
    void operator()(const node_p& node);

private:
    GraphEvalState& m_graph_eval_state;
    EvalContext     m_context;
};

}
/// @endcond

/**
 * Output iterator for use with bfs_down() to index a graph.
 *
 * Example:
 * @code
 * size_t index_limit;
 * bfs_down(
 *    graph.roots().first, graph.roots().second,
 *    make_indexer(index_limit)
 * );
 * @endcode
 *
 * @param[out] index_limit Where to store index limit.
 * @return Output iterator.
 **/
boost::function_output_iterator<Impl::make_indexer_helper_t>
make_indexer(size_t& index_limit);

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
