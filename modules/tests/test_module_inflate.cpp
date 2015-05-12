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
/// @brief IronBee --- Eudoxus operator module tests
///
/// @author Craig Forbes <cforbes@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include <zlib.h>
#ifdef __clang__
#pragma clang diagnostic push
#if __has_warning("-Wunused-local-typedef")
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
#endif
#include <boost/foreach.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <ironbee/stream_pump.h>
#include <ironbee/stream_processor.h>
#include <ironbee/mm_mpool.h>
#include <ironbee/string.h>

#include <ironbeepp/list.hpp>
#include <ironbeepp/memory_pool_lite.hpp>
#include <ironbeepp/memory_manager.hpp>
#include <ironbeepp/connection.hpp>
#include <ironbeepp/transaction.hpp>
#include <ironbeepp/test_fixture.hpp>

#include "stream_inflate_private.h"

using namespace IronBee;

const uint8_t compressed_data[] = {
    0x78, 0x9c, 0x0b, 0xc9, 0xc8, 0x2c, 0x56, 0x00, 0xa2, 0x44, 0x85, 0x92,
    0xd4, 0xe2, 0x12, 0x2e, 0x00, 0x29, 0x73, 0x05, 0x00
};
const int compressed_data_len = 21;

const uint8_t uncompressed_data[] = {
    0x54, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x61, 0x20, 0x74, 0x65,
    0x73, 0x74, 0x0a
};
const int uncompressed_data_len = 15;

ib_status_t create_test_processor(
    void    *instance_data,
    ib_tx_t *tx,
    void    *cbdata
)
{
    std::string* collector = static_cast<std::string*>(cbdata);
    collector->clear();
    return IB_OK;
}

void destroy_test_processor(
    void *instance_data,
    void *cbdata
)
{
    // NOOP
}

ib_status_t execute_test_processor(
    void                *instance_data,
    ib_tx_t             *tx,
    ib_mm_t              mm_eval,
    ib_stream_io_tx_t   *io_tx,
    void                *cbdata
)
{
    std::string* collector = static_cast<std::string*>(cbdata);

    ib_stream_io_type_t  data_type;
    ib_stream_io_data_t *stream_data;
    uint8_t *buf;
    size_t buf_len;
    ib_status_t rc;

    rc = ib_stream_io_data_take(io_tx, &stream_data, &buf, &buf_len, &data_type);
    collector->append(reinterpret_cast<char*>(buf), buf_len);
    if (rc == IB_ENOENT) {
      rc = IB_OK;
    }

    ib_stream_io_data_put(io_tx, stream_data);
    return IB_OK;
}

class TestStream
    : public ::testing::Test, public TestFixture {

protected:
    virtual void SetUp() {
        m_reg = ib_engine_stream_processor_registry(m_engine.ib());
        List<const char *> types =
            List<const char *>::create(m_engine.main_memory_mm());
        types.push_back("compressed");

        ASSERT_EQ(IB_OK,
                  ib_stream_processor_registry_register(
                      m_reg,
                      "inflate",
                      types.ib(),
                      create_inflate_processor,
                      NULL,
                      execute_inflate_processor,
                      NULL,
                      destroy_inflate_processor,
                      NULL
                  ));
       ASSERT_EQ(IB_OK,
                  ib_stream_processor_registry_register(
                      m_reg,
                      "collector",
                      types.ib(),
                      create_test_processor,
                      (void *) &m_collector,
                      execute_test_processor,
                      (void *) &m_collector,
                      destroy_test_processor,
                      NULL
                  ));
    }

    ib_stream_processor_registry_t *m_reg;
    std::string m_collector;
};

TEST_F(TestStream, Simple)
{
    Connection c = Connection::create(m_engine);
    Transaction tx = Transaction::create(c);
    ib_stream_pump_t *pump;
    ASSERT_EQ(IB_OK,
              ib_stream_pump_create(&pump, m_reg, tx.ib())
             );
    ASSERT_EQ(IB_OK,
              ib_stream_pump_processor_add(pump, "inflate")
             );
    ASSERT_EQ(IB_OK,
              ib_stream_pump_processor_add(pump, "collector")
             );

    ASSERT_EQ(IB_OK,
              ib_stream_pump_flush(pump)
             );
    ASSERT_EQ(IB_OK,
              ib_stream_pump_process(pump, compressed_data, 3)
             );
    ASSERT_EQ(IB_OK,
              ib_stream_pump_process(pump,
                                     compressed_data + 3,
                                     compressed_data_len - 3)
             );
    ASSERT_EQ(IB_OK,
              ib_stream_pump_flush(pump)
             );
    ASSERT_EQ(0,
              m_collector.compare(0, std::string::npos,
                                  reinterpret_cast<const char*>(uncompressed_data),
                                  uncompressed_data_len));
}
