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
 * @brief IronBee Modules --- EngineShutdown
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include <ironbeepp/connection.hpp>
#include <ironbeepp/engine.hpp>
#include <ironbeepp/hooks.hpp>
#include <ironbeepp/module_bootstrap.hpp>
#include <ironbeepp/module_delegate.hpp>
#include <ironbeepp/parsed_header.hpp>
#include <ironbeepp/transaction.hpp>

/* C includes. */
extern "C" {
#include <ironbee/engine.h>
#include <ironbee/server.h>
}

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/version.hpp>

#if BOOST_VERSION >= 105300
#define HAVE_BOOST_ATOMIC_HPP 1
#endif

#ifdef HAVE_BOOST_ATOMIC_HPP

// Avoid a part of atomic.hpp.
#ifndef BOOST_NO_CXX11_CHAR16_T
#define BOOST_NO_CXX11_CHAR16_T 1
#endif

// Avoid a part of atomic.hpp.
#ifndef BOOST_NO_CXX11_CHAR32_T
#define BOOST_NO_CXX11_CHAR32_T 1
#endif

#include <boost/atomic.hpp>

// Force the user to accept not-lock-free use of the EngineShutdown module
// if they cannot compile lock free pointer access.
#if !defined(IBMOD_ENGINE_SHUTDOWN_LOCK)

#if BOOST_ATOMIC_ADDRESS_LOCK_FREE != 2 || BOOST_ATOMIC_INT_LOCK_FREE != 2
#error "EngineShutdown Module is not lock-free. Define IBMOD_ENGINE_SHUTDOWN_LOCK to ignore."
#endif

#endif

#else
extern "C" {
#include <signal.h>
}
#endif

/**
 * Implement simple policy changes when the IronBee engines is to shutdown.
 */
class EngineShutdownModule : public IronBee::ModuleDelegate
{
public:

    /**
     * Constructor.
     * @param[in] module The IronBee++ module.
     */
    explicit EngineShutdownModule(IronBee::Module module) :
        IronBee::ModuleDelegate(module),
        m_mode(RUNNING)
    {
        assert(module);

        module.engine().register_hooks()
            .transaction_started(
                boost::bind(
                    &EngineShutdownModule::on_transaction_started,
                    this,
                    _1,
                    _2
                )
            )
            .response_header_data(
                boost::bind(
                    &EngineShutdownModule::on_response_header_data,
                    this,
                    _1,
                    _2,
                    _3,
                    _4
                )
            )
            .connection_opened(
                boost::bind(
                    &EngineShutdownModule::on_connection_opened,
                    this,
                    _1,
                    _2
                )
            )
            .engine_shutdown_initiated(
                boost::bind(
                    &EngineShutdownModule::on_engine_shutdown_initiated,
                    this,
                    _1
                )
            );
    }

    /**
     * The mode of the module.
     *
     * Normally this is always RUNNING. When an
     * @ref engine_shutdown_initiated_event
     * is received this is set to STOPPING.
     *
     * When this module is set to STOPPING it will begin taking
     * actions to close the transport layer connections with clients
     * to allow the current IronBee engine to be cleaned up quickly.
     */
    enum mode_t {
        RUNNING, /**< Normal mode. */
        STOPPING /**< Promote connection closure. Set by shutdown. */
    };

private:

    /**
     * Get the current @ref mode_t of this module.
     * @returns
     *   - @ref STOPPING if the shutdown initiated hook has fired.
     *   - @ref RUNNING otherwise.
     */
    mode_t get_mode() const
    {
#ifdef HAVE_BOOST_ATOMIC_HPP
        return m_mode.load(boost::memory_order_acquire);
#else
        return (mode_t)m_mode;
#endif
    }

    /**
     * Set the current mode in an atomic manner.
     * @sa mode_t
     * @param[in] mode The mode.
     */
    void set_mode(mode_t mode)
    {
#ifdef HAVE_BOOST_ATOMIC_HPP
        m_mode.store(mode, boost::memory_order_release);
#else
        m_mode = (sig_atomic_t)mode;
#endif
    }

    /**
     * Block a transaction if get_mode() returns not @ref RUNNING.
     *
     * @sa mode_t
     *
     * @param[in] ib IronBee Engine.
     * @param[in] tx The transaction.
     * @param[in] event The shutdown event.
     */
    void on_transaction_started(
        IronBee::Engine ib,
        IronBee::Transaction tx) const
    {
        if (get_mode() != RUNNING) {
            ib_log_error(
                ib.ib(),
                "New transaction started after shutdown req.");
        }
    }

    /**
     * Add headers to promote closing if get_mode() returns not @ref RUNNING.
     *
     * @sa mode_t
     *
     * @param[in] ib IronBee Engine.
     * @param[in] event The shutdown event.
     */
    void on_response_header_data(
        IronBee::Engine ib,
        IronBee::Transaction tx,
        IronBee::Engine::state_event_e event,
        IronBee::ParsedHeader header) const
    {
        if (get_mode() != RUNNING) {
            ib_status_t rc = ib_tx_server_header(
                tx.ib(),
                IB_SERVER_RESPONSE,
                IB_HDR_SET,
                "Connection",
                "close",
                NULL);
            if (rc != IB_OK) {
            }
        }
    }

    /**
     * Use set_mode() to change this module into @ref STOPPING mode.
     *
     * @sa mode_t
     *
     * @param[in] ib IronBee Engine.
     * @param[in] event The shutdown event.
     */
    void on_connection_opened(
        IronBee::Engine ib,
        IronBee::Connection conn) const
    {
        if (get_mode() != RUNNING) {
            ib_log_error(
                ib.ib(),
                "New connection started after shutdown req.");
        }
    }

    /**
     * Use set_mode() to change this module into @ref STOPPING mode.
     *
     * @sa mode_t
     *
     * @param[in] ib IronBee Engine.
     * @param[in] event The shutdown event.
     */
    void on_engine_shutdown_initiated(IronBee::Engine ib)
    {
        ib_log_info(ib.ib(), "EngineShutdown module entering shutdown mode.");

        set_mode(STOPPING);
    }

    /**
     * The mode configuration.
     */
#ifdef HAVE_BOOST_ATOMIC_HPP
    boost::atomic<mode_t>
#else
    sig_atomic_t
#endif
        m_mode;
};

IBPP_BOOTSTRAP_MODULE_DELEGATE("EngineShutdownModule", EngineShutdownModule);
