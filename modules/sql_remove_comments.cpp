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
 * @brief IronBee --- SQL Remove Comments
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 *
 * Provide transformation logic to remove comments from various
 * database types.
 */

#include "sql_remove_comments.hpp"

#include <ironbeepp/memory_manager.hpp>
#include <ironbeepp/module.hpp>
#include <ironbeepp/transformation.hpp>

#ifdef __clang__
#pragma clang diagnostic push
#if __has_warning("-Wc++11-extensions")
#pragma clang diagnostic ignored "-Wc++11-extensions"
#endif
#if __has_warning("-Wunused-local-typedef")
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
#endif
#include <boost/bind.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/lex_lexertl.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_statement.hpp>
#include <boost/spirit/include/phoenix_container.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <string>

namespace remove_comments {

using namespace IronBee;

// Remove PG Comments.
namespace {

//! Token IDs.
enum {
    PG_ID_START_COMMENT,
    PG_ID_STOP_COMMENT,
    PG_ID_EOL,
    PG_ID_CHAR
};

template <typename Lexer>
struct pg_comment_tokens : boost::spirit::lex::lexer<Lexer> {

    //! Constructor
    pg_comment_tokens() {
        this->self.add
            ("/\\*",    PG_ID_START_COMMENT)
            ("\\*/",    PG_ID_STOP_COMMENT)
            ("\\r?\\n", PG_ID_EOL)
            (".",       PG_ID_CHAR)
        ;
    }
};

struct pg_token_handler {
    typedef bool result_type;
    template<typename Token>
    bool operator()(Token const& t)
    {
        switch(t.id()) {
        case PG_ID_START_COMMENT:
            break;
        case PG_ID_STOP_COMMENT:
            break;
        case PG_ID_EOL:
            break;
        case PG_ID_CHAR:
            break;
        }

        // Always continue to tokenize.
        return true;
    }
};

ConstField sql_remove_pg_comments(
    MemoryManager mm,
    ConstField    field_in
)
{
    std::string s = field_in.to_s();

    char const* first = s.c_str();
    char const* last = &first[s.size()];

    // Create a lexer with a lexer implementation.
    pg_comment_tokens<boost::spirit::lex::lexertl::lexer<> > pg_c_t;

    // Create a handler.
    pg_token_handler token_handler;

    // Tokenize!
    boost::spirit::lex::tokenize(
        first,
        last,
        pg_c_t,
        boost::bind(token_handler, _1));

    return field_in;
}

} // Anonymous Namespace

void register_trasformations(Module module)
{
    Transformation::create<void>(
        module.engine().main_memory_mm(),
        "remove_pg_comments",
        false,
        NULL,
        NULL,
        boost::bind(sql_remove_pg_comments, _1, _2)
    ).register_with(module.engine());
}


};

