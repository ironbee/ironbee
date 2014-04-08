
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
/// @brief IronBee --- Engine Manager Channel Test Functions
///
/// @author Sam Baskinger <sbaskinger@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"
#include "base_fixture.h"

#include <ironbee/engine_manager.h>
#include <ironbee/engine_manager_control_channel.h>

#include <boost/filesystem.hpp>
#include <boost/thread.hpp>

class EngMgrCtrlChanTest : public BaseFixture
{
public:
    void SetUp()
    {
        BaseFixture::SetUp();
        ASSERT_EQ(
            IB_OK,
            ib_manager_create(
                &m_ib_manager,
                &ibt_ibserver,
                10
            )
        );
    }

    virtual void TearDown()
    {
        ib_manager_destroy(m_ib_manager);
        BaseFixture::TearDown();
    }

    ib_manager_t* EngineManager() const
    {
        return m_ib_manager;
    }

private:
    ib_manager_t* m_ib_manager;
};

TEST_F(EngMgrCtrlChanTest, init) {
    ib_engine_manager_control_channel_t* channel;

    ASSERT_EQ(
        IB_OK,
        ib_engine_manager_control_channel_create(
            &channel,
            MainMM(),
            EngineManager()
        )
    );
}

TEST_F(EngMgrCtrlChanTest, socket_path)
{
    ib_engine_manager_control_channel_t* channel;

    ASSERT_EQ(
        IB_OK,
        ib_engine_manager_control_channel_create(
            &channel,
            MainMM(),
            EngineManager()
        )
    );

    /* Check that a default is defined. */
    ASSERT_TRUE(
        ib_engine_manager_control_channel_socket_path_get(channel)
    );

    ASSERT_EQ(
        IB_OK,
        ib_engine_manager_control_channel_socket_path_set(channel, "path")
    );

    /* Check the custom one. */
    ASSERT_STREQ(
        "path",
        ib_engine_manager_control_channel_socket_path_get(channel)
    );
}

TEST_F(EngMgrCtrlChanTest, start_stop)
{
    ib_engine_manager_control_channel_t* channel;

    ASSERT_EQ(
        IB_OK,
        ib_engine_manager_control_channel_create(
            &channel,
            MainMM(),
            EngineManager()
        )
    );

    ASSERT_EQ(
        IB_OK,
        ib_engine_manager_control_channel_socket_path_set(channel, "./tmp.sock")
    );

    ASSERT_EQ(
        IB_OK,
        ib_engine_manager_control_channel_start(channel)
    );

    /* Check that the file exists and is not a normal file or dir. */
    ASSERT_TRUE(boost::filesystem::is_other("./tmp.sock"));

    ASSERT_EQ(
        IB_OK,
        ib_engine_manager_control_channel_stop(channel)
    );

    ASSERT_FALSE(boost::filesystem::exists("./tmp.sock"));
}

TEST_F(EngMgrCtrlChanTest, send_echo)
{
    ib_engine_manager_control_channel_t* channel;
    const char* response;

    ASSERT_EQ(
        IB_OK,
        ib_engine_manager_control_channel_create(
            &channel,
            MainMM(),
            EngineManager()
        )
    );

    ASSERT_EQ(
        IB_OK,
        ib_engine_manager_control_channel_socket_path_set(channel, "./tmp.sock")
    );

    ASSERT_EQ(
        IB_OK,
        ib_engine_manager_control_echo_register(channel)
    );

    ASSERT_EQ(
        IB_OK,
        ib_engine_manager_control_channel_start(channel)
    );

    ASSERT_TRUE(boost::filesystem::is_other("./tmp.sock"));

    boost::packaged_task<ib_status_t> pt(
        boost::bind(
            ib_engine_manager_control_send,
            "./tmp.sock",
            "echo hi, how are you?",
            MainMM(),
            &response
        )
    );
    boost::shared_future<ib_status_t> fut = pt.get_future().share();
    boost::thread thr(boost::move(pt));

    ASSERT_EQ(
        IB_OK,
        ib_engine_manager_control_recv(
            channel
        )
    );

    thr.join();
    ASSERT_EQ(IB_OK, fut.get());

    ASSERT_STREQ(
        "hi, how are you?",
        response
    );

    ASSERT_EQ(
        IB_OK,
        ib_engine_manager_control_channel_stop(channel)
    );

    ASSERT_FALSE(boost::filesystem::exists("./tmp.sock"));
}

TEST_F(EngMgrCtrlChanTest, diag_version)
{
    ib_engine_manager_control_channel_t* channel;
    const char* response;

    ASSERT_EQ(
        IB_OK,
        ib_engine_manager_control_channel_create(
            &channel,
            MainMM(),
            EngineManager()
        )
    );

    ASSERT_EQ(
        IB_OK,
        ib_engine_manager_control_channel_socket_path_set(channel, "./tmp.sock")
    );

    ASSERT_EQ(
        IB_OK,
        ib_engine_manager_control_manager_diag_register(channel)
    );

    ASSERT_EQ(
        IB_OK,
        ib_engine_manager_control_channel_start(channel)
    );

    ASSERT_TRUE(boost::filesystem::is_other("./tmp.sock"));

    boost::packaged_task<ib_status_t> pt(
        boost::bind(
            ib_engine_manager_control_send,
            "./tmp.sock",
            "version",
            MainMM(),
            &response
        )
    );
    boost::shared_future<ib_status_t> fut = pt.get_future().share();
    boost::thread thr(boost::move(pt));

    ASSERT_EQ(
        IB_OK,
        ib_engine_manager_control_recv(
            channel
        )
    );

    thr.join();
    ASSERT_EQ(IB_OK, fut.get());

    ASSERT_STREQ(IB_VERSION, response);

    ASSERT_EQ(
        IB_OK,
        ib_engine_manager_control_channel_stop(channel)
    );

    ASSERT_FALSE(boost::filesystem::exists("./tmp.sock"));
}


