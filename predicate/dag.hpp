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

#include <boost/foreach.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <iostream>
#include <list>
#include <string>

#include <ironbeepp/field.hpp>
#include <ironbeepp/transaction.hpp>

namespace IronBee {
namespace Predicate {

typedef IronBee::ConstField Value;
typedef IronBee::ConstTransaction Context;

class Node;
class Call;
class Literal;
class Null;
class String;

// Defined in reporter.hpp
class NodeReporter;
// Defined in merge_graph.hpp
class MergeGraph;

/// @cond Impl
namespace Impl {

/**
 * Used to make certain classes unsubclassable.
 *
 * See C++ FAQ.
 **/
class Final
{
    // Classes to be final.
    friend class Null;
    friend class String;
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
 * @sa Call
 * @sa Literal
 */
class Node :
    public boost::enable_shared_from_this<Node>
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

    //! True iff has value.
    bool has_value() const
    {
        return m_has_value;
    }

    //! True iff node is a literal, in which case eval(Context()) is valid.
    bool is_literal() const;

    //! Return value, calculating if needed.
    Value eval(Context context);

    //! Return value, throwing if none.
    Value value() const;

    //! Reset to valueless.
    void reset();

    /**
     * Perform pre-transformation validations.
     *
     * This method may be overridden by a child.  Default behavior is to do
     * nothing.
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
     * @param[in] reporter    Reporter to use for errors or warnings.
     * @param[in] merge_graph MergeGraph used to change the DAG.
     * @return true iff any changes were made.
     **/
    virtual bool transform(
        NodeReporter reporter,
        MergeGraph&  merge_graph
    );

    /**
     * Perform post-transformation validations.
     *
     * See pre_transform() for additional discussion.
     * @param[in] reporter Reporter to use for errors or warnings.
     **/
    virtual void post_transform(NodeReporter reporter) const;

protected:
    /**
     * Calculate value.
     *
     * Subclass classes should implement this to calculate and return the
     * value.
     *
     * @param [in] context Contex of calculation.
     * @return Value of node.
     */
    virtual Value calculate(Context context) = 0;

private:
    /**
     * Remove this from parents of @a child.
     *
     * @param[in] child Child to unlink from.
     * @throw IronBee::einval if not in @a child's parent list.
     **/
    void unlink_from_child(const node_p& child) const;

    bool             m_has_value;
    Value            m_value;
    weak_node_list_t m_parents;
    node_list_t      m_children;
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
 * Literal node: no children and value independent of Context.
 *
 * This class may not be subclassed.
 **/
class Literal :
    public Node
{
    friend class String;
    friend class Null;

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
     * @param[in] s Value of node.
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
    virtual Value calculate(Context context);

private:
    //! Value as a C++ string.
    const std::string         m_value_as_s;
    //! S-expression.
    const std::string         m_s;
    //! Memory pool to create field value from.  Alias of m_value_as_s.
    IronBee::ScopedMemoryPool m_pool;
    //! Value returned by calculate().
    IronBee::ConstField       m_value_as_field;
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
    virtual Value calculate(Context context);
};

} // Predicate
} // IronBee

#endif
