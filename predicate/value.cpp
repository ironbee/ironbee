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
 * @brief Predicate --- Value Utilities Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <predicate/value.hpp>
#include <predicate/parse.hpp>

#include <boost/algorithm/string/join.hpp>
#include <boost/foreach.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {
    
Value::Value()
{
    // nop
}

Value::Value(ConstField field) :
    m_field(field)
{
    // nop
}

Value::Value(ib_type ib) :
    m_field(ib)
{
    // nop
}

Value Value::create_number(MemoryManager mm, int64_t num)
{
    return create_number(mm, "", 0, num);
}

Value Value::create_number(
    MemoryManager mm,
    const char*   name,
    size_t        name_length,
    int64_t       num
)
{
    return Value(Field::create_number(mm, name, name_length, num));
}

Value Value::create_float(MemoryManager mm, long double f)
{
    return create_float(mm, "", 0, f);
}

Value Value::create_float(
    MemoryManager mm,
    const char*   name,
    size_t        name_length,
    long double   f
)
{
    return Value(Field::create_float(mm, name, name_length, f));
}

Value Value::create_string(MemoryManager mm, ConstByteString s)
{
    return create_string(mm, "", 0, s);
}

Value Value::create_string(
    MemoryManager   mm,
    const char*     name,
    size_t          name_length,
    ConstByteString s
)
{
    return Value(Field::create_byte_string(mm, name, name_length, s));
}

Value Value::alias_list(
    MemoryManager    mm,
    ConstList<Value> l
)
{
    return alias_list(mm, "", 0, l);
}

Value Value::alias_list(
    MemoryManager    mm,
    const char*      name,
    size_t           name_length,
    ConstList<Value> l
)
{
    return Value(Field::create_no_copy_list(
        mm, 
        name, name_length, 
        // Value does not allow access to non-const version.
        List<Value>::remove_const(l)
    ));
}

Value Value::dup(MemoryManager mm) const
{
    return dup(mm, name(), name_length());
}

Value Value::dup(
    MemoryManager mm,
    const char*   name,
    size_t        name_length
) const
{
    if (type() == LIST) {
        List<Value> l = List<Value>::create(mm);
        Value v = alias_list(mm, name, name_length, l);
        BOOST_FOREACH(Value subvalue, as_list()) {
            l.push_back(subvalue.dup(mm));
        }
        return v;
    }
    else {
        return Value(m_field.dup(mm, name, name_length));
    }
}

Value::operator unspecified_bool_type() const
{
    return (
        m_field && 
        (type() != LIST || ! as_list().empty())
    ) ? unspecified_bool : NULL;
}

bool Value::is_null() const
{
    return ! m_field;
}

namespace {
    
string valuelist_to_string(ConstList<Value> values)
{
    list<string> string_values;
    BOOST_FOREACH(const Value& v, values) {
        string_values.push_back(v.to_s());
    }
    return "[" + boost::algorithm::join(string_values, " ") + "]";
}

} // Anonymous

const string Value::to_s() const
{
    if (! m_field) {
        return ":";
    }
    string string_value;
    if (name_length() > 0) {
        string_value += emit_literal_name(
            string(name(), name_length())
        );
        string_value += ":";
    }
    if (type() == LIST) {
        string_value +=
            valuelist_to_string(as_list());
    }
    else if (type() == STRING) {
        string_value += "'" + emit_escaped_string(m_field.to_s()) + "'";
    }
    else {
        string_value += m_field.to_s();
    }

    return string_value;
}

int64_t Value::as_number() const
{
    return m_field.value_as_number();
}

long double Value::as_float() const
{
    return m_field.value_as_float();
}

ConstByteString Value::as_string() const
{
    return m_field.value_as_byte_string();
}

ConstList<Value> Value::as_list() const
{
    return m_field.value_as_list<Value>();
}

const char* Value::name() const
{
    return m_field.name();
}

size_t Value::name_length() const
{
    return m_field.name_length();
}

Value::type_e Value::type() const
{
    return static_cast<type_e>(m_field.type());
}

ostream& operator<<(ostream& o, const Value& v)
{
    o << v.to_s();
    return o;
}

} // Predicate
} // IronBee
