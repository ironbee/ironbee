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
 * @brief IronAutomata --- Buffer test.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironautomata/buffer.hpp>

#include "gtest/gtest.h"

using namespace std;
using namespace IronAutomata;

TEST(TestBuffer, Trivial)
{
    buffer_t buffer;
    BufferAssembler a(buffer);

    EXPECT_EQ(&buffer, &a.buffer());
    EXPECT_EQ(0UL, a.size());
}

TEST(TestBuffer, Extend)
{
    buffer_t buffer;
    BufferAssembler a(buffer);
    size_t result = a.extend(5);

    EXPECT_EQ(0UL, result);
    EXPECT_LE(5UL, buffer.capacity());
}

TEST(TestBuffer, IndexAndPtr)
{
    buffer_t buffer;
    BufferAssembler a(buffer);

    EXPECT_EQ(3UL, a.index(buffer.data() + 3));
    EXPECT_EQ(buffer.data() + 3, a.ptr<uint8_t>(3));
    EXPECT_EQ(3UL, a.index(a.ptr<char>(3)));
}

namespace {

struct foo_t
{
    int a;
    int b;
};

}

TEST(TestBuffer, AppendObject)
{
    foo_t f = {1, 2};
    foo_t g = {3, 4};
    buffer_t buffer;
    BufferAssembler a(buffer);

    foo_t* f_p = a.append_object(f);
    size_t f_index = a.index(f_p);
    EXPECT_EQ(sizeof(foo_t), a.size());
    EXPECT_EQ(buffer.data(), reinterpret_cast<uint8_t*>(f_p));
    foo_t* g_p = a.append_object(g);
    EXPECT_EQ(2 * sizeof(foo_t), a.size());
    EXPECT_EQ(buffer.data() + sizeof(foo_t), reinterpret_cast<uint8_t*>(g_p));

    // f_p may have been invalidated.
    f_p = a.ptr<foo_t>(f_index);
    EXPECT_EQ(f.a, f_p->a);
    EXPECT_EQ(f.b, f_p->b);
    EXPECT_EQ(g.a, g_p->a);
    EXPECT_EQ(g.b, g_p->b);
}

TEST(TestBuffer, AppendArray)
{
    buffer_t buffer;
    BufferAssembler a(buffer);

    int* p = a.append_array<int>(5);
    EXPECT_EQ(5 * sizeof(int), a.size());
    EXPECT_EQ(buffer.data(), reinterpret_cast<uint8_t*>(p));

    for (size_t i = 0; i < buffer.size(); ++i) {
        EXPECT_EQ(0, buffer[i]);
    }
}

TEST(TestBuffer, AppendString)
{
    buffer_t buffer;
    BufferAssembler a(buffer);

    static const char c_content[] = "Hello World";
    char* p = a.append_string(c_content);
    EXPECT_EQ(string(c_content), string(p, sizeof(c_content) - 1));
    EXPECT_EQ(string(c_content), string(reinterpret_cast<const char*>(buffer.data()), sizeof(c_content) - 1));
    EXPECT_EQ(reinterpret_cast<const char*>(buffer.data()), p);
    // -1 because no NUL.
    EXPECT_EQ(sizeof(c_content) - 1, a.size());
}

TEST(TestBuffer, AppendBytes)
{
    buffer_t buffer;
    BufferAssembler a(buffer);

    vector<uint8_t> content;
    content.push_back(13);
    content.push_back(14);
    content.push_back(15);

    uint8_t* p = a.append_bytes(content.data(), content.size());
    EXPECT_EQ(buffer.data(), reinterpret_cast<uint8_t*>(p));
    EXPECT_EQ(3UL, a.size());
    EXPECT_TRUE(equal(buffer.begin(), buffer.end(), content.begin()));
}
