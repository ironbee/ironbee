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
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <predicate/parse.hpp>

#include <boost/lexical_cast.hpp>

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

bool name_char(char c)
{
    return
        ( c >= 'a' && c <= 'z' ) ||
        ( c >= 'A' && c <= 'Z' ) ||
        ( c >= '0' && c <= '9' ) ||
        c == '_'
        ;
}

bool num_char(char c)
{
    return
        ( c >= '0' && c <= '9' )
        ;
}

/**
 * As parse_literal() but return Value allocated from @a mm.
 *
 * @param[in]      text Text to parse.
 * @param[in, out] i    Index to advance.
 * @param[in]      mm   Memory manager to allocate Values from.
 * @returns Values.
 **/
ValueList parse_literal_values(
    const std::string& text,
    size_t&            i,
    MemoryManager      mm
)
{
    List<Value> values = List<Value>::create(mm);

    size_t length = text.length();
    bool escape = false;
    string value;

    // Null Literal
    if (text.substr(i, 4) == "null") {
        i += 3;
        return values;
    }

    // Number Literal
    if (num_char(text[i]) || text[i] == '-') {
        bool have_dot = false;
        size_t initial_i = i;

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

        value = text.substr(initial_i, i - initial_i);
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
            values.push_back(Field::create_float(mm, "", 0, fvalue));
            return values;
        }
        else {
            int64_t ivalue;
            try {
                ivalue = boost::lexical_cast<int64_t>(value);
            }
            catch (boost::bad_lexical_cast) {
                error(i, "Could not convert to integer.");
            }
            values.push_back(Field::create_number(mm, "", 0, ivalue));
            return values;
        }
    }

    // String Literal
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
    values.push_back(
        Field::create_byte_string(mm, "", 0,
            ByteString::create(mm, value)
        )
    );
    return values;
}

}

node_p parse_literal(
    const std::string& text,
    size_t&            i
)
{
    boost::shared_ptr<ScopedMemoryPoolLite> mpl(new ScopedMemoryPoolLite());
    ValueList values = parse_literal_values(text, i, *mpl);

    return node_p(new Literal(mpl, values));
}

// The following could be more cleanly implemented recursively, but would
// limit stack depth.
node_p parse_call(
    const std::string& text,
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
        case '\'':
        case 'n':
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
        {
            if (! current) {
                error(i, "Naked literal");
            }
            current->add_child(parse_literal(text, i));
            assert(text[i] == '\'' || text[i] == 'l' || num_char(text[i]));
            advance(i, length, "Unterminated call");
            break;
        }
        default:
            error(i, string("Unexpected character ") + text[i]);
        }
    }
    if (! top) {
        error(i, "Unterminated call");
    }
    assert(current == top);
    return top;
}

} // Predicate
} // IronBee
