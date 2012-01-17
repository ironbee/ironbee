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
/// @brief IronBee - Operator Test Functions
///
/// @author Craig Forbes <cforbes@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include <ironbee/operator.h>

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

ib_status_t test_create_fn(void *data) {
    return IB_OK;
}

ib_status_t test_destroy_fn() {
    return IB_OK;
}

ib_status_t test_execute_fn(void *data, ib_field_t *field, ib_num_t *result) {
    *result = 1;
    return IB_OK;
}

TEST(OperatorTests, call_operator) {
    ib_status_t status;
    ib_num_t call_result;
    status = register_operator("test_op",
                               test_create_fn,
                               test_destroy_fn,
                               test_execute_fn);

    ASSERT_EQ(IB_OK, status);

    ib_operator_instance_t op;
    status = create_operator_instance("test_op","data",&op);
    ASSERT_EQ(IB_OK, status);

    ib_field_t field;

    status = call_operator_instance(&op, &field, &call_result);
    ASSERT_EQ(IB_OK, status);
    EXPECT_EQ(1, call_result);
}


