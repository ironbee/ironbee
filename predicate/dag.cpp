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

#include "dag.hpp"
#include "reporter.hpp"
#include "merge_graph.hpp"

#include <ironbeepp/engine.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {

Node::Node() :
    m_has_value(false)
{
    // nop
}

Node::~Node()
{
    // nop
}

Value Node::eval(EvalContext context)
{
    if (! has_value()) {
        m_value = calculate(context);
        m_has_value = true;
    }
    return value();
}

Value Node::value() const
{
    if (! has_value()) {
        BOOST_THROW_EXCEPTION(
            einval() << errinfo_what(
                "Value asked of valueless node."
            )
        );
    }
    return m_value;
}

void Node::reset()
{
    m_value = IronBee::ConstField();
    m_has_value = 0;
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
    // nop
}

bool Node::transform(
    NodeReporter,
    MergeGraph&,
    const CallFactory&
)
{
    return false;
}

void Node::post_transform(NodeReporter reporter) const
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

Value String::calculate(EvalContext)
{
    return m_value_as_field;
}

const string& Null::to_s() const
{
    static std::string s_null("null");
    return s_null;
}

Value Null::calculate(EvalContext)
{
    return Value();
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
