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
 * @brief Predicate --- DAG
 *
 * Defines the base hierarchy for DAG nodes.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __PREDICATE__DAG__
#define __PREDICATE__DAG__

#include <predicate/ironbee.hpp>
#include <predicate/value.hpp>

#include <ironbeepp/memory_pool_lite.hpp>

#include <boost/enable_shared_from_this.hpp>
#include <boost/foreach.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

#include <iostream>
#include <list>
#include <string>

namespace IronBee {
namespace Predicate {

// Defined below.
class Node;
class Call;
class Literal;

// Defined in reporter.hpp
class NodeReporter;
// Defined in merge_graph.hpp
class MergeGraph;
// Defined in call_factory.hpp
class CallFactory;
// Defined in eval.hpp.
class GraphEvalState;
class NodeEvalState;

/**
 * Shared pointer to Node.
 **/
typedef boost::shared_ptr<Node> node_p;

/**
 * Weak pointer to Node.
 **/
typedef boost::weak_ptr<Node> weak_node_p;

/**
 * Shared const pointer to Node.
 **/
typedef boost::shared_ptr<const Node> node_cp;

/**
 * Shared pointer to Call.
 **/
typedef boost::shared_ptr<Call> call_p;

/**
 * Shared const pointer to Call.
 **/
typedef boost::shared_ptr<const Call> call_cp;

/**
 * Shared pointer to Literal.
 **/
typedef boost::shared_ptr<Literal> literal_p;

/**
 * Shared const pointer to Literal.
 **/
typedef boost::shared_ptr<const Literal> literal_cp;

//! List of nodes.  See children().
typedef std::list<node_p> node_list_t;

//! Weak list of nodes.  See parents().
typedef std::list<weak_node_p> weak_node_list_t;

//! List of const nodes.
typedef std::list<node_cp> node_clist_t;

/**
 * A node in the predicate DAG.
 *
 * Nodes make up the predicate DAG.  They also appear in the expressions trees
 * that are merged together to construct the DAG. This class is the top of
 * the class hierarchy for nodes.  It can not be directly instantiated or
 * subclassed.  For literal values, Literal.  For call values, create and
 * instantiate a subclass of Call.
 *
 * This class hierarchy defines how to evaluate (through eval_calculate(),
 * eval_initialize()) but does not store the data.
 * Data is stored separately in GraphEvalState and NodeEvalState.  This
 * separation allows simultaneous evaluations of the DAG across different
 * EvalContext.
 *
 * @sa Call
 * @sa Literal
 */
class Node :
    public boost::enable_shared_from_this<Node>,
    boost::noncopyable
{
    friend class Call;
    friend class Literal;

private:
    //! Private Constructor.
    Node();

public:
    //! Destructor.
    virtual ~Node();

    /**
     * @name Graph Structure Manipulation
     * These routines are used to modify the structure of the graph.
     **/
    ///@{

    /**
     * Add a child.
     *
     * Adds to end of children() and adds @c this to end of parents() of
     * child.  Subclasses can add additional behavior.
     *
     * O(1)
     *
     * @param[in] child Child to add.
     * @throw IronBee::einval if ! @a child.
     **/
    virtual void add_child(const node_p& child);

    /**
     * Remove a child.
     *
     * Removes from children() and removes @c this from parents() of child.
     * Subclasses can add additional behavior.
     *
     * O(n) where n is size of children() of @c this or parents() of child.
     *
     * @param[in] child Child to remove.
     * @throw IronBee::enoent if no such child.
     * @throw IronBee::einval if not a parent of child.
     * @throw IronBee::einval if ! @a child.
     **/
    virtual void remove_child(const node_p& child);

    /**
     * Replace a child.
     *
     * Replaces a child with another node.  This can be used to change
     * a child without moving it to the end of the children as remove_child(),
     * add_child() would.
     *
     * O(n) where n is size of children() of @c this or parents() of child.
     *
     * @param[in] child Child to replace.
     * @param[in] with  Child to replace with.
     * @throw IronBee::enoent if no such child.
     * @throw IronBee::einval if not a parent of child.
     * @throw IronBee::einval if ! @a child or ! @a with.
     **/
    virtual void replace_child(const node_p& child, const node_p& with);

    ///@}


    /**
     * S-Expression.
     *
     * An S-expression representation of the expression tree rooted at this
     * node.  See parse.hpp for detailed grammar.
     *
     * This method returns a string reference, but this reference is not
     * stable if this node or any children are changed.  It should be copied
     * if needed beyond such operations.
     *
     * S-Expressions are calculated dynamically and cached, so calling this
     * method is cheap.
     *
     * @return S-Expression of expression tree rooted at node.
     **/
    virtual const std::string& to_s() const = 0;

    //! Children accessor -- const.
    const node_list_t& children() const
    {
        return m_children;
    }
    //! Parents accessor -- const.
    const weak_node_list_t& parents() const
    {
        return m_parents;
    }

    //! True iff node is a literal.
    bool is_literal() const;

    /**
     * @name Lifecycle
     * These routine implement the graph lifecycle.
     **/
    ///@{

    /**
     * Perform pre-transformation validations.
     *
     * This method may be overridden by a child.  Default behavior is to
     * call validate().
     *
     * @note Exceptions will not be immediately caught so should only be used
     *       if it is appropriate to abort the entire predicate system, e.g.,
     *       on an insanity error.
     *
     * @param[in] reporter Reporter to use for errors or warnings.
     **/
    virtual void pre_transform(NodeReporter reporter) const;

    /**
     * Perform transformations.
     *
     * This method may be overridden by a child.  Default behavior is to
     * do nothing and return false.
     *
     * This method is called for every Node during the transformation phase.
     * If any such call returns true, the whole process is repeated.
     *
     * Transformations should not be done directly, but rather through
     * @a merge_graph(), i.e., do not use add_child(), remove_child(), or
     * replace_child(); instead use MergeGraph::add(), MergeGraph::remove(),
     * or MergeGraph::replace().  Note that your method can fetch a
     * @ref node_p for itself via shared_from_this().
     *
     * Note: Reporting errors will allow the current transformation loop to
     * continue for other nodes, but will then end the transformation phase.
     *
     * @param[in] merge_graph  MergeGraph used to change the DAG.
     * @param[in] call_factory CallFactory to create new nodes with.
     * @param[in] reporter     Reporter to use for errors or warnings.
     * @return true iff any changes were made.
     **/
    virtual bool transform(
        MergeGraph&        merge_graph,
        const CallFactory& call_factory,
        NodeReporter       reporter
    );

    /**
     * Perform post-transformation validations.
     *
     * Default behavior is to call validate().  See pre_transform() for
     * additional discussion.
     *
     * @param[in] reporter Reporter to use for errors or warnings.
     **/
    virtual void post_transform(NodeReporter reporter) const;

    /**
     * Perform validation checks.
     *
     * By default, this is called by pre_transform() and post_transform(),
     * although that behavior can be overridden.
     *
     * Default behavior is to do nothing.
     *
     * @sa Standard::Validate
     *
     * @param[in] reporter Reporter to use for errors or warnings.
     * @return true iff no *errors* reported.
     **/
    virtual bool validate(NodeReporter reporter) const;

    /**
     * Preform any state preparations needed for evaluation.
     *
     * This method is called after all transformations are complete but before
     * any evaluation.  It provides the environment of the node and should be
     * used to do any setup needed to calculate values.
     *
     * @param[in]  environment Environment for evaluation.
     * @param[in]  reporter    Reporter to report errors with.
     **/
    virtual void pre_eval(Environment environment, NodeReporter reporter);

    ///@}

    /**
     * @name Evaluation Support
     * Methods to support evaluation.  These methods are usually not called
     * directly, but instead are called from GraphEvalState and NodeEvalState.
     * They are public to allow for certain low level interactions with nodes.
     **/
    ///@{

    //! Set index of node to @a index.
    void set_index(size_t index);

    //! Access index.
    // Intentionally inline.
    size_t index() const
    {
        return m_index;
    }

    /**
     * Initialize node eval state.
     *
     * This method is called before each evaluation run.  It should setup
     * any state or initial values of @a node_eval_state.  See
     * NodeEvalState::state().
     *
     * @param[in] graph_eval_state Graph evaluation state.
     * @param[in] context          Evaluation context.
     **/
    virtual void eval_initialize(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;

    /**
     * Calculate value and update state.
     *
     * Subclass classes should implement this to calculate and call methods
     * of @a node_eval_state appropriately to add values and finish.
     *
     * This method will be called any time eval() is called while the node is
     * unfinished.  It will not be called on a finished node.
     *
     * @param[in] graph_eval_state Graph evaluation state.
     * @param[in] context          Context of calculation.
     * @return Value of node.
     */
    virtual void eval_calculate(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const = 0;

    ///@}

private:
    /**
     * Remove this from parents of @a child.
     *
     * @param[in] child Child to unlink from.
     * @throw IronBee::einval if not in @a child's parent list.
     **/
    void unlink_from_child(const node_p& child) const;

    //! List of parents (note weak pointers).
    weak_node_list_t m_parents;
    //! List of children.
    node_list_t m_children;
    //! Index.
    size_t m_index;
};

//! Ostream output operator.
std::ostream& operator<<(std::ostream& out, const Node& node);

/**
 * Node representing a function Call.
 *
 * All Call nodes must have a name.  Calls with the same name are considered
 * to implement the same function.
 *
 * This class is unique in the class hierarchy as being subclassable.  Users
 * should create subclasses for specific functions.  Subclasses must implement
 * name() and eval_calculate().  Subclasses may also define add_child(),
 * remove_child(), and replace_child() but must call the parents methods
* within.
 **/
class Call :
    public Node
{
public:
    //! Constructor.
    Call();

    //! See Node::add_child().
    virtual void add_child(const node_p& child);

    //! See Node::remove_child().
    virtual void remove_child(const node_p& child);

    //! See Node::replace_child().
    virtual void replace_child(const node_p& child, const node_p& with);

    //! S-expression: (@c name children...)
    virtual const std::string& to_s() const;

    /**
     * Name accessor.
     */
    virtual std::string name() const = 0;

private:
    //! Mark m_s as in need of recalculation.
    void reset_s() const;

    //! Recalculate m_s.
    void recalculate_s() const;

    // Because name() is pure, Call::Call() can not calculate m_s.  I.e., we
    // need to fully construct the class before setting m_s.  So we have
    // to_s() calculate it on the fly the first time.  The following two
    // members maintain that cache.

    //! Have we calculate m_s at all?
    mutable bool m_calculated_s;

    //! String form.
    mutable std::string m_s;
};

/**
 * Literal node: no children and value independent of EvalContext.
 *
 * This class may not be subclassed.
 **/
class Literal :
    public Node
{
public:
    //! Construct null literal.
    Literal();

    //! Construct literal from memory pool and value.
    Literal(
        const boost::shared_ptr<ScopedMemoryPoolLite>& memory_pool,
        Value                                          value
    );

    //! Construct literal from value, duping value with Value::dup().
    explicit Literal(Value value);
    
    //! Construct literal from int.
    explicit Literal(int value);

    //! Construct literal from float.
    explicit Literal(long double value);

    //! Construct literal from string.
    explicit Literal(const std::string& value);

    //! Value of literal.
    // Intentionally inline.
    Value literal_value() const
    {
        return m_value;
    }

    //! @throw IronBee::einval always.
    virtual void add_child(const node_p&);
    //! @throw IronBee::einval always.
    virtual void remove_child(const node_p&);
    //! @throw IronBee::einval always.
    virtual void replace_child(const node_p& child, const node_p& with);

    //! @throw IronBee::einval always.
    virtual void eval_calculate(
        GraphEvalState& graph_eval_state,
        EvalContext    context
    ) const;

    //! Set value based on literal_values() and finish.
    virtual void eval_initialize(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;

    //! S-Expression.
    // Intentionally inline.
    virtual const std::string& to_s() const
    {
        return m_sexpr;
    }

private:
    //! Keep track of where our memory comes from.
    boost::shared_ptr<ScopedMemoryPoolLite> m_memory_pool;

    //! Value returned by literal_values().
    Value m_value;

    //! Cache sexpr for easy access.
    std::string m_sexpr;
};

} // Predicate
} // IronBee

#endif
