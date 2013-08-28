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
 * @brief Predicate --- DAG Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <predicate/dag.hpp>
#include <predicate/merge_graph.hpp>
#include <predicate/reporter.hpp>

#include <ironbeepp/engine.hpp>

#include <boost/lexical_cast.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {

namespace {
static IronBee::ScopedMemoryPool c_static_pool;
static const Value
    c_empty_string(
        IronBee::Field::create_byte_string(
            c_static_pool,
            "", 0,
            IronBee::ByteString::create(c_static_pool)
        )
    );

}

/**
 * Per-Thread node information.
 *
 * This class holds the node information that is specific to a single thread,
 * that is, the state of the node during an evaluation run.
 **/
class Node::per_thread_t
{
public:
    //! Constructor.
    per_thread_t() :
        m_pool("node value private pool"),
        m_finished(false),
        m_own_values(List<Value>::create(m_pool)),
        m_values(m_own_values)
    {
        // nop
    }

    //! Forward values and finished queries to node @a to.
    void forward(const node_p& to)
    {
        if (is_forwarding()) {
            BOOST_THROW_EXCEPTION(
                IronBee::einval() << errinfo_what(
                    "Can't forward a forwarded node."
                )
            );
        }
        if (is_finished()) {
            BOOST_THROW_EXCEPTION(
                IronBee::einval() << errinfo_what(
                    "Can't finish an already finished node."
                )
            );
        }
        if (! m_values.empty()) {
            BOOST_THROW_EXCEPTION(
                IronBee::einval() << errinfo_what(
                    "Can't combine existing values with forwarded values."
                )
            );
        }
        m_forward = to;
    }

    //! Read accessor for forward node.
    const node_p& forward_node() const
    {
        return m_forward;
    }

    //! Is node finished?
    bool is_finished() const
    {
        return m_forward ?
            m_forward->lookup_value().is_finished() :
            m_finished;
    }

    //! Is node forwarding?
    bool is_forwarding() const
    {
        return m_forward;
    }

    //! Is node aliased?
    bool is_aliased() const
    {
        return m_values != m_own_values;
    }

    //! Value list.
    ValueList values() const
    {
        return m_forward ?
            m_forward->lookup_value().values() :
            ValueList(m_values);
    }

    //! Reset node.
    void reset()
    {
        cout << "Node::reset()" << endl;
        m_forward.reset();
        m_finished = false;
        m_values = m_own_values;
        m_own_values.clear();
    }

    //! Add @a value to values list.
    void add_value(Value value)
    {
        if (is_forwarding()) {
            BOOST_THROW_EXCEPTION(
                einval() << errinfo_what(
                    "Can't add value to forwarded node."
                )
            );
        }
        if (is_finished()) {
            BOOST_THROW_EXCEPTION(
                IronBee::einval() << errinfo_what(
                    "Can't add value to finished node."
                )
            );
        }
        if (m_own_values != m_values) {
            BOOST_THROW_EXCEPTION(
                IronBee::einval() << errinfo_what(
                    "Can't add value to aliased node."
                )
            );
        }
        m_own_values.push_back(value);
    }

    //! Finish node.
    void finish()
    {
        if (is_forwarding()) {
            BOOST_THROW_EXCEPTION(
                IronBee::einval() << errinfo_what(
                    "Can't finish a forwarded node."
                )
            );
        }
        if (is_finished()) {
            BOOST_THROW_EXCEPTION(
                IronBee::einval() << errinfo_what(
                    "Can't finish an already finished node."
                )
            );
        }
        m_finished = true;
    }

    //! Alias value list @a other.
    void alias(ValueList other)
    {
        if (is_forwarding()) {
            BOOST_THROW_EXCEPTION(
                einval() << errinfo_what(
                    "Can't alias a forwarded node."
                )
            );
        }
        if (is_finished()) {
            BOOST_THROW_EXCEPTION(
                einval() << errinfo_what(
                    "Can't alias a finished node."
                )
            );
        }
        if (m_values != m_own_values) {
            BOOST_THROW_EXCEPTION(
                einval() << errinfo_what(
                    "Can't alias an aliased node."
                )
            );
        }
        m_values = other;
    }

private:
    //! What node forwarding to.
    node_p m_forward;
    //! Local memory pool.
    IronBee::ScopedMemoryPool m_pool;
    //! Is node finished.
    bool m_finished;
    //! Value list owned by node.
    List<Value> m_own_values;
    //! Value list to use for values; might be @ref m_own_values.
    ValueList m_values;
};

Node::Node()
{
    // nop
}

Node::~Node()
{
    // nop
}

Node::per_thread_t& Node::lookup_value()
{
    per_thread_t* v = m_value.get();
    if (! v) {
        v = new per_thread_t();
        m_value.reset(v);
    }
    return *v;
}

const Node::per_thread_t& Node::lookup_value() const
{
    static const per_thread_t empty_value;
    const per_thread_t* v = m_value.get();
    if (! v) {
        return empty_value;
    }
    else {
        return *v;
    }
}

ValueList Node::eval(EvalContext context)
{
    per_thread_t& v = lookup_value();
    if (v.is_forwarding()) {
        return v.forward_node()->eval(context);
    }

    if (! v.is_forwarding() && ! v.is_finished()) {
        calculate(context);
    }
    return v.values();
}

ValueList Node::values() const
{
    return lookup_value().values();
}

void Node::reset()
{
    lookup_value().reset();
}

bool Node::is_finished() const
{
    return lookup_value().is_finished();
}

bool Node::is_forwarding() const
{
    return lookup_value().is_forwarding();
}

bool Node::is_aliased() const
{
    return lookup_value().is_aliased();
}

void Node::add_value(Value value)
{
    lookup_value().add_value(value);
}

void Node::finish()
{
    lookup_value().finish();
}

void Node::alias(ValueList other)
{
    lookup_value().alias(other);
}

void Node::finish_true()
{
    if (! values().empty()) {
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << errinfo_what(
                "Can't finish a node as true if it already has values."
            )
        );
    }
    add_value(c_empty_string);
    finish();
}

void Node::finish_false()
{
    if (! values().empty()) {
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << errinfo_what(
                "Can't finish a node as false if it already has values."
            )
        );
    }
    finish();
}

void Node::forward(const node_p& other)
{
    lookup_value().forward(other);
}

void Node::add_child(const node_p& child)
{
    if (! child) {
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << errinfo_what(
                "Can't add a singular child."
            )
        );
    }

    m_children.push_back(child);
    child->m_parents.push_back(shared_from_this());
}

void Node::unlink_from_child(const node_p& child) const
{
    bool found_parent = false;
    for (
        weak_node_list_t::iterator i = child->m_parents.begin();
        i != child->m_parents.end();
        ++i
    ) {
        if (i->lock().get() == this) {
            found_parent = true;
            child->m_parents.erase(i);
            break;
        }
    }
    if (! found_parent) {
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << errinfo_what(
                "Not a parent of child."
            )
        );
    }
}

void Node::remove_child(const node_p& child)
{
    if (! child) {
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << errinfo_what(
                "Can't remove a singular child."
            )
        );
    }

    // Want to only remove first child.
    node_list_t::iterator i =
        find(m_children.begin(), m_children.end(), child);
    if (i == m_children.end()) {
        BOOST_THROW_EXCEPTION(
            IronBee::enoent() << errinfo_what(
                "No such child."
            )
        );
    }
    m_children.erase(i);

    unlink_from_child(child);
}

void Node::replace_child(const node_p& child, const node_p& with)
{
    if (! child) {
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << errinfo_what(
                "Can't replace a singular child."
            )
        );
    }
    if (! with) {
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << errinfo_what(
                "Can't replace with a singular child."
            )
        );
    }

    node_list_t::iterator i =
        find(m_children.begin(), m_children.end(), child);
    if (i == m_children.end()) {
        BOOST_THROW_EXCEPTION(
            IronBee::enoent() << errinfo_what(
                "No such child."
            )
        );
    }

    unlink_from_child(child);

    with->m_parents.push_back(shared_from_this());

    *i = with;
}

void Node::pre_transform(NodeReporter reporter) const
{
    validate(reporter);
}

bool Node::transform(
    MergeGraph&,
    const CallFactory&,
    NodeReporter
)
{
    return false;
}

void Node::post_transform(NodeReporter reporter) const
{
    validate(reporter);
}

bool Node::validate(NodeReporter reporter) const
{
    return true;
}

void Node::pre_eval(Environment environment, NodeReporter reporter)
{
    // nop
}

bool Node::is_literal() const
{
    return dynamic_cast<const Literal*>(this);
}

ostream& operator<<(ostream& out, const Node& node)
{
    out << node.to_s();
    return out;
}

String::String(const string& value) :
    m_value_as_s(value),
    m_s("'" + String::escape(value) + "'"),
    m_pool(new IronBee::ScopedMemoryPool("IronBee::Predicate::String")),
    m_value_as_field(
        IronBee::Field::create_byte_string(
            *m_pool,
            "", 0,
            IronBee::ByteString::create_alias(
                *m_pool,
                m_value_as_s
            )
        )
    )
{
    // nop
}

string String::escape(const std::string& s)
{
    string escaped;
    size_t pos = 0;
    size_t last_pos = 0;

    pos = s.find_first_of("'\\", pos);
    while (pos != string::npos) {
        escaped += s.substr(last_pos, pos);
        escaped += '\\';
        escaped += s[pos];
        last_pos = pos + 1;
        pos = s.find_first_of("'\\", last_pos);
    }
    escaped += s.substr(last_pos, pos);
    return escaped;
}

void String::calculate(EvalContext)
{
    add_value(m_value_as_field);
    finish();
}

const string& Null::to_s() const
{
    static std::string s_null("null");
    return s_null;
}

void Null::calculate(EvalContext)
{
    finish_false();
}

Integer::Integer(int64_t value) :
    m_value_as_i(value),
    m_s(boost::lexical_cast<string>(value)),
    m_pool(new IronBee::ScopedMemoryPool("IronBee::Predicate::Integer")),
    m_value_as_field(
        IronBee::Field::create_number(
            *m_pool,
            "", 0,
            value
        )
    )
{
    // nop
}

void Integer::calculate(EvalContext)
{
    add_value(m_value_as_field);
    finish();
}

Float::Float(long double value) :
    m_value_as_f(value),
    m_s(boost::lexical_cast<string>(value)),
    m_pool(new IronBee::ScopedMemoryPool("IronBee::Predicate::Float")),
    m_value_as_field(
        IronBee::Field::create_float(
            *m_pool,
            "", 0,
            value
        )
    )
{
    // nop
}

void Float::calculate(EvalContext)
{
    add_value(m_value_as_field);
    finish();
}

// Don't use recalculate_s() as we don't want to update parents.
Call::Call() :
    m_calculated_s(false)
{
    // nop
}

const std::string& Call::to_s() const
{
    if (! m_calculated_s) {
        recalculate_s();
    }
    return m_s;
}

void Call::add_child(const node_p& child)
{
    Node::add_child(child);
    reset_s();
}

void Call::remove_child(const node_p& child)
{
    Node::remove_child(child);
    reset_s();
}

void Call::replace_child(const node_p& child, const node_p& with)
{
    Node::replace_child(child, with);
    reset_s();
}

void Call::recalculate_s() const
{
    m_s.clear();
    m_s = "(" + name();
    BOOST_FOREACH(const node_p& child, this->children()) {
        m_s += " " + child->to_s();
    }
    m_s += ")";

    BOOST_FOREACH(const weak_node_p& weak_parent, parents()) {
        call_p parent = boost::dynamic_pointer_cast<Call>(
            weak_parent.lock()
        );
        if (! parent) {
            BOOST_THROW_EXCEPTION(
                IronBee::einval() << errinfo_what(
                    "Have non-Call parent."
                )
            );
        }
        parent->reset_s();
    }
    m_calculated_s = true;
}

void Call::reset_s() const
{
    BOOST_FOREACH(const weak_node_p& weak_parent, parents()) {
        call_p parent = boost::dynamic_pointer_cast<Call>(
            weak_parent.lock()
        );
        if (! parent) {
            BOOST_THROW_EXCEPTION(
                IronBee::einval() << errinfo_what(
                    "Have non-Call parent."
                )
            );
        }
        parent->reset_s();
    }
    m_calculated_s = false;
}

void Literal::add_child(const node_p&)
{
    BOOST_THROW_EXCEPTION(
        IronBee::einval() << errinfo_what(
            "Literals can not have children."
        )
    );
}

void Literal::remove_child(const node_p&)
{
    BOOST_THROW_EXCEPTION(
        IronBee::einval() << errinfo_what(
            "Literals can not have children."
        )
    );
}

void Literal::replace_child(const node_p& child, const node_p& with)
{
    BOOST_THROW_EXCEPTION(
        IronBee::einval() << errinfo_what(
            "Literals can not have children."
        )
    );
}

} // Predicate
} // IronBee
