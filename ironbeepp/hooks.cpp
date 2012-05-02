#include <ironbeepp/hooks.hpp>
#include <ironbeepp/connection.hpp>
#include <ironbeepp/connection_data.hpp>
#include <ironbeepp/transaction.hpp>
#include <ironbeepp/transaction_data.hpp>
#include <ironbeepp/parsed_name_value.hpp>
#include <ironbeepp/parsed_request_line.hpp>
#include <ironbeepp/parsed_response_line.hpp>
#include <ironbeepp/internal/catch.hpp>
#include <ironbeepp/internal/throw.hpp>
#include <ironbeepp/internal/data.hpp>

#include <ironbee/debug.h>

namespace IronBee {

namespace Internal {
namespace {
namespace Hooks {

/**
 * Hooks handler for null callbacks.
 *
 * @param[in] ib_engine The IronBee engine.
 * @param[in] event     Which event happened.
 * @param[in] cbdata    Callback data: contains C++ functional to forward to.
 * @returns Status code reflecting any exceptions thrown.
 **/
ib_status_t null(
    ib_engine_t*          ib_engine,
    ib_state_event_type_t event,
    void*                 cbdata
)
{
    IB_FTRACE_INIT();

    assert(ib_engine != NULL);
    assert(cbdata != NULL);

    IB_FTRACE_RET_STATUS(IBPP_TRY_CATCH(ib_engine,
        Internal::data_to_value<HooksRegistrar::null_t>(cbdata)(
            Engine(ib_engine),
            static_cast<Engine::state_event_e>(event)
        )
    ));
}

/**
 * Hooks handler for headers_data callbacks.
 *
 * @param[in] ib_engine  The IronBee engine.
 * @param[in] ib_tx      Transaction.
 * @param[in] event      Which event happened.
 * @param[in] ib_headers Data of event.
 * @param[in] cbdata     Callback data: contains C++ functional to forward to.
 * @returns Status code reflecting any exceptions thrown.
 **/
ib_status_t headers_data(
    ib_engine_t*          ib_engine,
    ib_tx_t*              ib_tx,
    ib_state_event_type_t event,
    ib_parsed_header_t*   ib_headers,
    void*                 cbdata
)
{
    IB_FTRACE_INIT();

    assert(ib_engine != NULL);
    /* ib_tx may be NULL */
    assert(ib_headers != NULL);
    assert(cbdata != NULL);

    IB_FTRACE_RET_STATUS(IBPP_TRY_CATCH(ib_engine,
        Internal::data_to_value<HooksRegistrar::headers_data_t>(cbdata)(
            Engine(ib_engine),
            Transaction(ib_tx),
            static_cast<Engine::state_event_e>(event),
            ParsedNameValue(ib_headers)
        )
    ));
}

/**
 * Hooks handler for request_line callbacks.
 *
 * @param[in] ib_engine       The IronBee engine.
 * @param[in] ib_tx           Transaction.
 * @param[in] event           Which event happened.
 * @param[in] ib_request_line Data of event.
 * @param[in] cbdata          Callback data: contains C++ functional to
 *                            forward to.
 * @returns Status code reflecting any exceptions thrown.
 **/
ib_status_t request_line(
    ib_engine_t*          ib_engine,
    ib_tx_t*              ib_tx,
    ib_state_event_type_t event,
    ib_parsed_req_line_t* ib_request_line,
    void*                 cbdata
)
{
    IB_FTRACE_INIT();

    assert(ib_engine != NULL);
    /* ib_tx may be NULL */
    assert(ib_request_line != NULL);
    assert(cbdata != NULL);

    IB_FTRACE_RET_STATUS(IBPP_TRY_CATCH(ib_engine,
        Internal::data_to_value<HooksRegistrar::request_line_t>(cbdata)(
            Engine(ib_engine),
            Transaction(ib_tx),
            static_cast<Engine::state_event_e>(event),
            ParsedRequestLine(ib_request_line)
        )
    ));
}

/**
 * Hooks handler for response_line callbacks.
 *
 * @param[in] ib_engine        The IronBee engine.
 * @param[in] ib_tx            Transaction.
 * @param[in] event            Which event happened.
 * @param[in] ib_response_line Data of event.
 * @param[in] cbdata           Callback data: contains C++ functional to
 *                             forward to.
 * @returns Status code reflecting any exceptions thrown.
 **/
ib_status_t response_line(
    ib_engine_t*           ib_engine,
    ib_tx_t*               ib_tx,
    ib_state_event_type_t  event,
    ib_parsed_resp_line_t* ib_response_line,
    void*                  cbdata
)
{
    IB_FTRACE_INIT();

    assert(ib_engine != NULL);
    /* ib_tx may be NULL. */
    assert(ib_response_line != NULL);
    assert(cbdata != NULL);

    IB_FTRACE_RET_STATUS(IBPP_TRY_CATCH(ib_engine,
        Internal::data_to_value<HooksRegistrar::response_line_t>(cbdata)(
            Engine(ib_engine),
            Transaction(ib_tx),
            static_cast<Engine::state_event_e>(event),
            ParsedResponseLine(ib_response_line)
        )
    ));
}

/**
 * Hooks handler for connection callbacks.
 *
 * @param[in] ib_engine     The IronBee engine.
 * @param[in] event         Which event happened.
 * @param[in] ib_connection Data of event.
 * @param[in] cbdata        Callback data: contains C++ functional to forward
 *                          to.
 * @returns Status code reflecting any exceptions thrown.
 **/
ib_status_t connection(
    ib_engine_t*          ib_engine,
    ib_state_event_type_t event,
    ib_conn_t*            ib_connection,
    void*                 cbdata
)
{
    IB_FTRACE_INIT();

    assert(ib_engine != NULL);
    assert(ib_connection != NULL);
    assert(cbdata != NULL);

    IB_FTRACE_RET_STATUS(IBPP_TRY_CATCH(ib_engine,
        Internal::data_to_value<HooksRegistrar::connection_t>(cbdata)(
            Engine(ib_engine),
            static_cast<Engine::state_event_e>(event),
            Connection(ib_connection)
        )
    ));
}

/**
 * Hooks handler for connection_data callbacks.
 *
 * @param[in] ib_engine          The IronBee engine.
 * @param[in] event              Which event happened.
 * @param[in] ib_connection_data Data of event.
 * @param[in] cbdata             Callback data: contains C++ functional to
 *                               forward to.
 * @returns Status code reflecting any exceptions thrown.
 **/
ib_status_t connection_data(
    ib_engine_t*          ib_engine,
    ib_state_event_type_t event,
    ib_conndata_t*        ib_connection_data,
    void*                 cbdata
)
{
    IB_FTRACE_INIT();

    assert(ib_engine != NULL);
    assert(ib_connection_data != NULL);
    assert(cbdata != NULL);

    IB_FTRACE_RET_STATUS(IBPP_TRY_CATCH(ib_engine,
        Internal::data_to_value<HooksRegistrar::connection_data_t>(cbdata)(
            Engine(ib_engine),
            static_cast<Engine::state_event_e>(event),
            ConnectionData(ib_connection_data)
        )
    ));
}

/**
 * Hooks handler for transaction callbacks.
 *
 * @param[in] ib_engine      The IronBee engine.
 * @param[in] ib_transaction Data of event.
 * @param[in] event          Which event happened.
 * @param[in] cbdata         Callback data: contains C++ functional to
 *                           forward to.
 * @returns Status code reflecting any exceptions thrown.
 **/
ib_status_t transaction(
    ib_engine_t*          ib_engine,
    ib_tx_t*              ib_transaction,
    ib_state_event_type_t event,
    void*                 cbdata
)
{
    IB_FTRACE_INIT();

    assert(ib_engine != NULL);
    assert(ib_transaction != NULL);
    assert(cbdata != NULL);

    IB_FTRACE_RET_STATUS(IBPP_TRY_CATCH(ib_engine,
        Internal::data_to_value<HooksRegistrar::transaction_t>(cbdata)(
            Engine(ib_engine),
            Transaction(ib_transaction),
            static_cast<Engine::state_event_e>(event)
        )
    ));
}

/**
 * Hooks handler for transaction_data callbacks.
 *
 * @param[in] ib_engine           The IronBee engine.
 * @param[in] ib_tx               Current transaction.
 * @param[in] event               Which event happened.
 * @param[in] ib_transaction_data Data of event.
 * @param[in] cbdata              Callback data: contains C++ functional to
 *                                forward to.
 * @returns Status code reflecting any exceptions thrown.
 **/
ib_status_t transaction_data(
    ib_engine_t*          ib_engine,
    ib_tx_t*              ib_tx,
    ib_state_event_type_t event,
    ib_txdata_t*          ib_transaction_data,
    void*                 cbdata
)
{
    IB_FTRACE_INIT();

    assert(ib_engine != NULL);
    assert(ib_tx != NULL);
    assert(ib_transaction_data != NULL);
    assert(cbdata != NULL);

    IB_FTRACE_RET_STATUS(IBPP_TRY_CATCH(ib_engine,
        Internal::data_to_value<HooksRegistrar::transaction_data_t>(cbdata)(
            Engine(ib_engine),
            Transaction(ib_tx),
            static_cast<Engine::state_event_e>(event),
            TransactionData(ib_transaction_data)
        )
    ));
}

} // Hooks
}
} // Internal

HooksRegistrar::HooksRegistrar(Engine engine) :
    m_engine(engine)
{
    // nop
}

HooksRegistrar& HooksRegistrar::null(
    Engine::state_event_e event,
    null_t                f
)
{
    if (f.empty()) {
        BOOST_THROW_EXCEPTION(einval() << errinfo_what(
            "Empty functional passed to hook registrarion."
        ));
    }

    Internal::throw_if_error(
        ib_hook_null_register(
            m_engine.ib(),
            static_cast<ib_state_event_type_t>(event),
            &Internal::Hooks::null,
            Internal::value_to_data<null_t>(
                f,
                m_engine.main_memory_pool().ib()
            )
        )
    );

    return *this;
}

HooksRegistrar& HooksRegistrar::headers_data(
    Engine::state_event_e event,
    headers_data_t        f
)
{
    if (f.empty()) {
        BOOST_THROW_EXCEPTION(einval() << errinfo_what(
            "Empty functional passed to hook registrarion."
        ));
    }

    Internal::throw_if_error(
        ib_hook_parsed_header_data_register(
            m_engine.ib(),
            static_cast<ib_state_event_type_t>(event),
            &Internal::Hooks::headers_data,
            Internal::value_to_data<headers_data_t>(
                f,
                m_engine.main_memory_pool().ib()
            )
        )
    );

    return *this;
}

HooksRegistrar& HooksRegistrar::request_line(
    Engine::state_event_e event,
    request_line_t        f
)
{
    if (f.empty()) {
        BOOST_THROW_EXCEPTION(einval() << errinfo_what(
            "Empty functional passed to hook registrarion."
        ));
    }

    Internal::throw_if_error(
        ib_hook_parsed_req_line_register(
            m_engine.ib(),
            static_cast<ib_state_event_type_t>(event),
            &Internal::Hooks::request_line,
            Internal::value_to_data<request_line_t>(
                f,
                m_engine.main_memory_pool().ib()
            )
        )
    );

    return *this;
}

HooksRegistrar& HooksRegistrar::response_line(
    Engine::state_event_e event,
    response_line_t       f
)
{
    if (f.empty()) {
        BOOST_THROW_EXCEPTION(einval() << errinfo_what(
            "Empty functional passed to hook registrarion."
        ));
    }

    Internal::throw_if_error(
        ib_hook_parsed_resp_line_register(
            m_engine.ib(),
            static_cast<ib_state_event_type_t>(event),
            &Internal::Hooks::response_line,
            Internal::value_to_data<response_line_t>(
                f,
                m_engine.main_memory_pool().ib()
            )
        )
    );

    return *this;
}

HooksRegistrar& HooksRegistrar::connection(
    Engine::state_event_e event,
    connection_t          f
)
{
    if (f.empty()) {
        BOOST_THROW_EXCEPTION(einval() << errinfo_what(
            "Empty functional passed to hook registrarion."
        ));
    }

    Internal::throw_if_error(
        ib_hook_conn_register(
            m_engine.ib(),
            static_cast<ib_state_event_type_t>(event),
            &Internal::Hooks::connection,
            Internal::value_to_data<connection_t>(
                f,
                m_engine.main_memory_pool().ib()
            )
        )
    );

    return *this;
}

HooksRegistrar& HooksRegistrar::connection_data(
    Engine::state_event_e event,
    connection_data_t     f
)
{
    if (f.empty()) {
        BOOST_THROW_EXCEPTION(einval() << errinfo_what(
            "Empty functional passed to hook registrarion."
        ));
    }

    Internal::throw_if_error(
        ib_hook_conndata_register(
            m_engine.ib(),
            static_cast<ib_state_event_type_t>(event),
            &Internal::Hooks::connection_data,
            Internal::value_to_data<connection_data_t>(
                f,
                m_engine.main_memory_pool().ib()
            )
        )
    );

    return *this;
}

HooksRegistrar& HooksRegistrar::transaction(
    Engine::state_event_e event,
    transaction_t         f
)
{
    if (f.empty()) {
        BOOST_THROW_EXCEPTION(einval() << errinfo_what(
            "Empty functional passed to hook registrarion."
        ));
    }

    Internal::throw_if_error(
        ib_hook_tx_register(
            m_engine.ib(),
            static_cast<ib_state_event_type_t>(event),
            &Internal::Hooks::transaction,
            Internal::value_to_data<transaction_t>(
                f,
                m_engine.main_memory_pool().ib()
            )
        )
    );

    return *this;
}

HooksRegistrar& HooksRegistrar::transaction_data(
    Engine::state_event_e event,
    transaction_data_t    f
)
{
    if (f.empty()) {
        BOOST_THROW_EXCEPTION(einval() << errinfo_what(
            "Empty functional passed to hook registrarion."
        ));
    }

    Internal::throw_if_error(
        ib_hook_txdata_register(
            m_engine.ib(),
            static_cast<ib_state_event_type_t>(event),
            &Internal::Hooks::transaction_data,
            Internal::value_to_data<transaction_data_t>(
                f,
                m_engine.main_memory_pool().ib()
            )
        )
    );

    return *this;
}

HooksRegistrar& HooksRegistrar::configuration_started(null_t f)
{
    return null(
        Engine::configuration_started,
        f
    );
}

HooksRegistrar& HooksRegistrar::configuration_finished(null_t f)
{
    return null(
        Engine::configuration_finished,
        f
    );
}

HooksRegistrar& HooksRegistrar::request_headers_data(headers_data_t f)
{
    return headers_data(
        Engine::request_headers_data,
        f
    );
}

HooksRegistrar& HooksRegistrar::response_headers_data(headers_data_t f)
{
    return headers_data(
        Engine::response_headers_data,
        f
    );
}

HooksRegistrar& HooksRegistrar::request_started(request_line_t f)
{
    return request_line(
        Engine::request_started,
        f
    );
}

HooksRegistrar& HooksRegistrar::response_started(response_line_t f)
{
    return response_line(
        Engine::response_started,
        f
    );
}

HooksRegistrar& HooksRegistrar::connection_started(connection_t f)
{
    return connection(
        Engine::connection_started,
        f
    );
}

HooksRegistrar& HooksRegistrar::connection_finished(connection_t f)
{
    return connection(
        Engine::connection_finished,
        f
    );
}

HooksRegistrar& HooksRegistrar::connection_opened(connection_t f)
{
    return connection(
        Engine::connection_opened,
        f
    );
}

HooksRegistrar& HooksRegistrar::connection_closed(connection_t f)
{
    return connection(
        Engine::connection_closed,
        f
    );
}

HooksRegistrar& HooksRegistrar::handle_context_connection(connection_t f)
{
    return connection(
        Engine::handle_context_connection,
        f
    );
}

HooksRegistrar& HooksRegistrar::handle_connect(connection_t f)
{
    return connection(
        Engine::handle_connect,
        f
    );
}

HooksRegistrar& HooksRegistrar::handle_disconnect(connection_t f)
{
    return connection(
        Engine::handle_disconnect,
        f
    );
}

HooksRegistrar& HooksRegistrar::connection_data_in(connection_data_t f)
{
    return connection_data(
        Engine::connection_data_in,
        f
    );
}

HooksRegistrar& HooksRegistrar::connection_data_out(connection_data_t f)
{
    return connection_data(
        Engine::connection_data_out,
        f
    );
}

HooksRegistrar& HooksRegistrar::transaction_started(transaction_t f)
{
    return transaction(
        Engine::transaction_started,
        f
    );
}

HooksRegistrar& HooksRegistrar::transaction_process(transaction_t f)
{
    return transaction(
        Engine::transaction_process,
        f
    );
}

HooksRegistrar& HooksRegistrar::transaction_finished(transaction_t f)
{
    return transaction(
        Engine::transaction_finished,
        f
    );
}

HooksRegistrar& HooksRegistrar::handle_context_transaction(transaction_t f)
{
    return transaction(
        Engine::handle_context_transaction,
        f
    );
}

HooksRegistrar& HooksRegistrar::handle_request_headers(transaction_t f)
{
    return transaction(
        Engine::handle_request_headers,
        f
    );
}

HooksRegistrar& HooksRegistrar::handle_request(transaction_t f)
{
    return transaction(
        Engine::handle_request,
        f
    );
}

HooksRegistrar& HooksRegistrar::handle_response_headers(transaction_t f)
{
    return transaction(
        Engine::handle_response_headers,
        f
    );
}

HooksRegistrar& HooksRegistrar::handle_response(transaction_t f)
{
    return transaction(
        Engine::handle_response,
        f
    );
}

HooksRegistrar& HooksRegistrar::handle_postprocess(transaction_t f)
{
    return transaction(
        Engine::handle_postprocess,
        f
    );
}

HooksRegistrar& HooksRegistrar::request_headers(transaction_t f)
{
    return transaction(
        Engine::request_headers,
        f
    );
}

HooksRegistrar& HooksRegistrar::request_finished(transaction_t f)
{
    return transaction(
        Engine::request_finished,
        f
    );
}

HooksRegistrar& HooksRegistrar::response_headers(transaction_t f)
{
    return transaction(
        Engine::response_headers,
        f
    );
}

HooksRegistrar& HooksRegistrar::response_finished(transaction_t f)
{
    return transaction(
        Engine::response_finished,
        f
    );
}

HooksRegistrar& HooksRegistrar::transaction_data_in(transaction_data_t f)
{
    return transaction_data(
        Engine::transaction_data_in,
        f
    );
}

HooksRegistrar& HooksRegistrar::transaction_data_out(transaction_data_t f)
{
    return transaction_data(
        Engine::transaction_data_out,
        f
    );
}

HooksRegistrar& HooksRegistrar::request_body_data(transaction_data_t f)
{
    return transaction_data(
        Engine::request_body_data,
        f
    );
}

HooksRegistrar& HooksRegistrar::response_body_data(transaction_data_t f)
{
    return transaction_data(
        Engine::response_body_data,
        f
    );
}

} // IronBee
