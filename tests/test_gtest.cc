//////////////////////////////////////////////////////////////////////////////
// Licensed to Qualys, Inc. (QUALYS) under one or more
// contributor license agreements.  See the NOTICE file distributed with
// this work for additional information regarding copyright ownership.
// QUALYS licenses this file to You under the Apache License, Version 2.0
// (the "License"); you may not use this file except in compliance with
// the License.  You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
/// @file
/// @brief IronBee --- Test Framework Test Functions
///
/// @author Brian Rectanus <brectanus@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

#include <ironbee/util.h>

// Allow testing a test-failure
static void TestFailure()
{
    EXPECT_EQ(5, 2 + 2) << "This should fail";
}

/// @test Basic tests to make sure the framework is working.
TEST(Test, TestFrameworkWorking)
{
    EXPECT_EQ(2, 1 + 1) << "Basic addition failed!";
    EXPECT_NONFATAL_FAILURE(
        TestFailure(),
        "This should fail"
    );
    EXPECT_STRCASEEQ("foo", "FOO") << "\"foo\" != \"FOO\"";
}
