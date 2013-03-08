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

/**
 * Shared pointer to Node.
 *
 * Nodes are intended to be used in DAG, which, by definition is acyclic.
 * Guaranteeing acyclicness is the responsibility of the user and violating it
 * will allow for shared pointer loops and memory leaks.
 **/
typedef boost::shared_ptr<Node> node_p;

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

    //! List of nodes.  See children() and parents()
    typedef std::list<node_p> node_list_t;

    //! String representation.
    virtual std::string to_s() const = 0;

    //! Hash of node.
    virtual size_t hash() const = 0;

    // Accessors are intentionally inline.

    //! Subclassren accessor -- const.
    const node_list_t& children() const
    {
        return m_children;
    }
    //! Parents accessor -- const.
    const node_list_t& parents() const
    {
        return m_parents;
    }
    //! Subclassren accessor -- non-const.
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
     * Subclass classes should implement this to calculate the value and then
     * call set_value() with it.
     *
     * @param [in] context Contex of calculation.
     */
    virtual void calculate(Context context) = 0;

    /**
     * Set value of node.
     *
     * Usually called from calculate().
     *
     * @param [in] v Value to set.
     */
    void set_value(Value v);

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
 * All Call nodes must have a name that is the same across all nodes of the
 * same subclass.  To enforce this, the CRTP is used.  A subclass C must
 * publically inherit @c Call<C> and define:
 *
 * @code
 * static const std::string class_name;
 * @endcode
 *
 * Generally, specific call classes inherit from OrderedCall or UnorderedCall
 * instead of Call.
 *
 * @sa OrderedCall
 * @sa UnorderedCall
 *
 * @tparam Subclass Subclass; see CRTP.
 **/
template <class Subclass>
class Call :
    public Node
{
public:
    //! Convert to string: (@a name children...)
    virtual std::string to_s() const;

    /**
     * Name accessor.
     *
     * Returns @c Subclass::class_name.
     */
    std::string name() const
    {
        return Subclass::class_name;
    }
};

/**
 * Node representing a function call where argument order matters.
 *
 * Call nodes where argument order matters should inherit from this class,
 * passing themselves as the template argument (see Call and CRTP).  They
 * must define a static @c class_name (see Call) and implement the
 * protected calculate() method (see Node).  The name() and hash() methods
 * will be implemented for them.
 *
 * @sa Call
 * @sa UnorderedCall
 * @sa Node
 *
 * @tparam Subclass Subclass; see CRTP.
 **/
template <class Subclass>
class OrderedCall :
     public Call<Subclass>
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
 * Call nodes where argument order does not matters should inherit from this
 * class, passing themselves as the template argument (see Call and CRTP).
 * They must define a static @c class_name (see Call) and implement the
 * protected calculate() method (see Node).  The name() and hash() methods
 * will be implemented for them.
 *
 * @sa Call
 * @sa OrderedCall
 * @sa Node
 *
 * @tparam Subclass Subclass; see CRTP.
 **/
template <class Subclass>
class UnorderedCall :
    public Call<Subclass>
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
 * DAG Node representing a string literal.
 *
 * This class is a fully defined Node class and can be instantiated directly
 * to represent a Literal node.
 *
 * @warn Literal nodes are expected to have no children.
 **/
class Literal :
    public Node
{
public:
    /**
     * Constructor.
     *
     * @param[in] s Value of node.
     **/
    explicit
    Literal(const std::string& s);

    //! Convert to string.
    virtual std::string to_s() const;

    //! Hash value.  Intentionally inline.
    virtual size_t hash() const
    {
        return m_hash;
    }

protected:
    //! See Node::calculate()
    virtual void calculate(Context context);

private:
    const size_t m_hash;
    const std::string m_s;
    IronBee::ConstField m_pre_value;
};

template <class Subclass>
std::string Call<Subclass>::to_s() const
{
    std::string r;
    r = "(" + name();
    BOOST_FOREACH(const node_p& child, this->children()) {
        r += " " + child->to_s();
    }
    r += ")";
    return r;
}

template <class Subclass>
OrderedCall<Subclass>::OrderedCall() :
    m_hash(0)
{
    // nop
}

template <class Subclass>
size_t OrderedCall<Subclass>::hash() const
{
    if (m_hash == 0) {
        BOOST_FOREACH(const node_p& child, this->children()) {
            // From boost::hash_combine.
            m_hash ^= child->hash() + 0x9e3779b9 + (m_hash << 6) +
                      (m_hash >> 2);
        }
        boost::hash_combine(m_hash, this->name());
    }
    return m_hash;
}

template <class Subclass>
UnorderedCall<Subclass>::UnorderedCall() :
    m_hash(0)
{
    // nop
}

template <class Subclass>
size_t UnorderedCall<Subclass>::hash() const
{
    if (m_hash == 0) {
        BOOST_FOREACH(const node_p& child, this->children()) {
            // Note: Commutative
            m_hash += child->hash();
        }
        boost::hash_combine(m_hash, this->name());
    }
    return m_hash;
}

} // DAG
} // Predicate
} // IronBee

#endif
