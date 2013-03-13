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
namespace DAG {

typedef IronBee::ConstField Value;
typedef IronBee::ConstTransaction Context;

class Node;
class Call;
class Literal;

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

/**
 * A node in the predicate DAG.
 *
 * Nodes make up the predicate DAG.  They also appear in the expressions trees
 * that are merged together to construct the DAG.  The base classes provides
 * for value and evaluation, edges (as child and parent lists).
 * Subclasses are responsible for defining how to calculate value as
 * well as a string representation.
 *
 * @sa Call
 * @sa Literal
 */
class Node :
    public boost::enable_shared_from_this<Node>
{
public:
    //! Constructor.
    Node();

    //! Destructor.
    virtual ~Node();

    //! List of nodes.  See children().
    typedef std::list<node_p> node_list_t;

    //! Weak list of nodes.  See parents().
    typedef std::list<weak_node_p> weak_node_list_t;

    //! S-expression.
    virtual std::string to_s() const = 0;

    //! True iff node can calculate value without context.
    virtual bool is_static() const
    {
        return false;
    }

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

    //! Return value, calculating if needed.
    Value eval(Context context);

    //! Return value, throwing if none.
    Value value() const;

    //! Reset to valueless.
    void reset();

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
 * Generally, specific call classes inherit from OrderedCall or UnorderedCall
 * instead of Call.
 *
 * @sa OrderedCall
 * @sa UnorderedCall
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

    //! S-expression: (@c name children...)
    virtual std::string to_s() const;

    /**
     * Name accessor.
     */
    virtual std::string name() const = 0;

private:
    //! Recalculate m_s.
    void recalculate_s();

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
 **/
class Literal :
    public Node
{
public:
    //! @throw IronBee::einval always.
    virtual void add_child(const node_p&);
    //! @throw IronBee::einval always.
    virtual void remove_child(const node_p&);

    //! Is static!
    virtual bool is_static() const
    {
        return true;
    }
};

/**
 * DAG Node representing a string literal.
 *
 * This class is a fully defined Node class and can be instantiated directly
 * to represent a Literal node.
 *
 * @warn Literal nodes should not be subclassed.
 * @warn Literal nodes are expected to have no children.
 **/
class StringLiteral :
    public Literal
{
public:
    /**
     * Constructor.
     *
     * @param[in] s Value of node.
     **/
    explicit
    StringLiteral(const std::string& value);

    //! Value as string.
    std::string value_as_s() const
    {
        return m_value_as_s;
    }

    //! S-expression: 'value'
    virtual std::string to_s() const
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
 * DAG Node representing a NULL value.
 *
 * This class is a fully defined Node class and can be instantiated directly
 * to represent a Null node.
 *
 * @warn Null nodes should not be subclassed.
 * @warn Null nodes are expected to have no children.
 **/
class Null :
    public Literal
{
public:
    //! S-expression: null
    virtual std::string to_s() const;

protected:
    //! See Node::calculate()
    virtual Value calculate(Context context);
};

} // DAG
} // Predicate
} // IronBee

#endif
