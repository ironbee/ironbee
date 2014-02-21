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
 * @brief IronBee --- CLIPP Configuration Parser Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "configuration_parser.hpp"

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/fusion/adapted.hpp>

#include <fstream>

using namespace std;


BOOST_FUSION_ADAPT_STRUCT(
    IronBee::CLIPP::ConfigurationParser::component_t,
    (string, name)
    (string, arg)
)

BOOST_FUSION_ADAPT_STRUCT(
    IronBee::CLIPP::ConfigurationParser::chain_t,
    (IronBee::CLIPP::ConfigurationParser::component_t,     base)
    (IronBee::CLIPP::ConfigurationParser::component_vec_t, modifiers)
)

namespace IronBee {
namespace CLIPP {
namespace ConfigurationParser {

namespace {

chain_vec_t parse(const std::string& input)
{
    using namespace boost::spirit;

    using ascii::char_;
    using ascii::space;
    using qi::lit;
    using qi::_1;
    using qi::_2;
    using qi::_val;
    using qi::lexeme;
    using qi::omit;
    using boost::phoenix::push_back;

    typedef string::const_iterator iterator;
    typedef qi::rule<iterator, char()>            char_rule;
    typedef qi::rule<iterator, string()>          string_rule;
    typedef qi::rule<iterator, component_t()>     component_rule;
    typedef qi::rule<iterator, chain_t()>         chain_rule;
    typedef qi::rule<iterator, component_vec_t()> component_vec_rule;
    typedef qi::rule<iterator, chain_vec_t()>     chains_rule;

    char_rule   escaped = lit('\\') >> char_;
    string_rule quoted_cfg_string =
        lit('"') >> +(escaped | (char_ - '"')) >> '"';
    string_rule unquoted_cfg_string =
        +(char_ - '@' - space - '"');

    string_rule cfg_string;

    cfg_string = lexeme[
        (unquoted_cfg_string >> -cfg_string) |
        (quoted_cfg_string   >> -cfg_string)
    ];

    component_rule component =
        +(char_ - ':' - space) >> -(':' >> -cfg_string);
    component_rule modifier  = lit('@') >> component;
    component_rule base      = component;

    component_vec_rule modifiers =
        *(omit[*space] >> modifier >> omit[*space]);
    chain_rule chain = base >> omit[*space] >> modifiers;

    chains_rule chains = *(omit[*space] >> chain >> omit[*space]);

    chain_vec_t result;
    iterator begin = input.begin();
    iterator end  = input.end();

    bool success = qi::parse(
        begin, end,
        chains,
        result
    );

    if (! success) {
        size_t pos = begin - input.begin();
        throw runtime_error(
            "Parsing failed, next text = " + input.substr(pos, 100)
        );
    }
    if (begin != end) {
        size_t pos = begin - input.begin();
        throw runtime_error(
            "Parsing did not consume all input.  next text = " +
            input.substr(pos, 100)
        );
    }

    return result;
}

} // Anonymous

chain_vec_t parse_string(const std::string& input)
{
    return parse(input);
}

chain_vec_t parse_file(const std::string& path)
{
    ifstream in(path.c_str());
    if (! in) {
        throw runtime_error("Could not open " + path + " for reading.");
    }
    string input;
    string line;
    while (in) {
        getline(in, line);
        size_t pos = line.find_first_not_of(' ');
        if (pos != string::npos && line[pos] != '#') {
            input += line;
            input += " ";
        }
    }

    return parse_string(input);
}

} // ConfigurationParser
} // CLIPP
} // IronBee
