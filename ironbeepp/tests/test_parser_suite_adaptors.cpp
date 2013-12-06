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
 * @brief IronBee++ Internals --- ParserSuite adaptors Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/parser_suite_adaptors.hpp>

#include <ironbeepp/memory_pool.hpp>

#include "gtest/gtest.h"

using namespace IronBee;
using namespace std;

namespace {

ParserSuite::span_t span(const char* literal)
{
    return ParserSuite::span_t(literal, literal + strlen(literal));
}

};

TEST(TestParserSuiteAdpators, basic)
{
    typedef ParserSuite::parse_headers_result_t::header_t header_t;
    using ParserSuite::span_t;

    ScopedMemoryPool smp;

    ParserSuite::parse_headers_result_t::headers_t headers;
    headers.push_back(header_t(span("key1")));
    headers.back().value.push_back(span("value1"));
    headers.push_back(header_t(span("key2")));
    headers.back().value.push_back(span("valu"));
    headers.back().value.push_back(span("e2"));
    headers.push_back(header_t(span("key3")));
    headers.back().value.push_back(span("value3"));

    psheader_to_parsed_header_const_range_t result =
        psheaders_to_parsed_headers(smp, headers);

    ASSERT_EQ(3L, result.size());
    psheader_to_parsed_header_const_range_t::iterator i = result.begin();
    EXPECT_EQ("key1", i->name().to_s());
    EXPECT_EQ("value1", i->value().to_s());
    ++i;
    EXPECT_EQ("key2", i->name().to_s());
    EXPECT_EQ("value2", i->value().to_s());
    ++i;
    EXPECT_EQ("key3", i->name().to_s());
    EXPECT_EQ("value3", i->value().to_s());
    ++i;
    EXPECT_EQ(result.end(), i);
}
