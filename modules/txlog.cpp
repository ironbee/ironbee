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
#include "txlog_json.hpp"

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
#include <ironbeepp/var.hpp>

#include <ironbee/core.h>
#include <ironbee/logevent.h>
#include <ironbee/rule_engine.h>
#include <ironbee/string.h>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/time_facet.hpp>
#include <boost/function.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/lexical_cast.hpp>

#if defined(__cplusplus) && !defined(__STDC_FORMAT_MACROS)
/* C99 requires that inttypes.h only exposes PRI* macros
 * for C++ implementations if this is defined: */
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

namespace {

/**
 * State data built and stored in transactions.
 */
class TxLogData {
public:
    //! The blocking phase or "".
    const std::string& blockPhase() const;

    //! The blocking method or "".
    const std::string& blockMethod() const;

    //! The blocking action or "".
    const std::string& blockAction() const;

    //! The name of the auditlog file or "".
    const std::string& auditlogFile() const;

    //! The audit log boundary. Consider this the auditlog ID.
    const std::string &auditlogId() const;

    /**
     * Sets block and action data at request time.
     *
     * - TxLogData::m_blockPhase
     * - TxLogData::m_blockAction
     * - TxLogData::m_blockMethod
     *
     * @param[in] tx The transaction to examine.
     */
    void recordRequestBlockData(IronBee::ConstTransaction tx);

    /**
     * Sets block and action data at response time.
     *
     * - TxLogData::m_blockPhase
     * - TxLogData::m_blockAction
     * - TxLogData::m_blockMethod
     *
     * @param[in] tx The transaction to examine.
     */
    void recordResponseBlockData(IronBee::ConstTransaction tx);

    /**
     * Sets TxLogData::m_auditlogFile.
     *
     * @param[in] tx Transaction.
     * @param[in] auditlog The auditlog to examine.
     */
    void recordAuditLogData(
        IronBee::ConstTransaction tx,
        ib_auditlog_t*            auditlog);

private:

    //! Tracks action across the transaction phases.
    enum {
        TX_ACTION_PASSED,
        TX_ACTION_ALLOWED,
        TX_ACTION_BLOCKED
    } m_tx_action;

    //! The blocking phase or "".
    std::string m_blockPhase;

    //! The blocking method or "".
    std::string m_blockMethod;

    //! The blocking action or "".
    std::string m_blockAction;

    //! The name of the auditlog file or "".
    std::string m_auditlogFile;

    //! The audit log boundary. Consider this the audit log ID.
    std::string m_auditlogId;

    /**
     * Record data about blocking status.
     *
     * Any blocking action and method is recorded as happening during
     * the given phase.
     *
     * @param[in] tx Transaction.
     * @param[in] phase The phase which this is called.
     */
    void recordBlockData(
        IronBee::ConstTransaction tx,
        const std::string&        phase);

};

const std::string& TxLogData::blockPhase() const
{
    return m_blockPhase;
}

const std::string& TxLogData::blockMethod() const
{
    return m_blockMethod;
}

const std::string& TxLogData::blockAction() const
{
    return m_blockAction;
}

const std::string& TxLogData::auditlogId() const
{
    return m_auditlogId;
}

const std::string& TxLogData::auditlogFile() const
{
    return m_auditlogFile;
}

void TxLogData::recordBlockData(
    IronBee::ConstTransaction tx,
    const std::string&        phase
)
{
    /* NOTE: Request that is allowed, but then blocked in response
     * is still recorded as blocked. That is, blocking overrides
     * allowing when recording the action.
     */

    /* Already recorded earlier. */
    if (m_tx_action == TX_ACTION_BLOCKED) {
        return;
    }

    /* Record the action taken. */
    if ( (m_tx_action == TX_ACTION_PASSED) &&
         (tx.is_allow_request() || tx.is_allow_all()) )
    {
        m_tx_action = TX_ACTION_ALLOWED;
        m_blockPhase = phase;
        m_blockAction = "Allowed";
        m_blockMethod = "";
    }
    else if (tx.is_blocked())
    {
        m_tx_action = TX_ACTION_BLOCKED;
        m_blockPhase = phase;
        m_blockAction = "Blocked";

        switch(tx.block_method()) {
        case IB_BLOCK_METHOD_STATUS:
            m_blockMethod = "ErrorPage";
            break;
        case IB_BLOCK_METHOD_CLOSE:
            m_blockMethod = "Close";
            break;
        default:
            m_blockMethod = "";
        }
    }
}

void TxLogData::recordRequestBlockData(IronBee::ConstTransaction tx)
{
    /* Start with defaults. */
    m_tx_action = TX_ACTION_PASSED;
    m_blockPhase = "";
    m_blockAction = "";
    m_blockMethod = "";

    recordBlockData(tx, "Request");
}

void TxLogData::recordResponseBlockData(IronBee::ConstTransaction tx)
{
    recordBlockData(tx, "Response");
}

void TxLogData::recordAuditLogData(
    IronBee::ConstTransaction tx,
    ib_auditlog_t *auditlog
)
{
    m_auditlogFile = auditlog->cfg_data->full_path;
    m_auditlogId = tx.audit_log_id();
}

void eventsToJson(
    IronBee::ConstTransaction tx,
    TxLogJson& txLogJson
)
{
    IronBee::ConstList<ib_logevent_t *> ib_eventList(tx.ib()->logevents);

    txLogJson.withString("events");
    TxLogJsonArray<TxLogJson> events = txLogJson.withArray();

    BOOST_FOREACH(const ib_logevent_t *e, ib_eventList) {

        /* Skip suppressed events. */
        if (e->suppress != IB_LEVENT_SUPPRESS_NONE) {
            continue;
        }

        /* Open a map to the rendering of JSON. */
        TxLogJsonMap<TxLogJson> eventMap = txLogJson.withMap();

        /* Conditionally add the tags list. */
        if (e->tags && ib_list_elements(e->tags) > 0) {
            TxLogJsonArray<TxLogJsonMap<TxLogJson> > tags =
                eventMap.withArray("tags");
            IronBee::ConstList<const char *> ib_tagList(e->tags);
            BOOST_FOREACH(const char *tag, ib_tagList) {
                if (tag) {
                    tags.withString(tag);
                }
            }
            tags.close();
        }

        if (e->fields && ib_list_elements(e->fields) > 0) {
            TxLogJsonArray<TxLogJsonMap<TxLogJson> > locationList
                = eventMap.withArray("locations");
            IronBee::ConstList<const char *> ib_fieldList(e->fields);
            BOOST_FOREACH(const char *field, ib_fieldList) {
                locationList.withString(field);
            }
            locationList.close();
        }

        eventMap
            .withString("type", ib_logevent_type_name(e->type))
            .withString("rule", e->rule_id ? e->rule_id : "")
            .withString("message", e->msg ? e->msg : "")
            .withInt("confidence", e->confidence)
            .withInt("severity", e->severity)
            .withString("id", boost::lexical_cast<std::string>(e->event_id))
            .close();
    }

    events.close();
}

void requestHeadersToJson(
    IronBee::ConstTransaction tx,
    TxLogJson& txLogJson
)
{
    txLogJson.withString("headers");
    TxLogJsonArray<TxLogJson> headers = txLogJson.withArray();

    if (tx.request_header()) {
        for (
            IronBee::ConstParsedHeader headerNvp = tx.request_header();
            headerNvp;
            headerNvp = headerNvp.next()
        )
        {
            std::string headerName = headerNvp.name().to_s();
            std::string headerValue = headerNvp.value().to_s();

            if (boost::algorithm::iequals(headerName, "User-Agent") ||
                boost::algorithm::iequals(headerName, "Referer"))
            {
                headers.withMap()
                        .withString("name", headerNvp.name().to_s())
                        .withString("value", headerNvp.value().to_s())
                    .close();
            }
        }
    }

    headers.close();
}

void responseHeadersToJson(
    IronBee::ConstTransaction tx,
    TxLogJson& txLogJson
)
{
    txLogJson.withString("headers");
    TxLogJsonArray<TxLogJson> headers = txLogJson.withArray();

    if (tx.response_header()) {
        for (
            IronBee::ConstParsedHeader headerNvp = tx.response_header();
            headerNvp;
            headerNvp = headerNvp.next()
        )
        {
            std::string headerName = headerNvp.name().to_s();
            std::string headerValue = headerNvp.value().to_s();

            if (boost::algorithm::iequals(headerName, "Content-Type") ||
                boost::algorithm::iequals(headerName, "Server"))
            {
                headers.withMap()
                        .withString("name", headerNvp.name().to_s())
                        .withString("value", headerNvp.value().to_s())
                    .close();
            }
        }
    }

    headers.close();
}

/**
 * Renders @a name and then @a val if val is non-empty.
 *
 * If the length of @a val is 0, then nothing is done.
 *
 * This function is intended for use with calls to
 * TxLogJsonMap::withFunction() to render optional fields.
 *
 * @param[in] name The name of the value to render.
 * @param[in] val The value to render if val.length() > 0.
 * @param[in] txLogJson Used to render the values.
 */
void renderNonemptyString(
    const char*        name,
    const std::string& val,
    TxLogJson&         txLogJson
)
{
    if (val.length() > 0) {
        txLogJson.withString(name);
        txLogJson.withString(val);
    }
}

void addThreatLevel(
    IronBee::ConstContext     ctx,
    IronBee::ConstTransaction tx,
    TxLogJson&                txLogJson
)
{
    try
    {
        ib_core_cfg_t *ib_core_cfg;
        IronBee::throw_if_error(
            ib_core_context_config(ctx.ib(), &ib_core_cfg),
            "Failed to fetch core config.");

        IronBee::ConstField threat_level =
            IronBee::ConstVarSource(ib_core_cfg->vars->threat_level)
                .get(tx.var_store());

        // Add the threat level.
        txLogJson.withString("threatLevel");

        switch(threat_level.type()) {
        case IronBee::ConstField::NUMBER:
            txLogJson.withInt(threat_level.value_as_number());
            break;
        case IronBee::ConstField::FLOAT:
            txLogJson.withDouble(threat_level.value_as_float());
            break;
        case IronBee::ConstField::BYTE_STRING:
        case IronBee::ConstField::NULL_STRING:
            txLogJson.withString(threat_level.to_s());
            break;
        default:
            BOOST_THROW_EXCEPTION(
                IronBee::einval()
                    << IronBee::errinfo_what(
                        "Unsupported type for THREAT_LEVEL. "
                        "It must be a number or a string."));
        }
    }
    catch (const IronBee::enoent&) {
        /* Nop. */
    }
}
} /* Anonymous namespace. */

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
 * @param[in] log_msg Unused.
 * @param[in] log_msg_sz The length of @a log_msg.
 * @param[out] writer_record A @ref ib_logger_standard_msg_t if this returns
 *             IB_OK. Unset otherwise. This must be a `ib_logger_std_msg_t **`.
 * @param[in] data Callback data. Unused.
 *
 * @returns
 * - IB_OK On success.
 * - IB_DECLINE If the ib_logger_rec_t::type of @a rec is not
 *   @ref IB_LOGGER_TXLOG_TYPE.
 * - Other on error.
 */
static ib_status_t txlog_logger_format_fn(
    ib_logger_t           *logger,
    const ib_logger_rec_t *rec,
    const uint8_t         *log_msg,
    const size_t           log_msg_sz,
    void                  *writer_record,
    void                  *data
)
{
    assert(rec);

    /* Do not handle non-tx recs. */
    if (rec->tx == NULL || rec->type != IB_LOGGER_TXLOG_TYPE) {
        return IB_DECLINED;
    }

    assert(rec->tx->ib);
    assert(rec->tx);
    assert(rec->tx->mp);
    assert(rec->tx->ctx);
    assert(logger != NULL);
    assert(rec != NULL);

    /* Wrap some types into IronBee++. */
    IronBee::ConstTransaction tx(rec->tx);
    IronBee::ConstContext     ctx(rec->tx->ctx);
    IronBee::ConstConnection  conn(rec->conn);
    IronBee::ConstModule      module(rec->module);

    const std::string siteId =
        (! tx.context() || ! tx.context().site())?
            "" : tx.context().site().id();

    /* Fetch some telemetry from our tx. */
    TxLogData &txlogdata = IronBee::Transaction::remove_const(tx)
        .get_module_data<TxLogData&>(module);

    ib_logger_standard_msg_t *stdmsg;

    /* Build stdmsg and return it. */
    stdmsg =
        reinterpret_cast<ib_logger_standard_msg_t *>(
            malloc(sizeof(*stdmsg)));
    if (stdmsg == NULL) {
        return IB_EALLOC;
    }

    try {
        TxLogJson()
            .withMap()
                .withTime("timestamp", tx.started_time())
                .withInt("duration",
                    (tx.finished_time() - tx.started_time())
                        .total_milliseconds())
                .withString("id", tx.id())
                .withString("clientIp", tx.effective_remote_ip_string())
                .withInt("clientPort", conn.remote_port())
                .withString("sensorId", tx.engine().sensor_id())
                .withString("siteId", siteId)
                .withMap("connection")
                    .withString("id", conn.id())
                    .withString("clientIp", conn.remote_ip_string())
                    .withInt("clientPort", conn.remote_port())
                    .withString("serverIp", conn.local_ip_string())
                    .withInt("serverPort", conn.local_port())
                .close()
                .withMap("request")
                    .withString("method", tx.request_line().method().to_s())
                    .withString("uri", tx.request_line().uri().to_s())
                    .withString("protocol", tx.request_line().protocol().to_s())
                    .withString("host", tx.hostname())
                    .withInt("bandwidth", 0)
                    .withFunction(boost::bind(requestHeadersToJson, tx, _1))
                .close()
                .withMap("response")
                    .withString("protocol", tx.response_line().protocol().to_s())
                    .withString("status", tx.response_line().status().to_s())
                    .withString("message", tx.response_line().message().to_s())
                    .withInt("bandwidth", 0)
                    .withFunction(boost::bind(responseHeadersToJson, tx, _1))
                .close()
                .withMap("security")
                    .withFunction(
                        boost::bind(
                            renderNonemptyString,
                            "auditLogRef",
                            boost::ref(txlogdata.auditlogId()),
                            _1))
                    .withFunction(boost::bind(addThreatLevel, ctx, tx, _1))
                    .withFunction(boost::bind(eventsToJson, tx, _1))
                    .withFunction(
                        boost::bind(
                            renderNonemptyString,
                            "action",
                            boost::ref(txlogdata.blockAction()),
                            _1))
                    .withFunction(
                        boost::bind(
                            renderNonemptyString,
                            "actionMethod",
                            boost::ref(txlogdata.blockMethod()),
                            _1))
                    .withFunction(
                        boost::bind(
                            renderNonemptyString,
                            "actionPhase",
                            boost::ref(txlogdata.blockPhase()),
                            _1))
                .close()
            .close()
            .render(
                reinterpret_cast<char*&>(stdmsg->msg),
                stdmsg->msg_sz
            );
    }
    catch (...) {
        return IronBee::convert_exception(IronBee::ConstEngine(rec->tx->ib));
    }

    stdmsg->prefix = NULL;
    *reinterpret_cast<void **>(writer_record) = stdmsg;

    return IB_OK;
}

static void log_to_engine(void *element, void *cbdata) {
    assert(element != NULL);
    assert(cbdata != NULL);

    ib_logger_standard_msg_t *msg =
        reinterpret_cast<ib_logger_standard_msg_t *>(element);
    ib_engine_t *ib =
        reinterpret_cast<ib_engine_t *>(cbdata);

    ib_log_info(
        ib,
        "%.*s",
        static_cast<int>(msg->msg_sz),
        reinterpret_cast<char *>(msg->msg));
}

static ib_status_t txlog_log_to_engine(
    ib_logger_t        *logger,
    ib_logger_writer_t *writer,
    void               *cbdata
)
{
    return ib_logger_dequeue(logger, writer, &log_to_engine, cbdata);
}

} /* extern "C" */

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

    //! Container for C callback and callback data.
    std::pair<ib_core_auditlog_fn_t, void *> m_recordAuditLogInfoTrampoline;

    //! Object that destroys m_recordAuditLogInfoTrampoline.second, void *.
    boost::shared_ptr<void> m_recordAuditLogInfoTrampolinePtr;

    //! Enable/Disable directive callback.
    void onOffDirective(
        IronBee::ConfigurationParser  cp,
        bool                          enabled
    ) const;

    void logToStdLogDirective(IronBee::ConfigurationParser cp) const;

    //! Callback to log @a tx through the Logger of @a ib.
    void transactionFinishedHandler(
        IronBee::Engine      ib,
        IronBee::Transaction tx
    ) const;

    //! Callback to log @a tx through the Logger of @a ib.
    void transactionStartedHandler(
        IronBee::Engine      ib,
        IronBee::Transaction tx
    ) const;

    //! Callback that collects information about a request so as to log it.
    void handleRequest(
        IronBee::Engine      ib,
        IronBee::Transaction tx
    ) const;

    //! Callback that collects information about a response so as to log it.
    void handleResponse(
        IronBee::Engine      ib,
        IronBee::Transaction tx
    ) const;

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
        ib_auditlog_t             *auditlog
    ) const;
};

IBPP_BOOTSTRAP_MODULE_DELEGATE(TXLOG_MODULE_NAME, TxLogModule);

//! C++ify the C configuration struct.
struct TxLogConfig
{
    //! Logging enabled for this context?
    bool is_enabled;


    //! Constructor.
    TxLogConfig();
};

/**
 * Setup some good defaults.
 */
TxLogConfig::TxLogConfig():
    is_enabled(true)
{}

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
    ib_logger_format_t *format;

    IronBee::throw_if_error(
        ib_logger_format_create(
            ib_engine_logger_get(module.engine().ib()),
            &format,
            txlog_logger_format_fn,
            NULL,
            ib_logger_standard_msg_free,
            NULL));

    /* Register the TxLog logger format function. */
    IronBee::throw_if_error(
        ib_logger_register_format(
            ib_engine_logger_get(module.engine().ib()),
            TXLOG_FORMAT_FN_NAME,
            format));

    /* Set the default configuration. */
    module.set_configuration_data(TxLogConfig());

    /* Register configuration directives. */
    module.engine().register_configuration_directives()
        .list(
            "TxLogIronBeeLog",
            boost::bind(&TxLogModule::logToStdLogDirective, this, _1))
        .on_off(
            "TxLogEnabled",
            boost::bind(&TxLogModule::onOffDirective, this, _1, _3))
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
        bool                          enabled
) const
{
    TxLogConfig &cfg =
        module().configuration_data<TxLogConfig>(cp.current_context());

    /* Set the mapping in the context configuration. */
    cfg.is_enabled = enabled;
}

void TxLogModule::logToStdLogDirective(IronBee::ConfigurationParser cp) const {

    ib_logger_format_t *format;

    IronBee::throw_if_error(
        ib_logger_fetch_format(
            ib_engine_logger_get(cp.engine().ib()),
            TXLOG_FORMAT_FN_NAME,
            &format)
    );

    IronBee::throw_if_error(
        ib_logger_writer_add(
            ib_engine_logger_get(cp.engine().ib()),
            NULL, NULL,
            NULL, NULL,
            NULL, NULL,
            format,
            txlog_log_to_engine, cp.engine().ib())
    );
}


ib_status_t TxLogModule::recordAuditLogInfo(
    ib_engine_t               *ib,
    ib_tx_t                   *ib_tx,
    ib_core_auditlog_event_en  event,
    ib_auditlog_t             *auditlog
) const
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
) const
{
    tx.set_module_data(module(), TxLogData());
}

void TxLogModule::handleRequest(
    IronBee::Engine      ib,
    IronBee::Transaction tx
) const
{
    TxLogData &data = tx.get_module_data<TxLogData&>(module());

    data.recordRequestBlockData(tx);
}

void TxLogModule::handleResponse(
    IronBee::Engine      ib,
    IronBee::Transaction tx
) const
{
    TxLogData &data = tx.get_module_data<TxLogData&>(module());

    data.recordResponseBlockData(tx);
}

void TxLogModule::transactionFinishedHandler(
    IronBee::Engine      ib,
    IronBee::Transaction tx
) const
{
    assert(ib);
    assert(tx);

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
