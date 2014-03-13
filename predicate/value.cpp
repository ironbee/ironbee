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

string valuelist_to_string(ConstList<Value> values)
{
    list<string> string_values;
    BOOST_FOREACH(const Value& v, values) {
        string_values.push_back(value_to_string(v));
    }
    return "[" + boost::algorithm::join(string_values, " ") + "]";
}

string value_to_string(Value value)
{
	if (! value) {
		return ":";
	}
    string string_value;
    if (value.name_length() > 0) {
        string_value += emit_literal_name(
			string(value.name(), value.name_length())
		);
        string_value += ":";
    }
    if (value.type() == Value::LIST) {
        string_value +=
            valuelist_to_string(value.value_as_list<Value>());
    }
    else if (value.type() == Value::BYTE_STRING) {
        string_value += "'" + emit_escaped_string(value.to_s()) + "'";
    }
    else {
        string_value += value.to_s();
    }

    return string_value;
}

} // Predicate
} // IronBee
