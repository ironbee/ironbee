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
#include <boost/functional/hash.hpp>
#include <boost/shared_ptr.hpp>

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
 * for value and evaluation, edges (as child and parent lists), and hashing
 * (used to detect equivalent nodes).  Subclasses are responsible for
 * defining how to calculate value and hash, as well as a string
 * representation.
 *
 * @sa Call
 * @sa Literal
 */
class Node
{
public:
    //! Constructor.
    Node();

    //! Destructor.
    virtual ~Node();

    //! List of nodes.  See children() and parents().
    typedef std::list<node_p> node_list_t;

    //! String representation.
    virtual std::string to_s() const = 0;

    //! Hash of node.
    virtual size_t hash() const = 0;

    //! True iff node can calculate value without context.
    virtual bool is_static() const
    {
        return false;
    }

    // Accessors are intentionally inline.

    //! Children accessor -- const.
    const node_list_t& children() const
    {
        return m_children;
    }
    //! Parents accessor -- const.
    const node_list_t& parents() const
    {
        return m_parents;
    }
    //! Children accessor -- non-const.
    node_list_t& children()
    {
        return m_children;
    }
    //! Parents accessor -- non-const.
    node_list_t& parents()
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
    bool        m_has_value;
    Value       m_value;
    node_list_t m_parents;
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
    //! Convert to string: (@a name children...)
    virtual std::string to_s() const;

    /**
     * Name accessor.
     */
    virtual std::string name() const = 0;
};

/**
 * Node representing a function call where argument order matters.
 *
 * @sa Call
 * @sa UnorderedCall
 * @sa Node
 **/
class OrderedCall :
     public Call
{
public:
    //! Constructor.
    OrderedCall();

    /**
     * Hash of node.
     *
     * In contrast to UnorderedCall::hash(), reordering the arguments will
     * change the hash.
     */
    virtual size_t hash() const;

private:
    //! Cache of hash.  Mutable as does not reflect state.
    mutable size_t m_hash;
};

/**
 * Node representing a function call where argument order does not matter.
 *
 * @sa Call
 * @sa OrderedCall
 * @sa Node
 **/
class UnorderedCall :
    public Call
{
public:
    //! Constructor.
    UnorderedCall();

    /**
     * Hash of node.
     *
     * In contrast to OrderedCall::hash(), reordering the arguments will
     * *not* change the hash.
     */
    virtual size_t hash() const;

private:
    //! Cache of hash.  Mutable as does not reflect state.
    mutable size_t m_hash;
};

/**
 * Literal node: no children and value independent of Context.
 **/
class Literal :
    public Node
{
public:
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
    StringLiteral(const std::string& s);

    //! Convert to string.
    virtual std::string to_s() const;

    //! Hash value.  Intentionally inline.
    virtual size_t hash() const
    {
        return m_hash;
    }

protected:
    //! See Node::calculate()
    virtual Value calculate(Context context);

private:
    const size_t m_hash;
    const std::string m_s;
    IronBee::ScopedMemoryPool m_pool;
    IronBee::ConstField m_pre_value;
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
    //! Convert to string.
    virtual std::string to_s() const;

    //! Hash value.  Intentionally inline.
    virtual size_t hash() const
    {
        return 0;
    }

protected:
    //! See Node::calculate()
    virtual Value calculate(Context context);
};

} // DAG
} // Predicate
} // IronBee

#endif
