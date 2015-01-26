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
 * @brief Predicate --- Parse Implementation
 *
 * @author Christopher Alfeld <calfeld@qualytext.com>
 */

#include <ironbee/predicate/parse.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/shared_ptr.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {

namespace {

/**
 * Throw @ref einval error with message @a msg at position @a i.
 *
 * @param [in] i   Current position.
 * @param [in] msg Error message.
 */
void error(size_t i, const string& msg)
{
    BOOST_THROW_EXCEPTION(
        einval() << errinfo_what(
            msg + " at position " + boost::lexical_cast<string>(i)
        )
    );
}

/**
 * Increment @a i and require that @a i < @a length after increment.
 *
 * @param [in, out] i      Index to advance.
 * @param [in]      length Limit on @a i.
 * @param [in]      msg    Error message to use if @a i + 1 == @a length.
 */
void advance(size_t& i, size_t length, const string& msg)
{
    ++i;
    if (i >= length) {
        error(i, msg);
    }
}

bool first_name_char(char c)
{
    return
        ( c >= 'a' && c <= 'z' ) ||
        ( c >= 'A' && c <= 'Z' ) ||
        c == '_'
        ;
}

bool name_char(char c)
{
    return first_name_char(c) ||
        ( c >= '0' && c <= '9' ) ||
        c == '.' || c == '-'
        ;
}

bool num_char(char c)
{
    return
        ( c >= '0' && c <= '9' )
        ;
}

string parse_name(
    const string& text,
    size_t&       i
)
{
    size_t length = text.length();
    string value;

    if (! first_name_char(text[i])) {
        error(i, string("Invalid first name char: ") + text[i]);
    }
    while (name_char(text[i])) {
        value += text[i];
        advance(i, length, "Unterminated name");
    }

    return value;
}

List<Value> parse_list_value(
    const string& text,
    size_t&       i,
    MemoryManager mm
)
{
    List<Value> list = List<Value>::create(mm);
    size_t length = text.length();

    if (text[i] != '[') {
        error(i, string("Expect [ at beginning of list but found: ") + text[i]);
    }
    advance(i, length, "Unterminated list literal");

    while (text[i] != ']') {
        while (text[i] == ' ') {
            advance(i, length, "Unterminated list literal");
        }
        if (text[i] == ']') {
            break;
        }
        Value literal = parse_literal_value(text, i, mm);
        list.push_back(literal);
        advance(i, length, "Unterminated list literal");
        if (text[i] != ' ' && text[i] != ']') {
            error(i, string("Expected end of list or space but found: ") + text[i]);
        }
    }

    return list;
}

Value parse_list(
    const string& text,
    size_t&       i,
    MemoryManager mm,
    const string& name = ""
)
{
    return Value::alias_list(
        mm,
        mm.strdup(name.data()), name.length(),
        parse_list_value(text, i, mm)
    );
}

string parse_string_value(
    const string& text,
    size_t&       i
)
{
    size_t length = text.length();
    bool escape = false;
    string value;

    if (text[i] != '\'') {
        error(i, "Expected '");
    }
    advance(i, length, "Unterminated literal");
    while (text[i] != '\'' || escape) {
        if (text[i] == '\\' && ! escape) {
            escape = true;
        }
        else {
            value += text[i];
            escape = false;
        }
        advance(i, length, "Unterminated literal");
    }

    return value;
}

Value parse_string(
    const string& text,
    size_t&       i,
    MemoryManager mm,
    const string& name = ""
)
{
    return Value::create_string(
        mm, mm.strdup(name.data()), name.length(),
        ByteString::create(mm, parse_string_value(text, i))
    );
}

Value parse_number(
    const string& text,
    size_t&       i,
    MemoryManager mm,
    const string& name = ""
)
{
    bool have_dot = false;
    size_t initial_i = i;
    size_t length = text.length();

    if (text[i] == '-') {
        advance(i, length, "Unterminated literal");
    }

    while (
        i < length &&
        (num_char(text[i]) || text[i] == '.')
    ) {
        if (text[i] == '.') {
            if (have_dot) {
                error(i, "Multiple dots in numeric.");
            }
            have_dot = true;
        }
        ++i;
    }

    string value = text.substr(initial_i, i - initial_i);
    // Reduce i to match caller expectation: that i points to final
    // character of literal.
    --i;

    if (have_dot) {
        long double fvalue;
        try {
            fvalue = boost::lexical_cast<long double>(value);
        }
        catch (boost::bad_lexical_cast) {
            error(i, "Could not convert to float.");
        }
        return Value::create_float(mm, mm.strdup(name.data()), name.length(), fvalue);
    }
    else {
        int64_t ivalue;
        try {
            ivalue = boost::lexical_cast<int64_t>(value);
        }
        catch (boost::bad_lexical_cast) {
            error(i, "Could not convert to integer.");
        }
        return Value::create_number(mm, mm.strdup(name.data()), name.length(), ivalue);
    }
}

}

Value parse_literal_value(
    const string& text,
    size_t&       i,
    MemoryManager mm
)
{
    size_t length = text.length();
    string name;

    // Name or String Value
    switch (text[i]) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '-':
        case ':':
        case '[':
            // Unnamed literal.
            break;
        default: {
            // Name or String Value
            if (text[i] == '\'') {
                // String name or string literal.
                name = parse_string_value(text, i);
                if (i == length - 1 || text[i+1] != ':') {
                    return Value::create_string(
                        mm,
                        ByteString::create(mm, name)
                    );
                }
                else {
                    advance(i, length, "Unterminated named literal");
                }
            }
            else if (first_name_char(text[i])) {
                // Name name
                name = parse_name(text, i);
            }
            else {
                error(i, string("Unexpected character ") + text[i]);
            }

            if (text[i] != ':') {
                error(i, string("Expected :, found ") + text[i]);
            }
            advance(i, length, "Unterminated named literal");
        }
    }

    // Value
    switch (text[i]) {
        case ':':
            // Null.
            return Value();
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '-':
            // Number
            return parse_number(text, i, mm, name);
        case '[':
            // List
            return parse_list(text, i, mm, name);
        case '\'':
            // String
            return parse_string(text, i, mm, name);
        default:
            error(i, string("Unexpected character ") + text[i]);
    }

    assert(! "Unreachable.");
}

node_p parse_literal(
    const string& text,
    size_t&       i
)
{
    boost::shared_ptr<ScopedMemoryPoolLite> mpl(new ScopedMemoryPoolLite());
    Value value = parse_literal_value(text, i, *mpl);
    return node_p(new Literal(mpl, value));
}

// The following could be more cleanly implemented recursively, but would
// limit stack depth.
node_p parse_call(
    const string&      text,
    size_t&            i,
    const CallFactory& factory
)
{
    node_p current;
    node_p top;
    size_t length = text.length();
    bool done = false;

    if (length == 0) {
        return node_p();
    }

    while (i < length && ! done) {
        switch (text[i]) {
            case ' ':
                advance(i, length, "Unterminated call");
                break;
            case '(': {
                string op;
                advance(i, length, "Unterminated call");
                while (name_char(text[i])) {
                    op += text[i];
                    advance(i, length, "Unterminated call");
                }
                if (op.empty()) {
                    error(i, "Missing operation");
                }
                node_p n = factory(op);
                if (! top) {
                    // Very important to keep all our nodes in memory.
                    top = n;
                }
                if (current) {
                    current->add_child(n);
                }
                current = n;
                break;
            }
            case ')':
                if (! current) {
                    error(i, "Too many )");
                }
                else if (current->parents().empty()) {
                    done = true;
                }
                else {
                    current = current->parents().front().lock();
                    advance(i, length, "Expected )");
                }
                break;
            default: {
                if (! current) {
                    error(i, "Naked literal");
                }
                current->add_child(parse_literal(text, i));
                assert(text[i] == '\'' || text[i] == ']' || num_char(text[i]) || text[i] == ':');
                advance(i, length, "Unterminated call");
                break;
            }
        }
    }
    if (! top) {
        error(i, "Unterminated call");
    }
    assert(current == top);
    return top;
}

string emit_escaped_string(const string& text)
{
    string escaped;
    size_t pos = 0;
    size_t last_pos = 0;

    pos = text.find_first_of("'\\", pos);
    while (pos != string::npos) {
        escaped += text.substr(last_pos, pos - last_pos);
        escaped += '\\';
        escaped += text[pos];
        last_pos = pos + 1;
        pos = text.find_first_of("'\\", last_pos);
    }
    escaped += text.substr(last_pos);
    return escaped;
}

string emit_literal_name(const std::string& name)
{
    bool is_string = ! first_name_char(name[0]);
    size_t length = name.length();
    for (size_t i = 1; ! is_string && i < length; ++i) {
        is_string = ! name_char(name[i]);
    }

    if (is_string) {
        return "'" + emit_escaped_string(name) + "'";
    }
    else {
        return name;
    }
}

} // Predicate
} // IronBee
