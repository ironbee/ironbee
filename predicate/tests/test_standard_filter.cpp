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
 * @brief Predicate --- Standard Predicate Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <predicate/standard_filter.hpp>

#include "standard_test.hpp"

using namespace IronBee::Predicate;
using namespace std;

class TestStandardFilter :
    public StandardTest
{
protected:
    void SetUp()
    {   
        Standard::load_filter(factory());
    }
};

TEST_F(TestStandardFilter, eq)
{
    EXPECT_EQ("['b' 'b']", eval("(eq 'b' ['a' 'b' 'b'])"));
    EXPECT_EQ("'b'", eval("(eq 'b' 'b')"));
    EXPECT_EQ("[]", eval("(eq 'x' ['a' 'b' 'b'])"));
    EXPECT_EQ(":", eval("(eq 'x' 'b')"));

    EXPECT_EQ("['b' 'b']", transform("(eq 'b' ['a' 'b' 'b'])"));
    EXPECT_EQ("'b'", transform("(eq 'b' 'b')"));
    EXPECT_EQ("[]", transform("(eq 'x' ['a' 'b' 'b'])"));
    EXPECT_EQ(":", transform("(eq 'x' 'b')"));
    
    EXPECT_THROW(eval("(eq)"), IronBee::einval);
    EXPECT_THROW(eval("(eq 1)"), IronBee::einval);
    EXPECT_THROW(eval("(eq 1 2 3)"), IronBee::einval);
}

TEST_F(TestStandardFilter, ne)
{
    EXPECT_EQ("['b' 'b']", eval("(ne 'a' ['a' 'b' 'b'])"));
    EXPECT_EQ("'b'", eval("(ne 'a' 'b')"));
    EXPECT_EQ("['a' 'b' 'b']", eval("(ne 'x' ['a' 'b' 'b'])"));
    EXPECT_EQ(":", eval("(ne 'b' 'b')"));
    EXPECT_EQ("'a'", eval("(ne 5 'a')"));

    EXPECT_EQ("['b' 'b']", transform("(ne 'a' ['a' 'b' 'b'])"));
    EXPECT_EQ("'b'", transform("(ne 'a' 'b')"));
    EXPECT_EQ("['a' 'b' 'b']", transform("(ne 'x' ['a' 'b' 'b'])"));
    EXPECT_EQ(":", transform("(ne 'b' 'b')"));
    EXPECT_EQ("'a'", transform("(ne 5 'a')"));

    EXPECT_THROW(eval("(ne)"), IronBee::einval);
    EXPECT_THROW(eval("(ne 1)"), IronBee::einval);
    EXPECT_THROW(eval("(ne 1 2 3)"), IronBee::einval);
}

TEST_F(TestStandardFilter, Numeric)
{
    EXPECT_EQ("[1 2 3]", eval("(lt 4 [1 2 3 4 5 6 7])"));
    EXPECT_EQ("[1 2 3 4]", eval("(le 4 [1 2 3 4 5 6 7])"));
    EXPECT_EQ("[5 6 7]", eval("(gt 4 [1 2 3 4 5 6 7])"));
    EXPECT_EQ("[4 5 6 7]", eval("(ge 4 [1 2 3 4 5 6 7])"));

    EXPECT_EQ("[1 2 3]", transform("(lt 4 [1 2 3 4 5 6 7])"));
    EXPECT_EQ("[1 2 3 4]", transform("(le 4 [1 2 3 4 5 6 7])"));
    EXPECT_EQ("[5 6 7]", transform("(gt 4 [1 2 3 4 5 6 7])"));
    EXPECT_EQ("[4 5 6 7]", transform("(ge 4 [1 2 3 4 5 6 7])"));
    
    EXPECT_EQ("4", eval("(lt 5 4)"));
    EXPECT_EQ("4", eval("(le 4 4)"));
    EXPECT_EQ("4", eval("(gt 3 4)"));
    EXPECT_EQ("4", eval("(ge 4 4)"));
    EXPECT_EQ("4", transform("(ge 4 4)"));
    
    EXPECT_EQ(":", eval("(lt 5 6)"));
    EXPECT_EQ(":", eval("(le 5 6)"));
    EXPECT_EQ(":", eval("(gt 5 3)"));
    EXPECT_EQ(":", eval("(ge 5 3)"));
    EXPECT_EQ(":", transform("(ge 5 3)"));

    EXPECT_EQ(":", eval("(lt 5 :)"));
    EXPECT_EQ(":", eval("(le 5 :)"));
    EXPECT_EQ(":", eval("(gt 5 :)"));
    EXPECT_EQ(":", eval("(ge 5 :)"));
    EXPECT_EQ(":", transform("(ge 5 :)"));
    
    EXPECT_EQ("[]", eval("(lt 1 [])"));
    EXPECT_EQ("[]", eval("(le 1 [])"));
    EXPECT_EQ("[]", eval("(gt 1 [])"));
    EXPECT_EQ("[]", eval("(ge 1 [])"));
    EXPECT_EQ("[]", transform("(ge 1 [])"));
    
    EXPECT_THROW(eval("(lt 'a' 1)"), IronBee::einval);
    EXPECT_THROW(eval("(lt [5] 1)"), IronBee::einval);
    EXPECT_THROW(eval("(lt 5 'a')"), IronBee::einval);
    EXPECT_THROW(eval("(lt)"), IronBee::einval);
    EXPECT_THROW(eval("(lt 1)"), IronBee::einval);
    EXPECT_THROW(eval("(lt 1 2 3)"), IronBee::einval);
}

TEST_F(TestStandardFilter, Typed)
{
    EXPECT_EQ("[5]", eval("(typed 'number' ['a' 5 [6] 2.0])"));
    EXPECT_EQ("['a']", eval("(typed 'string' ['a' 5 [6] 2.0])"));
    EXPECT_EQ("[[6]]", eval("(typed 'list' ['a' 5 [6] 2.0])"));
    EXPECT_EQ("[2.000000]", eval("(typed 'float' ['a' 5 [6] 2.0])"));    

    EXPECT_EQ("[5]", transform("(typed 'number' ['a' 5 [6] 2.0])"));
    EXPECT_EQ("['a']", transform("(typed 'string' ['a' 5 [6] 2.0])"));
    EXPECT_EQ("[[6]]", transform("(typed 'list' ['a' 5 [6] 2.0])"));
    EXPECT_EQ("[2.000000]", transform("(typed 'float' ['a' 5 [6] 2.0])"));

    EXPECT_EQ("5", eval("(typed 'number' 5)"));
    EXPECT_EQ("'a'", eval("(typed 'string' 'a')"));
    EXPECT_EQ("2.000000", eval("(typed 'float' 2.0)"));
    
    EXPECT_EQ("5", transform("(typed 'number' 5)"));
    EXPECT_EQ("'a'", transform("(typed 'string' 'a')"));
    EXPECT_EQ("2.000000", transform("(typed 'float' 2.0)"));

    EXPECT_THROW(eval("(typed 1 2)"), IronBee::einval);
    EXPECT_THROW(eval("(typed 'foobar' 2)"), IronBee::einval);
    EXPECT_THROW(eval("(typed)"), IronBee::einval);
    EXPECT_THROW(eval("(typed 'string')"), IronBee::einval);
    EXPECT_THROW(eval("(typed 'string' 1 2)"), IronBee::einval);
}

TEST_F(TestStandardFilter, Named)
{
    EXPECT_EQ("[a:1]", eval("(named 'a' [a:1 b:2])"));
    EXPECT_EQ("[]", eval("(named 'x' [a:1 b:2])"));
    EXPECT_EQ("a:1", eval("(named 'a' a:1)"));
    EXPECT_EQ(":", eval("(named 'A' a:1)"));
    EXPECT_EQ("[]", eval("(named 'A' [a:1])"));

    EXPECT_EQ("[a:1]", transform("(named 'a' [a:1 b:2])"));
    EXPECT_EQ("[]", transform("(named 'x' [a:1 b:2])"));
    EXPECT_EQ("a:1", transform("(named 'a' a:1)"));
    EXPECT_EQ(":", transform("(named 'A' a:1)"));
    EXPECT_EQ("[]", transform("(named 'A' [a:1])"));
    
    EXPECT_THROW(eval("(named)"), IronBee::einval);
    EXPECT_THROW(eval("(named 'a')"), IronBee::einval);
    EXPECT_THROW(eval("(named 1 a:1)"), IronBee::einval);
    EXPECT_THROW(eval("(named 'a' 1 2)"), IronBee::einval);
}

TEST_F(TestStandardFilter, NamedI)
{
    EXPECT_EQ("[a:1]", eval("(namedi 'A' [a:1 b:2])"));
    EXPECT_EQ("[]", eval("(namedi 'x' [a:1 b:2])"));
    EXPECT_EQ("a:1", eval("(namedi 'A' a:1)"));

    EXPECT_EQ("[a:1]", transform("(namedi 'A' [a:1 b:2])"));
    EXPECT_EQ("[]", transform("(namedi 'x' [a:1 b:2])"));
    EXPECT_EQ("a:1", transform("(namedi 'A' a:1)"));
    
    EXPECT_THROW(eval("(namedi)"), IronBee::einval);
    EXPECT_THROW(eval("(namedi 'a')"), IronBee::einval);
    EXPECT_THROW(eval("(namedi 1 a:1)"), IronBee::einval);
    EXPECT_THROW(eval("(namedi 'a' 1 2)"), IronBee::einval);
}

TEST_F(TestStandardFilter, NamedRx)
{
    EXPECT_EQ("[foo:1]", eval("(namedRx 'f.o' [foo:1 bar:2])"));
    EXPECT_EQ("[]", eval("(namedRx 'x' [foo:1 bar:2])"));
    EXPECT_EQ("foo:1", eval("(namedRx 'f.o' foo:1)"));

    EXPECT_EQ("[foo:1]", transform("(namedRx 'f.o' [foo:1 bar:2])"));
    EXPECT_EQ("[]", transform("(namedRx 'x' [foo:1 bar:2])"));
    EXPECT_EQ("foo:1", transform("(namedRx 'f.o' foo:1)"));
    
    EXPECT_THROW(eval("(namedRx)"), IronBee::einval);
    EXPECT_THROW(eval("(namedRx 'a')"), IronBee::einval);
    EXPECT_THROW(eval("(namedRx 1 foo:1)"), IronBee::einval);
    EXPECT_THROW(eval("(namedRx 'f[' foo:1)"), IronBee::einval);
    EXPECT_THROW(eval("(namedRx 'a' 1 2)"), IronBee::einval);
}

TEST_F(TestStandardFilter, Longer)
{
    EXPECT_EQ("[[1 2 3]]", eval("(longer 2 [[1] [1 2] [1 2 3]])"));
    EXPECT_EQ("[[1 2 3]]", eval("(longer 2 [: [1] 'a' [1 2] 7 [1 2 3]])"));
    EXPECT_EQ("[]", eval("(longer 5 [[1] [1 2] [1 2 3]])"));

    EXPECT_EQ("[[1 2 3]]", transform("(longer 2 [[1] [1 2] [1 2 3]])"));
    EXPECT_EQ("[[1 2 3]]", transform("(longer 2 [: [1] 'a' [1 2] 7 [1 2 3]])"));
    EXPECT_EQ("[]", transform("(longer 5 [[1] [1 2] [1 2 3]])"));
    
    EXPECT_THROW(eval("(longer)"), IronBee::einval);
    EXPECT_THROW(eval("(longer 1)"), IronBee::einval);
    EXPECT_THROW(eval("(longer 'a' [1 2 3])"), IronBee::einval);
    EXPECT_THROW(eval("(longer 1 [1 2 3] 4)"), IronBee::einval);
}
