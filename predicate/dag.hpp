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
class Null;
class String;
class Integer;
class Float;

// Defined in reporter.hpp
class NodeReporter;
// Defined in merge_graph.hpp
class MergeGraph;
// Defined in call_factory.hpp
class CallFactory;

/// @cond internal
namespace Impl {

/**
 * Used to make certain classes unsubclassable.
 *
 * See C++ FAQ.
 **/
class Final
{
    // Classes to be final.
    friend class Predicate::Null;
    friend class Predicate::String;
    friend class Predicate::Integer;
    friend class Predicate::Float;
private:
    //! Private constructor.
    Final() {}
};

} // Impl
/// @endcond

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

/**
 * A node in the predicate DAG.
 *
 * Nodes make up the predicate DAG.  They also appear in the expressions trees
 * that are merged together to construct the DAG. This class is the top of
 * the class hierarchy for nodes.  It can not be directly instantiated or
 * subclassed.  For literal values, instantiate Null or String.  For
 * call values, create and instantiate a subclass of Call.
 *
 * Nodes are not thread safe except for values.  Values are per-node and
 * reset(), has_value(), and value() can be called safely from multiple
 * threads, resulting in the value for that particular thread.  Subclasses
 * that implement calculate() in a thread-safe manner can then have eval()
 * called safely from multiple threads.
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

    /**
     * Is node finished in this thread.
     *
     * Finished nodes guarantee that values() will not changed until the next
     * reset.  Unfinished nodes may add additional values (but will not change
     * or remove existing) values if eval() is called again; in particular,
     * if the Context changes.
     **/
    bool is_finished() const;

    /**
     * Values of node in this thread.
     *
     * Iterators from the returned list are invalidated by reset() and
     * may be invalidated when the list goes from empty to non-empty.  Once
     * the list is non-empty, iterators will not be invalidated except by
     * reset().  This contract allows for forwarding (see forward()).
     *
     * @sa finished()
     **/
    ValueList values() const;

    //! True iff node is a literal, in which case eval(EvalContext()) is valid.
    bool is_literal() const;

    //! Call calculate() if unfinished.
    ValueList eval(EvalContext context);

    /**
     * Reset to no values and unfinished for this thread.
     *
     * If a child class overrides it, it probably wants to store the data it
     * resets in thread local data.  That is, as a node may be in use by
     * multiple threads simultaneously, represented distinct evaluations, any
     * calculation dependent data should be thread local, just as the values
     * of the node are.
     **/
    virtual void reset();

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
     * Preform any one time preparations needed for evaluation.
     *
     * This method is called after all transformations are complete but before
     * any evaluation.  It provides the environment of the node and should be
     * used to do any setup needed to calculate values.
     *
     * @param[in] environment Environment for evaluation.
     * @param[in] reporter    Reporter to report errors with.
     **/
    virtual void pre_eval(Environment environment, NodeReporter reporter);

protected:
    /**
     * Calculate value.
     *
     * Subclass classes should implement this to calculate and call
     * add_value() and finish() appropriately.
     *
     * It is important to make this thread safe if intending to use Predicate
     * in a multithreaded situation.
     *
     * This method will be called any time eval() is called while the node is
     * unfinished.  It will not be called on a finished node.
     *
     * @param [in] context Context of calculation.
     * @return Value of node.
     */
    virtual void calculate(EvalContext context) = 0;

    /**
     * Add a value (thread specific).
     *
     * Should only be called from calculate().
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
     * Should only be called from calculate().
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
     * Is node forwarding?
     **/
    bool is_forwarding() const;

    /**
     * Is node aliased?
     **/
    bool is_aliased() const;

    /**
     * Finish node as true.
     *
     * Convenience method for finishing the current node with a truthy value.
     **/
    void finish_true();

   /**
    * Finish node as false.
    *
    * Convenience method for finishing the current node with a falsy value.
    **/
   void finish_false();

private:
    /**
     * Remove this from parents of @a child.
     *
     * @param[in] child Child to unlink from.
     * @throw IronBee::einval if not in @a child's parent list.
     **/
    void unlink_from_child(const node_p& child) const;

    //! Per-thread value information.
    class per_thread_t;

    //! Fetch per-thread info for this thread.
    per_thread_t& lookup_value();
    //! Fetch per-thread info for this thread.
    const per_thread_t& lookup_value() const;

    //! Thread specific value.
    boost::thread_specific_ptr<per_thread_t> m_value;

    //! List of parents (note weak pointers).
    weak_node_list_t m_parents;
    //! List of children.
    node_list_t m_children;
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
 * name() and calculate().  Subclasses may also define add_child(),
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
    friend class String;
    friend class Null;
    friend class Integer;
    friend class Float;

private:
    //! Private constructor to limit subclassing.
    Literal() {}
public:
    //! @throw IronBee::einval always.
    virtual void add_child(const node_p&);
    //! @throw IronBee::einval always.
    virtual void remove_child(const node_p&);
    //! @throw IronBee::einval always.
    virtual void replace_child(const node_p& child, const node_p& with);
};

/**
 * String literal: Literal node representing a string.
 *
 * This class may not be subclassed.
 **/
class String :
    public Literal,
    public virtual Impl::Final
{
public:
    /**
     * Constructor.
     *
     * @param[in] value Value of node.
     **/
    explicit
    String(const std::string& value);

    //! Value as string.
    std::string value_as_s() const
    {
        return m_value_as_s;
    }

    //! S-expression: 'value'
    virtual const std::string& to_s() const
    {
        return m_s;
    }

    /**
     * Escape a string.
     *
     * Adds backslashes before all single quotes and backslashes.
     *
     * @param[in] s String to escape.
     * @return Escaped string.
     **/
    static std::string escape(const std::string& s);

protected:
    //! See Node::calculate()
    virtual void calculate(EvalContext context);

private:
    //! Value as a C++ string.
    const std::string m_value_as_s;
    //! S-expression.
    const std::string m_s;
    //! Memory pool to create field value from.  Alias of m_value_as_s.
    boost::shared_ptr<IronBee::ScopedMemoryPool> m_pool;
    //! Value added by calculate().
    Value m_value_as_field;
};

/**
 * Null literal: Literal node representing the null value.
 *
 * This class may not be subclassed.
 **/
class Null :
    public Literal,
    public virtual Impl::Final
{
public:
    //! S-expression: null
    virtual const std::string& to_s() const;

protected:
    //! See Node::calculate()
    virtual void calculate(EvalContext context);
};

/**
 * Integer literal: Literal node representing integer literal.
 *
 * This class may not be subclassed.
 **/
class Integer :
    public Literal,
    public virtual Impl::Final
{
public:
    /**
     * Constructor.
     *
     * @param[in] value Value of node.
     **/
    explicit
    Integer(int64_t value);

    //! Value as integer.
    int64_t value_as_i() const
    {
        return m_value_as_i;
    }

    //! S-expression: n
    virtual const std::string& to_s() const
    {
        return m_s;
    }

protected:
    //! See Node::calculate()
    virtual void calculate(EvalContext context);

private:
    //! Value as integer.
    int64_t m_value_as_i;
    //! S-expression.
    const std::string m_s;
    //! Memory pool to create field value from.
    boost::shared_ptr<IronBee::ScopedMemoryPool> m_pool;
    //! Value added by calculate().
    Value m_value_as_field;
};

/**
 * Float literal: Literal node representing float literal.
 *
 * This class may not be subclassed.
 **/
class Float :
    public Literal,
    public virtual Impl::Final
{
public:
    /**
     * Constructor.
     *
     * @param[in] value Value of node.
     **/
    explicit
    Float(long double value);

    //! Value as float
    long double value_as_f() const
    {
        return m_value_as_f;
    }

    //! S-expression: n
    virtual const std::string& to_s() const
    {
        return m_s;
    }

protected:
    //! See Node::calculate()
    virtual void calculate(EvalContext context);

private:
    //! Value as integer.
    long double m_value_as_f;
    //! S-expression.
    const std::string m_s;
    //! Memory pool to create field value from.
    boost::shared_ptr<IronBee::ScopedMemoryPool> m_pool;
    //! Value added by calculate().
    Value m_value_as_field;
};

} // Predicate
} // IronBee

#endif
