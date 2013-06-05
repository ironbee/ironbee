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
 * @brief IronBee --- ParserSuite implementation.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "parser_suite.hpp"

#include <boost/fusion/adapted.hpp>
#include <boost/spirit/include/qi.hpp>

using namespace std;

// Tell Fusion about the result structures.
// These must be in the top namespace.

BOOST_FUSION_ADAPT_STRUCT(
    ::IronBee::ParserSuite::parse_request_line_result_t,
    (::IronBee::ParserSuite::span_t, method)
    (::IronBee::ParserSuite::span_t, uri)
    (::IronBee::ParserSuite::span_t, version)
)

BOOST_FUSION_ADAPT_STRUCT(
    ::IronBee::ParserSuite::parse_response_line_result_t,
    (::IronBee::ParserSuite::span_t, version)
    (::IronBee::ParserSuite::span_t, status)
    (::IronBee::ParserSuite::span_t, message)
)

BOOST_FUSION_ADAPT_STRUCT(
    ::IronBee::ParserSuite::parse_uri_result_t,
    (::IronBee::ParserSuite::span_t, scheme)
    (::IronBee::ParserSuite::span_t, authority)
    (::IronBee::ParserSuite::span_t, path)
    (::IronBee::ParserSuite::span_t, query)
    (::IronBee::ParserSuite::span_t, fragment)
)

BOOST_FUSION_ADAPT_STRUCT(
    ::IronBee::ParserSuite::parse_authority_result_t,
    (::IronBee::ParserSuite::span_t, username)
    (::IronBee::ParserSuite::span_t, password)
    (::IronBee::ParserSuite::span_t, host)
    (::IronBee::ParserSuite::span_t, port)
)

namespace IronBee {
namespace ParserSuite {

namespace  {

#ifndef DOXYGEN_SKIP
// Doxygen currently can't handle this.
//! Parser matching horizontal whitespace only.
const static auto sp = boost::spirit::ascii::char_(" \t");
#endif

}

ostream& operator<<(
    ostream&                      o,
    const parse_headers_result_t& R
)
{
    for (const auto& header : R.headers) {
        bool first = true;
        o << header.key << "=";
        for (const auto& v : header.value) {
            if (! first) {
                o << " ";
            }
            first = false;
            o << v;
        }
        o << endl;
    }

    o << "terminated=" << (R.terminated ? "true" : "false") << endl;

    return o;
}

parse_headers_result_t parse_headers(span_t& input)
{
    using namespace boost::spirit::qi;
    using ascii::char_;

    parse_headers_result_t R;

    auto begin = input.begin();
    auto end   = input.end();

    static const auto push_value = [&](const span_t& value)
    {
        R.headers.back().value.push_back(value);
    };

    static const auto new_header = [&](const span_t& key)
    {
        R.headers.push_back(parse_headers_result_t::header_t {key, {}});
    };

    static const auto value = raw[+(char_-char_("\r\n"))];
    static const auto key = raw[*(char_-char_(" :\r\n"))] >> omit[lit(":")];
    static const auto header =
        (
              key                           [new_header]
           >> *sp
           >> value                         [push_value]
           >> (eol|eoi)
        ) |
        (
              +char_(" ")
           >> value                        [push_value]
           >> (eol|eoi)
        )
        ;
    static const auto terminator = *sp >> eol;

    static const auto grammar =
           +header
        >> -terminator [([&]() {R.terminated = true;})]
        ;

    auto success = parse(begin, end, grammar);
    // Note: begin updated.

    if (! success) {
        BOOST_THROW_EXCEPTION(error()
            << errinfo_what("Incomplete headers.")
            << errinfo_location(begin)
        );
    }

    input = span_t(begin, end);
    return move(R);
}

parse_request_line_result_t parse_request_line(span_t& input)
{
    using namespace boost::spirit::qi;
    using ascii::char_;

    parse_request_line_result_t R;

    auto begin = input.begin();
    auto end   = input.end();

    static const auto word = raw[+(char_-char_(" \r\n"))];
    static const auto grammar =
           omit[*sp]
        >> word  // method
        >> omit[*sp]
        >> word  // uri
        >> omit[*sp]
        >> -word // version
        >> omit[eol|eoi]
        ;

    auto success = parse(begin, end, grammar, R);
    // Note: begin updated.

    if (! success) {
        BOOST_THROW_EXCEPTION(error()
            << errinfo_what("Incomplete request line.")
            << errinfo_location(begin)
        );
    }

    input = span_t(begin, end);
    return move(R);
}

ostream& operator<<(
    ostream&                           o,
    const parse_request_line_result_t& R
)
{
    o << "method="  << R.method  << endl
      << "uri="     << R.uri     << endl
      << "version=" << R.version << endl
      ;

    return o;
}

parse_response_line_result_t parse_response_line(span_t& input)
{
    using namespace boost::spirit::qi;
    using ascii::char_;

    parse_response_line_result_t R;

    auto begin = input.begin();
    auto end   = input.end();

    static const auto word = raw[+(char_-char_(" \r\n"))];
    static const auto grammar =
           omit[*sp]
        >> word                        // version
        >> omit[*sp]
        >> word                        // status
        >> omit[*sp]
        >> raw[*(char_-char_("\r\n"))] // message
        >> omit[eol|eoi]
        ;

    auto success = parse(begin, end, grammar, R);
    // Note: begin updated.

    if (! success) {
        BOOST_THROW_EXCEPTION(error()
            << errinfo_what("Incomplete response line.")
            << errinfo_location(begin)
        );
    }

    input = span_t(begin, end);
    return move(R);
}

ostream& operator<<(
    ostream&                            o,
    const parse_response_line_result_t& R
)
{
    o << "version=" << R.version << endl
      << "status="  << R.status  << endl
      << "message=" << R.message << endl
      ;

    return o;
}

parse_uri_result_t parse_uri(span_t& input)
{
    using namespace boost::spirit::qi;
    using ascii::char_;

    parse_uri_result_t R;

    auto begin = input.begin();
    auto end   = input.end();

    static const auto grammar =
           -(raw[+char_("-A-Za-z0-9+.")] >> omit[char_(":")])     // scheme
        >> -(omit[lit("//")] >> (raw[*(char_-char_("/?#\r\n"))])) // authority
        >> -(raw[*(char_-char_("?#\r\n"))])                       // path
        >> -(omit[char_("?")] >> raw[*(char_-char_("#\r\n"))])    // query
        >> -(omit[char_("#")] >> raw[*(char_-char_("\r\n"))])     // fragment
        >> omit[eol|eoi]
        ;

    auto success = parse(begin, end, grammar, R);
    // Note: begin updated.

    if (! success) {
        BOOST_THROW_EXCEPTION(error()
            << errinfo_what("Incomplete URI.")
            << errinfo_location(begin)
        );
    }

    input = span_t(begin, end);
    return move(R);
}

ostream& operator<<(
    ostream&                  o,
    const parse_uri_result_t& R
)
{
    o << "scheme="    << R.scheme    << endl
      << "authority=" << R.authority << endl
      << "path="      << R.path      << endl
      << "query="     << R.query     << endl
      << "fragment="  << R.fragment  << endl
      ;

    return o;
}

parse_request_result_t parse_request(span_t& input)
{
    parse_request_result_t R;

    const auto begin = input.begin();
    R.request_line = parse_request_line(input);
    R.raw_request_line = span_t(begin, input.begin() - 1);
    span_t uri = R.request_line.uri;
    R.uri = parse_uri(uri);
    if (! uri.empty()) {
        BOOST_THROW_EXCEPTION(error()
            << errinfo_what("URI not fully parsed.")
        );
    }
    R.headers = parse_headers(input);

    return move(R);
}

ostream& operator<<(
    ostream& o,
const parse_request_result_t& R
)
{
    o << "raw_request_line=" << R.raw_request_line << endl
      << R.request_line << endl
      << R.uri          << endl
      << R.headers      << endl
      ;

    return o;
}


parse_response_result_t parse_response(span_t& input)
{
    parse_response_result_t R;

    const auto begin = input.begin();
    R.response_line = parse_response_line(input);
    R.raw_response_line = span_t(begin, input.begin() - 1);
    R.headers = parse_headers(input);

    return move(R);
}

ostream& operator<<(
    ostream& o,
const parse_response_result_t& R
)
{
    o << "raw_response_line=" << R.raw_response_line << endl
      << R.response_line << endl
      << R.headers      << endl
      ;

    return o;
}

parse_authority_result_t parse_authority(span_t& input)
{
    using namespace boost::spirit::qi;
    using ascii::char_;

    parse_authority_result_t R;

    auto begin = input.begin();
    auto end   = input.end();

    static const auto word = raw[*(char_-char_("@: \t\r\n"))];
    auto set_username = [&](span_t s) {R.username = s;};
    auto set_password = [&](span_t s) {R.password = s;};
    auto set_host     = [&](span_t s) {R.host     = s;};
    auto set_port     = [&](span_t s) {R.port     = s;};
    auto only_host = [&](span_t s)
    {
        R = parse_authority_result_t();
        set_host(s);
    };

    static const auto grammar =
        (
            (
                   -word [set_username]
                >> -(omit[lit(":")] >> word [set_password])
                >> omit[lit("@")] >> word [set_host]
            ) |
            word [only_host]
        )
        >> -(omit[lit(":")] >> word [set_port])
        ;

    auto success = parse(begin, end, grammar);
    // Note: begin updated.

    if (! success) {
        BOOST_THROW_EXCEPTION(error()
            << errinfo_what("Incomplete authority.")
            << errinfo_location(begin)
        );
    }

    input = span_t(begin, end);
    return move(R);
}

std::ostream& operator<<(
    std::ostream&                   o,
    const parse_authority_result_t& R
)
{
    o  << "username=" << R.username << endl
       << "password=" << R.password << endl
       << "host="     << R.host     << endl
       << "port="     << R.port     << endl
       ;

    return o;
}

} // ParserSuite
} // IronBee
