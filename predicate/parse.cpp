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

}

node_p parse_literal(
    const std::string& text,
    size_t&            i
)
{
    size_t length = text.length();
    bool escape = false;
    string value;

    if (text.substr(i, 4) == "null") {
        i += 3;
        return node_p(new Null());
    }
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
    return node_p(new String(value));
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
                // Very importat to keep all our nodes in memory.
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
        {
            if (! current) {
                error(i, "Naked literal");
            }
            current->add_child(parse_literal(text, i));
            assert(text[i] == '\'' || text[i] == 'l');
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
