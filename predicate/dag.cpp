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

#include <ironbeepp/engine.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {
namespace DAG {

Node::Node() :
    m_has_value(false)
{
    // nop
}

Node::~Node()
{
    // nop
}

Value Node::eval(Context context)
{
    if (! has_value()) {
        calculate(context);
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

void Node::set_value(Value v)
{
    m_value     = v;
    m_has_value = true;
}

ostream& operator<<(ostream& out, const Node& node)
{
    out << node.to_s();
    return out;
}

StringLiteral::StringLiteral(const string& s) :
    m_hash(boost::hash_value(s)), m_s(s),
    m_pool("IronBee::Predicate::DAG::StringLiteral")
{
    // nop
}

string StringLiteral::to_s() const
{
    string escaped;
    size_t pos = 0;
    size_t last_pos = 0;

    pos = m_s.find_first_of("'\\", pos);
    while (pos != string::npos) {
        escaped += m_s.substr(last_pos, pos);
        escaped += '\\';
        escaped += m_s[pos];
        last_pos = pos + 1;
        pos = m_s.find_first_of("'\\", last_pos);
    }
    escaped += m_s.substr(last_pos, pos);
    return "'" + escaped + "'";
}

void StringLiteral::calculate(Context)
{
    if (! m_pre_value) {
        m_pre_value = IronBee::Field::create_byte_string(
            m_pool,
            "", 0,
            IronBee::ByteString::create_alias(
                m_pool,
                m_s
            )
        );
    }
    set_value(m_pre_value);
}

string Null::to_s() const
{
    return "null";
}

void Null::calculate(Context)
{
    set_value(Value());
}

std::string Call::to_s() const
{
    std::string r;
    r = "(" + name();
    BOOST_FOREACH(const node_p& child, this->children()) {
        r += " " + child->to_s();
    }
    r += ")";
    return r;
}

OrderedCall::OrderedCall() :
    m_hash(0)
{
    // nop
}

size_t OrderedCall::hash() const
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

UnorderedCall::UnorderedCall() :
    m_hash(0)
{
    // nop
}

size_t UnorderedCall::hash() const
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
