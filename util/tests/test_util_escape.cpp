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
 * @brief Predicate --- String Trim Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 * @author Nick LeRoy <nleroy@qualys.com>
 **/

#include "ironbee_config_auto.h"

#include <ironbee/escape.h>

#include <ironbeepp/all.hpp>

#include "gtest/gtest.h"

using namespace std;
using namespace IronBee;

namespace {

string escape_json(const string& s)
{
    vector<char> result(s.length() * 2 + 20);

    size_t result_size;
    throw_if_error(
        ib_string_escape_json_buf(
            reinterpret_cast<const uint8_t*>(s.data()), s.length(),
            &result.front(), result.size(),
            &result_size
        )
    );
    return string(&result.front(), result_size);
}

}

TEST(TestUtilEscapeJSON, easy)
{
    EXPECT_EQ("\"\"", escape_json(""));
    EXPECT_EQ("\"FooBar\"", escape_json("FooBar"));
    EXPECT_EQ("\"Foo+Bar\"", escape_json("Foo+Bar"));
}

TEST(TestUtilEscapeJSON, escapes)
{
    EXPECT_EQ("\"\\/\"", escape_json("/"));
    EXPECT_EQ("\"\\\"\"", escape_json("\""));
    EXPECT_EQ("\"'\"", escape_json("'"));
    EXPECT_EQ("\"\\\"\"", escape_json("\""));
    EXPECT_EQ("\"\\\\\"", escape_json("\\"));
    EXPECT_EQ("\"\\b\"", escape_json("\b"));
    EXPECT_EQ("\"\\f\"", escape_json("\f"));
    EXPECT_EQ("\"\\n\"", escape_json("\n"));
    EXPECT_EQ("\"\\r\"", escape_json("\r"));
    EXPECT_EQ("\"\\t\"", escape_json("\t"));

    EXPECT_EQ("\"x\\ty\"", escape_json("x\ty"));
    EXPECT_EQ("\"x\\t\\ty\"", escape_json("x\t\ty"));
    EXPECT_EQ("\"x\\n\\ry\"", escape_json("x\n\ry"));

    EXPECT_EQ(
        "\"x\\t\\tfoo\\u0000y\"",
        escape_json(string("x\t\tfoo\0y", 8))
    );
}

TEST(TestEscapeJSON, null)
{
    EXPECT_EQ("\"\\u0000\"", escape_json(string("\0", 1)));
    EXPECT_EQ("\"Test\\u0000Case\"", escape_json(string("Test\0Case", 9)));
}

TEST(TestEscapeJSON, nonprintable)
{
    EXPECT_EQ("\"\\u007f\"", escape_json("\x07f"));
    EXPECT_EQ("\"\\u0080\"", escape_json("\x080"));
    EXPECT_EQ("\"\\u00ff\"", escape_json("\x0ff"));
    EXPECT_EQ("\"Test\\u0001Case\"", escape_json("Test""\x001""Case"));
}

namespace {

string escape_json_list(size_t num, ...)
{
    ScopedMemoryPoolLite mpl;
    List<const char*> inputs = List<const char*>::create(mpl);
    va_list va;
    size_t total_length = 0;

    va_start(va, num);
    for (size_t n = 0; n < num; ++n) {
        const char* s = va_arg(va, const char*);
        total_length += strlen(s);
        inputs.push_back(s);
    }
    va_end(va);

    vector<char> result(total_length * 2 + 3 * num + 20);
    size_t result_size;
    throw_if_error(
        ib_strlist_escape_json_buf(
            inputs.ib(),
            &result.front(), result.size(),
            &result_size
        )
    );

    return string(&result.front(), result_size);
}

}

TEST(TestUtilEscapeJSONList, null)
{
    vector<char> result(20);
    size_t result_out;

    ib_status_t rc =
        ib_strlist_escape_json_buf(NULL, &result.front(), 20, &result_out);
    ASSERT_EQ(IB_OK, rc);
    EXPECT_EQ(0UL, result_out);
}

TEST(TestUtilEscapeJSONList, simple)
{
    EXPECT_EQ("", escape_json_list(0));
    EXPECT_EQ("\"x\"", escape_json_list(1, "x"));
    EXPECT_EQ("\"x\", \"y\"", escape_json_list(2, "x", "y"));
}

namespace {

string escape_hex(const string& s)
{
    ScopedMemoryPoolLite mpl;
    const char *result;
    result = ib_util_hex_escape(
        MemoryManager(mpl).ib(),
        reinterpret_cast<const uint8_t*>(s.data()), s.length()
    );

    return string(result);
}

}

TEST(TestUtilHexEscape, basic)
{
    EXPECT_EQ("escape me: 0x10x2", escape_hex("escape me: \01\02"));
}

TEST(TestUtilHexEscape, corners)
{
    EXPECT_EQ("0x0", escape_hex(string("\0", 1)));
    EXPECT_EQ("0x100x110x800xff", escape_hex("\x10\x11\x80\xff"));
}

namespace {

string unescape(const string& s)
{
    vector<char> result(s.length() + 1);

    size_t result_size;
    throw_if_error(
        ib_util_unescape_string(
            &result.front(), &result_size,
            s.data(), s.length()
        )
    );
    return string(&result.front(), result_size);
}

}

TEST(TestUtilUnescapeString, simple)
{
    EXPECT_EQ("\r\n\t", unescape("\\r\\n\\t"));
    EXPECT_EQ("\x01\x02", unescape("\\x01\\x02"));
    EXPECT_EQ(string("\0\x1\x43\x21", 4), unescape("\\u0001\\u4321"));
    EXPECT_EQ("Hello World", unescape("Hello World"));
}

TEST(TestIBUtilUnescapeString, einval)
{
    EXPECT_THROW(unescape("\\x01\\x0"), einval);
    EXPECT_THROW(unescape("\\x0\\x00"), einval);
    EXPECT_THROW(unescape("\\u001\\u4321"), einval);
    EXPECT_THROW(unescape("\\u0001\\u431"), einval);
}

TEST(TestIBUtilUnescapeString, removeQuotes)
{
    EXPECT_EQ("\"hi\'", unescape("\\\"hi\\\'"));
}
