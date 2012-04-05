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
 * @internal
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/transaction_data.hpp>
#include <ironbeepp/transaction.hpp>
#include <ironbeepp/engine.hpp>
#include <ironbeepp/memory_pool.hpp>

#include "gtest/gtest.h"

TEST(TestTransactionData, basic)
{
    ib_txdata_t ib_txdata;

    IronBee::TransactionData txdata(&ib_txdata);

    ASSERT_TRUE(txdata);

    ib_txdata.ib = (ib_engine_t*)1234;
    EXPECT_EQ(ib_txdata.ib, txdata.engine().ib());

    ib_txdata.mp = (ib_mpool_t*)1235;
    EXPECT_EQ(ib_txdata.mp, txdata.memory_pool().ib());

    ib_txdata.tx = (ib_tx_t*)1236;
    EXPECT_EQ(ib_txdata.tx, txdata.transaction().ib());

    ib_txdata.dtype = IB_DTYPE_HTTP_BODY;
    EXPECT_EQ(IronBee::TransactionData::HTTP_BODY, txdata.type());

    ib_txdata.dalloc = 13;
    EXPECT_EQ(ib_txdata.dalloc, txdata.allocated());
    ib_txdata.dlen = 14;
    EXPECT_EQ(ib_txdata.dlen, txdata.length());
    ib_txdata.data = (uint8_t*)15;
    EXPECT_EQ((char*)ib_txdata.data, txdata.data());
}
