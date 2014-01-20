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
 * @brief Predicate --- Standard Fixture
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#ifndef __PREDICATE__TESTS__STANDARD_TEST__
#define __PREDICATE__TESTS__STANDARD_TEST__

#include "predicate/dag.hpp"

#include "parse_fixture.hpp"
#include <ironbeepp/test_fixture.hpp>

#include "gtest/gtest.h"

class StandardTest :
    public ::testing::Test,
    public IronBee::TestFixture,
    public ParseFixture
{
protected:
    void SetUp();

    IronBee::Predicate::node_p parse(
        const std::string& text
    ) const;

    IronBee::Predicate::ValueList eval(
        IronBee::Predicate::node_p n
    );

    // The following copy the value out and thus are safe to use text
    // as there is no need to keep the expression tree around.
    bool eval_bool(
        IronBee::Predicate::node_p n
    );

    std::string eval_s(
        IronBee::Predicate::node_p n
    );

    std::string eval_l(
        IronBee::Predicate::node_p n
    );

    int64_t eval_n(
        IronBee::Predicate::node_p n
    );

    IronBee::Predicate::node_p transform(
        IronBee::Predicate::node_p n
    ) const;

    std::string transform(
        const std::string& s
    ) const;
};


#endif /* __PREDICATE__TESTS__STANDARD_TEST__ */
