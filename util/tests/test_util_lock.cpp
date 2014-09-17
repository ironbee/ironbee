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
/// @brief IronBee --- Lock Test Functions
///
/// @author Nick LeRoy <nleroy@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "ironbee_config_auto.h"

#include <ironbee/types.h>
#include <ironbee/mm.h>
#include <ironbee/util.h>
#include <ironbee/lock.h>

#include "gtest/gtest.h"
#include "simple_fixture.hpp"

#include <stdexcept>
#include <math.h>
#include <pthread.h>

using namespace std;

/* -- Tests -- */
typedef void *(* thread_start_fn)(void *thread_data);

class Thread
{
public:
    explicit
    Thread(int num) :
        m_handle(0),
        m_num(num),
        m_started(false),
        m_running(false),
        m_errors(0)
    {
    }
    Thread() :
        m_handle(0),
        m_num(-1),
        m_started(false),
        m_running(false),
        m_errors(0)
    {
    }

    void ThreadNum(int num) { m_num = num; };
    bool IsRunning() const { return m_running; };
    int ThreadNum() const { return m_num; };
    uint64_t ThreadId() const { return (uint64_t)m_handle; };
    uint64_t Errors() const { return m_errors; };
    void Error() { ++m_errors; };

    ib_status_t Create(thread_start_fn fn)
    {
        pthread_t      handle;
        pthread_attr_t attr;
        int            status;

        if (m_num < 0) {
            return IB_EINVAL;
        }
        if (m_started || m_running) {
            return IB_EINVAL;
        }

        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

        m_started = true;
        status = pthread_create(&handle, &attr, fn, this);
        if (status) {
            m_started = false;
            return IB_EUNKNOWN;
        }

        m_handle = handle;
        return IB_OK;
    }

    ib_status_t Join()
    {
        int         status;

        if (! m_started) {
            return IB_OK;
        }
        status = pthread_join(m_handle, NULL);
        if (status != 0) {
            return IB_EUNKNOWN;
        }
        m_running = false;
        m_started = false;

        return IB_OK;
    }

    ib_status_t Running(bool run)
    {
        if (!m_started) {
            return IB_EINVAL;
        }
        if (run == m_running) {
            return IB_EINVAL;
        }
        m_running = run;
        return IB_OK;
    }

private:
    pthread_t        m_handle;
    int              m_num;
    bool             m_started;
    bool             m_running;
    uint64_t         m_errors;
};

class TestIBUtilLock : public SimpleFixture
{
public:
    TestIBUtilLock() :
        m_max_threads(0),
        m_threads(NULL),
        m_lock_enabled(true),
        m_shared(0)
    {
        TestIBUtilLock::m_self = this;
        TestParams(100, 0.0005, true);
    }
    ~TestIBUtilLock()
    {
        if (m_threads != NULL) {
            delete []m_threads;
        }
    }

    virtual void SetUp()
    {
        SimpleFixture::SetUp();
    }
    virtual void TearDown()
    {
        SimpleFixture::TearDown();
        DestroyLock( );
    }

    ib_status_t CreateLock()
    {
        return ib_lock_create_malloc(&m_lock);
    }
    ib_status_t DestroyLock()
    {
        ib_lock_destroy_malloc(m_lock);
        return IB_OK;
    }
    ib_status_t LockLock()
    {
        return ib_lock_lock(m_lock);
    }
    ib_status_t UnlockLock()
    {
        return ib_lock_unlock(m_lock);
    }

    void InitThreads(size_t max_threads)
    {
        m_max_threads = max_threads;
        m_threads = new Thread[max_threads];

        for (int num = 0;  num < (int)max_threads;  ++num) {
            m_threads[num].ThreadNum(num);
        }

    }
    void TestParams(size_t loops, double seconds, bool lock)
    {
        Loops(loops);
        SleepTime(seconds);
        m_lock_enabled = lock;
    }

    void Loops(size_t loops) { m_loops = loops; };
    void SleepTime(double sec) { m_sleeptime = sec; };

    ib_status_t CreateThread(size_t num)
    {
        if(m_threads == NULL) {
            throw std::runtime_error("Thread handles not initialized.");
        }
        if (num >= m_max_threads) {
            throw std::runtime_error("Thread number greater than max.");
        }
        Thread *thread = &m_threads[num];
        if (thread->IsRunning()) {
            throw std::runtime_error("Thread already running.");
        }
        return thread->Create(TestIBUtilLock::StartThread);
    }

    ib_status_t CreateAllThreads()
    {
        if(m_threads == NULL) {
            throw std::runtime_error("Thread handles not initialized.");
        }

        size_t num;
        for (num = 0;  num < m_max_threads;  ++num) {
            ib_status_t rc = CreateThread(num);
            if (rc != IB_OK) {
                return rc;
            }
        }
        return IB_OK;
    }

    ib_status_t StartTest(size_t threads,
                          size_t loops,
                          double seconds,
                          bool lock)
    {
        InitThreads(threads);
        TestParams(loops, seconds, lock);
        printf("Starting: %zd threads, %d loops, %.8fs sleep, locks %s\n",
               m_max_threads, m_loops, m_sleeptime,
               m_lock_enabled ? "enabled" : "disabled");
        return CreateAllThreads( );
    }

    static void *StartThread(void *data)
    {
        Thread *thread = (Thread *)data;
        m_self->RunThread(thread);
        return NULL;
    }

    void RunThread(Thread *thread)
    {
        ib_status_t     rc;
        int             n;
        struct timespec ts;

        ts.tv_sec = (time_t)trunc(m_sleeptime);
        ts.tv_nsec = (long)(round((m_sleeptime - ts.tv_sec) * 1e9));

        thread->Running(true);

        for (n = 0;  n < m_loops;  ++n) {
            if (m_lock_enabled) {
                rc = LockLock( );
                if (rc != IB_OK) {
                    thread->Error();
                    break;
                }
            }
            // This code is an intentional race condition if m_lock_enabled is
            // false.  It is possible for it to fail to cause errors, but, at
            // least in common environments, that is very unlikely.
            if (++m_shared != 1) {
                thread->Error();
            }
            nanosleep(&ts, NULL);
            if (--m_shared != 0) {
                thread->Error();
            }

            if (m_lock_enabled) {
                rc = UnlockLock( );
                if (rc != IB_OK) {
                    thread->Error();
                    break;
                }
            }
        }
        thread->Running(false);
    }

    ib_status_t WaitForThreads(uint64_t *errors)
    {
        ib_status_t rc = IB_OK;
        size_t num;

        *errors = 0;
        for (num = 0;  num < m_max_threads;  ++num) {
            Thread *thread = &m_threads[num];
            ib_status_t irc = thread->Join();
            if (irc != IB_OK) {
                rc = irc;
            }
            *errors += thread->Errors();
        }
        return rc;
    }

private:
    static TestIBUtilLock *m_self;
    size_t                 m_max_threads;
    Thread                *m_threads;
    ib_lock_t             *m_lock;
    bool                   m_lock_enabled;
    int                    m_loops;
    double                 m_sleeptime;
    volatile int           m_shared;
};
TestIBUtilLock *TestIBUtilLock::m_self = NULL;

TEST(test_util_lock, misc)
{
    int test = 0;

    for (int n=0; n<100; ++n) {
        int t;
        t = ++test;
        ASSERT_EQ(1, t);
        t = --test;
        ASSERT_EQ(0, t);
    }
}

TEST_F(TestIBUtilLock, test_create)
{
    ib_status_t rc;

    rc = CreateLock( );
    ASSERT_EQ(IB_OK, rc);

    rc = LockLock( );
    ASSERT_EQ(IB_OK, rc);

    rc = UnlockLock( );
    ASSERT_EQ(IB_OK, rc);

    rc = DestroyLock( );
    ASSERT_EQ(IB_OK, rc);
}

// The following test is a true positive for a thread race condition.
// Disable it for thread sanitizer.
TEST_F(TestIBUtilLock, test_lock_disabled)
{
#ifdef IB_THREAD_SANITIZER_WORKAROUND
    cout << "Test skipped due to thread sanitizer." << endl;
    return;
#else
    ib_status_t rc;
    uint64_t    errors;

    rc = CreateLock( );
    ASSERT_EQ(IB_OK, rc);

    rc = StartTest(5, 100, 0.0000005, false);
    ASSERT_EQ(IB_OK, rc);

    rc = WaitForThreads( &errors );
    ASSERT_EQ(IB_OK, rc);
    ASSERT_NE(0LU, errors);

    rc = DestroyLock( );
    ASSERT_EQ(IB_OK, rc);
#endif
}

TEST_F(TestIBUtilLock, test_short)
{
    ib_status_t rc;
    uint64_t    errors;

    rc = CreateLock( );
    ASSERT_EQ(IB_OK, rc);

    rc = StartTest(5, 100, 0.0000005, true);
    ASSERT_EQ(IB_OK, rc);

    rc = WaitForThreads( &errors );
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(0LU, errors);

    rc = DestroyLock( );
    ASSERT_EQ(IB_OK, rc);
}

// This test is too intense for the thread sanitizer.
TEST_F(TestIBUtilLock, test_long)
{
#ifdef IB_THREAD_SANITIZER_WORKAROUND
    cout << "Test skipped due to thread sanitizer." << endl;
    return;
#else
    ib_status_t rc;
    uint64_t    errors;

    rc = CreateLock( );
    ASSERT_EQ(IB_OK, rc);

    rc = StartTest(20, 1000, 0.00005, true);
    ASSERT_EQ(IB_OK, rc);

    rc = WaitForThreads( &errors );
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(0LU, errors);

    rc = DestroyLock( );
    ASSERT_EQ(IB_OK, rc);
#endif
}
