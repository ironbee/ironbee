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
 * @brief IronBee++ Internals &mdash; Transaction Data Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/transaction_data.hpp>
#include <ironbeepp/transaction.hpp>
#include <ironbeepp/engine.hpp>
#include <ironbeepp/memory_pool.hpp>

#include "gtest/gtest.h"

using namespace IronBee;

TEST(TestTransactionData, basic)
{
    ib_txdata_t ib_txdata;

    TransactionData txdata(&ib_txdata);

    ASSERT_TRUE(txdata);

    ib_txdata.dlen = 14;
    EXPECT_EQ(ib_txdata.dlen, txdata.length());
    ib_txdata.data = (uint8_t*)15;
    EXPECT_EQ((char*)ib_txdata.data, txdata.data());
}

TEST(TestTransactionData, create)
{
    char* data = strdup("foobar");

    ScopedMemoryPool smp;
    MemoryPool mp = smp;

    TransactionData td = TransactionData::create_alias(
        mp,
        data,
        6
    );

    ASSERT_TRUE(td);
    EXPECT_EQ(data, td.data());
    EXPECT_EQ(6UL, td.length());

    free(data);
}
