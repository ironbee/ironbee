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

#include <ironbee/rule_engine.h>
#include <ironbee/string.h>

using namespace IronBee::Predicate;
using namespace std;

class TestStandardString :
    public StandardTest
{
};

TEST_F(TestStandardString, stringReplaceRx)
{
    EXPECT_EQ("hellobarworld", eval_s(parse("(stringReplaceRx 'foo' 'bar' 'hellofooworld')")));
    EXPECT_EQ("b=a&d=c&f=e", eval_s(parse("(stringReplaceRx '([a-z]+)=([a-z]+)' '$2=$1' 'a=b&c=d&e=f')")));
    
    EXPECT_THROW(eval_s(parse("(stringReplaceRx)")), IronBee::einval);
    EXPECT_THROW(eval_s(parse("(stringReplaceRx 'a')")), IronBee::einval);
    EXPECT_THROW(eval_s(parse("(stringReplaceRx 'a' 'b')")), IronBee::einval);
    EXPECT_THROW(eval_s(parse("(stringReplaceRx 'a' 'b' 'c' 'd')")), IronBee::einval);
    EXPECT_THROW(eval_s(parse("(stringReplaceRx 1 'b' 'c')")), IronBee::einval);
    EXPECT_THROW(eval_s(parse("(stringReplaceRx 'b' 1 'c')")), IronBee::einval);
}
