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
 * @brief Predicate --- Standard String Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include "standard_test.hpp"

#include <predicate/reporter.hpp>
#include <predicate/standard_string.hpp>

#include <ironbee/rule_engine.h>
#include <ironbee/string.h>

using namespace IronBee::Predicate;
using namespace std;

class TestStandardString :
    public StandardTest
{
protected:
    void SetUp()
    {
        Standard::load_string(factory());
    }    
};

TEST_F(TestStandardString, stringReplaceRx)
{
    EXPECT_EQ("'hellobarworld'", eval("(stringReplaceRx 'foo' 'bar' 'hellofooworld')"));
    EXPECT_EQ("'b=a&d=c&f=e'", eval("(stringReplaceRx '([a-z]+)=([a-z]+)' '$2=$1' 'a=b&c=d&e=f')"));
    EXPECT_EQ("['fxx' 'bxr']", eval("(stringReplaceRx 'a|o' 'x' ['foo' 'bar'])"));
    EXPECT_EQ("[: 'fxx' : 'bxr' :]", eval("(stringReplaceRx 'a|o' 'x' [1 'foo' 2 'bar' 3])"));
    
    EXPECT_EQ("'hellobarworld'", transform("(stringReplaceRx 'foo' 'bar' 'hellofooworld')"));
    EXPECT_EQ("'b=a&d=c&f=e'", transform("(stringReplaceRx '([a-z]+)=([a-z]+)' '$2=$1' 'a=b&c=d&e=f')"));
    EXPECT_EQ("['fxx' 'bxr']", transform("(stringReplaceRx 'a|o' 'x' ['foo' 'bar'])"));
    EXPECT_EQ("[: 'fxx' : 'bxr' :]", transform("(stringReplaceRx 'a|o' 'x' [1 'foo' 2 'bar' 3])"));

    EXPECT_THROW(eval("(stringReplaceRx)"), IronBee::einval);
    EXPECT_THROW(eval("(stringReplaceRx 'a')"), IronBee::einval);
    EXPECT_THROW(eval("(stringReplaceRx 'a' 'b')"), IronBee::einval);
    EXPECT_THROW(eval("(stringReplaceRx 'a' 'b' 'c' 'd')"), IronBee::einval);
    EXPECT_THROW(eval("(stringReplaceRx 1 'b' 'c')"), IronBee::einval);
    EXPECT_THROW(eval("(stringReplaceRx 'b' 1 'c')"), IronBee::einval);
}

TEST_F(TestStandardString, length)
{
    EXPECT_EQ("7", eval("(length 'abcdefg')"));
    EXPECT_EQ("[2 7]", eval("(length ['ab' 'abcdefg'])"));
    EXPECT_EQ("x:[a:2 b:7]", eval("(length x:[a:'ab' b:'abcdefg'])"));

    EXPECT_EQ("7", transform("(length 'abcdefg')"));
    EXPECT_EQ("[2 7]", transform("(length ['ab' 'abcdefg'])"));
    EXPECT_EQ("x:[a:2 b:7]", transform("(length x:[a:'ab' b:'abcdefg'])"));
    
    EXPECT_THROW(eval("(length)"), IronBee::einval);
    EXPECT_THROW(eval("(length 'a' 'b')"), IronBee::einval);
}
