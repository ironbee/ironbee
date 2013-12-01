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
 * @brief IronBee Modules --- Transaction Logs
 *
 * The TxLog module, if enabled for a site, writes transaction logs.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include "ironbee_config_auto.h"

/* Include our own public header file. */
#include "txlog.h"
#include "txlog_private.hpp"

#include <ironbeepp/configuration_directives.hpp>
#include <ironbeepp/configuration_parser.hpp>
#include <ironbeepp/connection.hpp>
#include <ironbeepp/c_trampoline.hpp>
#include <ironbeepp/hooks.hpp>
#include <ironbeepp/module.hpp>
#include <ironbeepp/module_bootstrap.hpp>
#include <ironbeepp/module_delegate.hpp>
#include <ironbeepp/parsed_header.hpp>
#include <ironbeepp/parsed_request_line.hpp>
#include <ironbeepp/parsed_response_line.hpp>
#include <ironbeepp/site.hpp>
#include <ironbeepp/transaction.hpp>

#include <ironbee/core.h>
#include <ironbee/logevent.h>
#include <ironbee/rule_engine.h>
#include <ironbee/string.h>

#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/time_facet.hpp>
#include <boost/function.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/shared_ptr.hpp>

/* Enable PRId64 printf. */
extern "C" {
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>
}

namespace {

/**
 * State data built and stored in transactions.
 */
class TxLogData {
public:
    //! The response blocking method or "-".
    std::string responseBlockMethod;

    //! The response blocking action or "-".
    std::string responseBlockAction;

    //! The request blocking method or "-".
    std::string requestBlockMethod;

    //! The request blocking action or "-".
    std::string requestBlockAction;

    //! The name of the auditlog file or "-".
    std::string auditlogFile;

    /**
     * Constructor.
     */
    TxLogData();

    /**
     * Record data about a response from a transaction.
     *
     * @param[in] tx The transaction to examine.
     */
    void recordResponseData(IronBee::ConstTransaction tx);

    /**
     * Record data about a request from a transaction.
     *
     * @param[in] tx The transaction to examine.
     */
    void recordRequestData(IronBee::ConstTransaction tx);

    /**
     * Record auditlog information in the given transaction.
     *
     * @param[in] tx Transaction.
     * @param[in] auditlog The auditlog to examine.
     */
    void recordAuditLogData(
        IronBee::ConstTransaction tx,
        ib_auditlog_t *auditlog);

private:

    /**
     * Record data about blocking status into @a action and @a method.
     *
     * @param[in] tx Transaction.
     * @param[out] action The blocking action currently in use.
     * @param[out] method The blocking method currently in use.
     */
    void recordBlockData(
        IronBee::ConstTransaction tx,
        std::string &action,
        std::string &method);

};

TxLogData::TxLogData() :
    responseBlockMethod("-"),
    responseBlockAction("-"),
    requestBlockMethod("-"),
    requestBlockAction("-"),
    auditlogFile("-")
{
}

void TxLogData::recordBlockData(
    IronBee::ConstTransaction tx,
    std::string &action,
    std::string &method
)
{

    /* Insert Request Action */
    if (ib_tx_flags_isset(tx.ib(), IB_TX_ALLOW_REQUEST | IB_TX_ALLOW_ALL)) {
        action = "Allow";
        method = "-";
    }
    else if(ib_tx_flags_isset(tx.ib(), IB_TX_FBLOCKED))
    {
        action = "Blocked";

        switch(tx.ib()->block_method) {
        case IB_BLOCK_METHOD_STATUS:
            method = "Close";
            break;
        case IB_BLOCK_METHOD_CLOSE:
            method = "ErrorPage";
            break;
        default:
            method = "-";
        }
    }
    else {
        action = "Passed";
        method = "-";
    }
}

void TxLogData::recordResponseData(IronBee::ConstTransaction tx) {
    recordBlockData(tx, responseBlockAction, responseBlockMethod);
}

void TxLogData::recordRequestData(IronBee::ConstTransaction tx) {
    recordBlockData(tx, requestBlockAction, requestBlockMethod);
}

void TxLogData::recordAuditLogData(
    IronBee::ConstTransaction tx,
    ib_auditlog_t *auditlog
)
{
    auditlogFile = auditlog->cfg_data->full_path;
}
extern "C" {

/**
 * An implementation of @ref ib_logger_format_fn_t for this module.
 *
 * It produces a @ref ib_logger_standard_msg_t in @a writer_record.
 *
 * The produced @a writer_record should be freed with
 * ib_logger_standard_msg_free().
 *
 * @param[in] logger The logger.
 * @param[in] rec The record to produce @a writer_record from.
 * @param[in] log_msg_sz The length of @a log_msg.
 * @param[out] writer_record A @ref ib_logger_standard_msg_t if this returns
 *             IB_OK. Unset otherwise. This must be a ib_logger_std_msg_t **.
 * @param[in] data Callback data.
 */
ib_status_t txlog_logger_format_fn(
    ib_logger_t   *logger,
    const          ib_logger_rec_t *rec,
    const uint8_t *log_msg,
    const size_t   log_msg_sz,
    void          *writer_record,
    void          *data
)
{
    assert(logger != NULL);
    assert(rec != NULL);

    /* Do not handle non-tx recs. */
    if (rec->tx == NULL || rec->type != IB_LOGGER_TXLOG_TYPE) {
        return IB_DECLINED;
    }

    ib_logger_standard_msg_t *stdmsg;
    std::ostringstream logstr;

    /* Wrap some types into IronBee++. */
    IronBee::ConstTransaction tx(rec->tx);
    IronBee::ConstConnection  conn(rec->conn);
    IronBee::ConstModule      module(rec->module);

    /* Fetch some telemetry from our tx. */
    TxLogData &txlogdata = IronBee::Transaction::remove_const(tx)
        .get_module_data<TxLogData&>(module);

    /* Setup posix time formatting. */
    logstr.imbue(
        std::locale(
            logstr.getloc(),
            new boost::date_time::time_facet
            <
                boost::posix_time::ptime,
                char
            > (
                "%Y-%m-%d %H:%M:%S %q"
            )
        )
    );

    /* Append start time. */
    logstr << "["
           << tx.started_time()
           << "]" ;

    /* Insert UUIDs. */
    logstr << "[" << ib_engine_sensor_id(tx.engine().ib());
    if (
        conn == IronBee::ConstConnection() ||
        conn.context() == IronBee::ConstContext() ||
        conn.context().site() == IronBee::ConstSite() 
    )
    {
        logstr << " -";
    }
    else {
        logstr << " " << conn.context().site().id_as_s();
    }
    logstr << " " << tx.id()
           << "]";

    /* Insert IP information. */
    logstr << "[" << "-" // FIXME - remote ip
           << " " << "-" // FIXME - remove port
           << " " << "-" // FIXME - local port
           << " " << "-" // FIXME - origin ip
           << " " << "-" // FIXME - origin port
           << "]";

    /* Insert encryption info. */
    logstr << "["
           << " - " /* TODO - when encryption info is available, replace. */
           << "]";

    /* Insert HTTP Status info. */
    logstr << "[" << tx.request_line().method().to_s()
           << " " << tx.request_line().uri().to_s()
           << " " << tx.request_line().protocol().to_s()
           << "]";

    /* Insert HTTP Request Normalized Data */
    logstr << "[" << tx.hostname()
           << " " << "Order=-" /* TODO - replace when available. */
           << "]";

    /* Insert request headers. */
    logstr << "[";
    if (tx.request_header()) {
        for (
            IronBee::ConstParsedHeader headerNvp = tx.request_header();
            headerNvp;
            headerNvp = headerNvp.next()
        )
        {
            logstr << '"' << headerNvp.name().to_s()
                << '=' << headerNvp.value().to_s()
                << "\" ";
        }
    }
    else {
        logstr << " -  ";
    }
    /* Erase the extra trailing space. */
    logstr.seekp(-1, std::ios_base::cur);
    logstr << "]";

    logstr << "[" << txlogdata.requestBlockAction
           << " " << txlogdata.requestBlockMethod
           << "]";
    /* Insert Response */
    logstr << "[" << tx.response_line().protocol().to_s()
           << " " << tx.response_line().status().to_s()
           << " " << tx.response_line().message().to_s()
           << "]";

    /* Insert Response Normalized Data */
    logstr << "[\"Order=-\"]"; /* TODO - replace when available. */

    /* Insert response headers. */
    if (tx.response_header()) {
        for (
            IronBee::ConstParsedHeader headerNvp = tx.response_header();
            headerNvp;
            headerNvp = headerNvp.next()
        )
        {
            logstr << '"' << headerNvp.name().to_s()
                << '=' << headerNvp.value().to_s()
                << "\" ";
        }
    }
    else {
        logstr << " -  ";
    }
    /* Erase the extra trailing space. */
    logstr.seekp(-1, std::ios_base::cur);
    logstr << "]";

    /* Insert the response actions. */
    logstr << "[" << txlogdata.responseBlockAction
           << " " << txlogdata.responseBlockMethod
           << "]";

    /* Insert Session. */
    logstr << "[ - ]";

    /* Insert content stats. */
    logstr << "[" << "-" // "remote client waf request size." // FIXME
           << " " << "-" // "waf to origin request size" // FIXME
           << " " << "-" // "origin to waf response size" // FIXME
           << " " << "-" // "waf to remote response size" // FIXME
           << "]";

    /* Insert generated audit log. */
    logstr << "[AuditLog " << txlogdata.auditlogFile << " ]";

    /* Insert events. */
    IronBee::ConstList<ib_logevent_t *> eventList(tx.ib()->logevents);
    BOOST_FOREACH(const ib_logevent_t *e, eventList) {
        logstr << "[Event " 
               << " " << "-" // FIXME - category
               << " " << "-" // FIXME - matched location
               << " " << e->rule_id
               << " " << e->event_id << "]";
    }

    /* Build stdmsg and return it. */
    stdmsg =
        reinterpret_cast<ib_logger_standard_msg_t *>(
            malloc(sizeof(*stdmsg)));
    if (stdmsg == NULL) {
        return IB_EALLOC;
    }

    stdmsg->prefix = NULL;
    stdmsg->msg_sz = logstr.str().length();
    stdmsg->msg    =
        reinterpret_cast<uint8_t *>(
            strndup(
                reinterpret_cast<const char *>(
                    logstr.str().data()), logstr.str().length()));

    *reinterpret_cast<void **>(writer_record) = stdmsg;
    return IB_OK;
}

} /* extern "C" */
} /* Anonymous namespace. */

/**
 * Transaction log module.
 */
class TxLogModule : public IronBee::ModuleDelegate
{
public:
    /**
     * Constructor.
     *
     * @param[in] module The IronBee++ module.
     */
    explicit TxLogModule(IronBee::Module module);

private:

    ///! Container for C callback and callback data.
    std::pair<ib_core_auditlog_fn_t, void *> m_recordAuditLogInfoTrampoline;

    ///! Object that destroys m_recordAuditLogInfoTrampoline.second, void *.
    boost::shared_ptr<void> m_recordAuditLogInfoTrampolinePtr;

    ///! Enable/Disable directive callback.
    void onOffDirective(
        IronBee::ConfigurationParser  cp,
        const char                   *name,
        bool                          enabled
    );

    ///! TxLogBaseFileName config directive callback.
    void logBaseNameDirective(
        IronBee::ConfigurationParser  cp,
        const char                   *name,
        const char                   *param1
    );

    ///! TxLogBaseDirectory config directive callback.
    void logBaseDirDirective(
        IronBee::ConfigurationParser  cp,
        const char                   *name,
        const char                   *param1
    );

    ///! TxLogSizeLimit config directive callback.
    void logSizeLimitDirective(
        IronBee::ConfigurationParser  cp,
        const char                   *name,
        const char                   *param1
    );

    ///! TxLogAgeLimit config directive callback.
    void logAgeLimitDirective(
        IronBee::ConfigurationParser  cp,
        const char                   *name,
        const char                   *param1
    );

    ///! Callback to log @a tx through the Logger of @a ib.
    void transactionFinishedHandler(
        IronBee::Engine      ib,
        IronBee::Transaction tx
    );

    ///! Callback to log @a tx through the Logger of @a ib.
    void transactionStartedHandler(
        IronBee::Engine      ib,
        IronBee::Transaction tx
    );

    ///! Callback that collects information about a request so as to log it.
    void handleRequest(
        IronBee::Engine      ib,
        IronBee::Transaction tx
    );

    ///! Callback that collects information about a response so as to log it.
    void handleResponse(
        IronBee::Engine      ib,
        IronBee::Transaction tx
    );

    /**
     * Is a @ref ib_core_auditlog_fn_t to collect data about auditlogs.
     *
     * @param[in] ib IronBee engine.
     * @param[in] tx Transaction.
     * @param[in] event The current event.
     * @param[in] auditlog The auditlog to examine.
     *
     * @returns
     * - IB_OK On success.
     * - Other on failure.
     */
    ib_status_t recordAuditLogInfo(
        ib_engine_t               *ib,
        ib_tx_t                   *tx,
        ib_core_auditlog_event_en  event,
        ib_auditlog_t             *auditlog);
};

IBPP_BOOTSTRAP_MODULE_DELEGATE(TXLOG_MODULE_NAME, TxLogModule);

/**
 * Setup some good defaults.
 */
TxLogConfig::TxLogConfig() {

    /* First, zero-out the C-struct. */
    memset(&pub_cfg, 0, sizeof(pub_cfg));

    /* Now set all the values we care about. */
    pub_cfg.is_enabled       = true;
    pub_cfg.log_basename     = "txlog";
    pub_cfg.log_basedir      = "/var/log/ironbee/txlogs";
    pub_cfg.max_size         = 5 * 1024;
    pub_cfg.max_age          = 60 * 10;
    pub_cfg.logger_format_fn = &txlog_logger_format_fn;
}

/* Implementation */

TxLogModule::TxLogModule(IronBee::Module module):
    /* Initialize the parent. */
    IronBee::ModuleDelegate(module),

    /* Store the c trampoline so it is cleaned up with the module. */
    m_recordAuditLogInfoTrampoline(
        IronBee::make_c_trampoline<
            ib_status_t(
                ib_engine_t *,
                ib_tx_t *,
                ib_core_auditlog_event_en,
                ib_auditlog_t *
            )
        >
        (
            boost::bind(&TxLogModule::recordAuditLogInfo, this, _1, _2, _3, _4)
        )
    ),

    /* Stuff the void* into a smart pointer to auto-delete it. */
    m_recordAuditLogInfoTrampolinePtr(
        m_recordAuditLogInfoTrampoline.second,
        IronBee::delete_c_trampoline
    )
{
    /* Set the default configuration. */
    module.set_configuration_data(TxLogConfig());

    /* Register configuration directives. */
    module.engine().register_configuration_directives()
        .on_off(
            "TxLogEnabled",
            boost::bind(&TxLogModule::onOffDirective, this, _1, _2, _3))
        .param1(
            "TxLogBaseDirectory",
            boost::bind(&TxLogModule::logBaseDirDirective, this, _1, _2, _3))
        .param1(
            "TxLogBaseFileName",
            boost::bind(&TxLogModule::logBaseNameDirective, this, _1, _2, _3))
        .param1(
            "TxLogSizeLimit",
            boost::bind(&TxLogModule::logSizeLimitDirective, this, _1, _2, _3))
        .param1(
            "TxLogAgeLimit",
            boost::bind(&TxLogModule::logAgeLimitDirective, this, _1, _2, _3))
        ;

    /* Register engine callbacks. */
    module.engine().register_hooks()
        .transaction(
            IronBee::Engine::transaction_started,
            boost::bind(&TxLogModule::transactionStartedHandler, this, _1, _2))
        .transaction(
            IronBee::Engine::transaction_finished,
            boost::bind(&TxLogModule::transactionFinishedHandler, this, _1, _2))
        .transaction(
            IronBee::Engine::handle_request,
            boost::bind(&TxLogModule::handleRequest, this, _1, _2))
        .transaction(
            IronBee::Engine::handle_response,
            boost::bind(&TxLogModule::handleResponse, this, _1, _2))
        ;

    /* Register a core module auditlog callback. */
    IronBee::throw_if_error(
        ib_core_add_auditlog_handler(
            module.engine().main_context().ib(),
            m_recordAuditLogInfoTrampoline.first,
            m_recordAuditLogInfoTrampoline.second
        ),
        "Failed to register auditlog handler with core module."
    );
}

void TxLogModule::onOffDirective(
        IronBee::ConfigurationParser  cp,
        const char                   *name,
        bool                          enabled)
{
    TxLogConfig &cfg =
        module().configuration_data<TxLogConfig>(cp.current_context());

    /* Set the mapping in the context configuration. */
    cfg.pub_cfg.is_enabled = enabled;
}

void TxLogModule::logBaseNameDirective(
        IronBee::ConfigurationParser  cp,
        const char                   *name,
        const char                   *param1)
{
    TxLogConfig &cfg =
        module().configuration_data<TxLogConfig>(cp.current_context());

    /* Set the mapping in the context configuration. */
    cfg.pub_cfg.log_basedir = ib_mpool_strdup(cp.ib()->mp, param1);
}

void TxLogModule::logBaseDirDirective(
        IronBee::ConfigurationParser  cp,
        const char                   *name,
        const char                   *param1)
{
    TxLogConfig &cfg =
        module().configuration_data<TxLogConfig>(cp.current_context());

    /* Set the mapping in the context configuration. */
    cfg.pub_cfg.log_basename = ib_mpool_strdup(cp.ib()->mp, param1);
}

void TxLogModule::logSizeLimitDirective(
        IronBee::ConfigurationParser  cp,
        const char                   *name,
        const char                   *param1)
{
    TxLogConfig &cfg =
        module().configuration_data<TxLogConfig>(cp.current_context());

    IronBee::throw_if_error(
        ib_string_to_num(param1, 10, &cfg.pub_cfg.max_size));
}

void TxLogModule::logAgeLimitDirective(
        IronBee::ConfigurationParser  cp,
        const char                   *name,
        const char                   *param1)
{
    TxLogConfig &cfg =
        module().configuration_data<TxLogConfig>(cp.current_context());

    IronBee::throw_if_error(
        ib_string_to_num(param1, 10, &cfg.pub_cfg.max_age));
}

ib_status_t TxLogModule::recordAuditLogInfo(
    ib_engine_t               *ib,
    ib_tx_t                   *ib_tx,
    ib_core_auditlog_event_en  event,
    ib_auditlog_t             *auditlog
)
{
    if (event == IB_CORE_AUDITLOG_CLOSED) {
        IronBee::Transaction tx(ib_tx);
        TxLogData &data = tx.get_module_data<TxLogData&>(module());

        data.recordAuditLogData(
            IronBee::ConstTransaction(ib_tx),
            auditlog);
    }

    return IB_OK;
}

void TxLogModule::transactionStartedHandler(
    IronBee::Engine      ib,
    IronBee::Transaction tx
)
{
    tx.set_module_data(module(), TxLogData());
}

void TxLogModule::handleRequest(
    IronBee::Engine      ib,
    IronBee::Transaction tx
)
{
    TxLogData &data = tx.get_module_data<TxLogData&>(module());

    data.recordRequestData(tx);
}

void TxLogModule::handleResponse(
    IronBee::Engine      ib,
    IronBee::Transaction tx
)
{
    TxLogData &data = tx.get_module_data<TxLogData&>(module());

    data.recordResponseData(tx);
}

void TxLogModule::transactionFinishedHandler(
    IronBee::Engine      ib,
    IronBee::Transaction tx
)
{
    assert(ib.ib() != NULL);
    assert(tx.ib() != NULL);

    ib_logger_log_va(
        ib_engine_logger_get(ib.ib()),
        IB_LOGGER_TXLOG_TYPE,
        __FILE__,
        __func__,
        __LINE__,
        ib.ib(),
        module().ib(),
        tx.connection().ib(),
        tx.ib(),
        IB_LOG_EMERGENCY,
        "no message"
    );
}
