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
 * @brief IronBee --- WriteClipp Module
 *
 * Module to write traffic in the CLIPP PB format.
 *
 * Adds two actions:
 *
 * - `write_clipp_tx:`*path* -- Append a connection with the current
 *   transaction to *path* in CLIPP format.
 * - `write_clipp_conn:`*path* -- As `write_clipp_tx`, but also writes all
 *   subsequent transaction in connection.  Note: Does not write earlier
 *   transactions.
 *
 * In both cases, *path* may be empty in which case output is written to
 * cerr.
 *
 * If multiple actions fire, the last overrides any former.
 *
 * @warning These actions can have a significant performance impact in both
 *          time and space.
 *
 * @note Either action must be fired before IB_PHASE_LOGGING in order to
 *       capture the current transaction.  They must be fired the body
 *       phases in order to capture the body.  Otherwise, it does not
 *       matter which phase they are fired in; non-body writing and all file
 *       I/O takes place in the logging phase.
 *
 * @note The resulting PB is a reproduction of the traffic, not an
 *       accurate reproduction of the event stream.  Differences include
 *       a lack of timing information, a lack of multiple header/body
 *       events, and only including body up to the buffering limit.  These
 *       limitations reflect an requirement at being able to log the
 *       current transaction if a rule fires for it.  Such a requirement
 *       requires reconstructing an event sequence.  A future module may
 *       provide less control and higher fidelity.
 *
 * @cond Internal
 *
 * This module reuses the CLIPP PBConsumer code which writes CLIPP Inputs out
 * in PB format one Input at a time.  In order to use it, we need a complete
 * Input before we write anything, which means, for `write_clipp_conn`,
 * writing at the end of the connection.  As transaction data does not live
 * past the end of the transaction, we make copies of the relevant data
 * using the connection memory pool.
 *
 * There are more memory efficient alternatives including:
 *
 * - Not copying data for `write_clipp_tx`.
 * - Using a custom PB writer that could stream out one transaction at a time.
 *
 * All of these are possible but add code complexity.  The current code
 * focuses on simplicity instead of memory efficiency.
 *
 * @endcond
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <clipp/pb_consumer.hpp>

#include <ironbeepp/all.hpp>

#include <ironbee/rule_engine.h>

using namespace std;
using namespace IronBee;

namespace {

//! Name of action to write current transaction.
static const char* c_tx_action = "write_clipp_tx";
//! Name of action to write rest of connection.
static const char* c_conn_action = "write_clipp_conn";

/**
 * Per connection data.
 *
 * This is set by either action and used by the event handlers to determined
 * what to do.
 **/
struct per_connection_t
{
    //! Constructor.
    per_connection_t() :
        active(true), all_tx(false), to("")
    {
        // nop
    }

    //! If true, doing something.
    bool active;

    //! If true, write all remaining tx.
    bool all_tx;

    //! Input being built.  Singular means start a new one.
    CLIPP::Input::input_p input;

    //! Where to write at connection close.
    const char* to;
};

//! Pointer to @ref per_connection_t; stored in connection module data.
typedef boost::shared_ptr<per_connection_t> per_connection_p;

//! Delegate
class Delegate :
    public IronBee::ModuleDelegate
{
public:
    //! Constructor.
    explicit
    Delegate(IronBee::Module module);

private:

    /**
     * Action generator.
     *
     * @param[in] all_tx    Whether action applies to a single transaction
     *                      (false) or the rest of the connection (true).
     * @param[in] mm        Memory manager.
     * @param[in] to        The `to` parameter from action_create().
     * @return Action instance.
     **/
    Action::action_instance_t action_generator(
        bool all_tx,
        MemoryManager mm,
        const char* to
    ) const;

    /**
     * Execute either action as determined by @a all_tx.
     *
     * Sets up connection data appropriately.  All actual work is done in
     * on_logging() and on_connection_close().
     *
     * @param[in] to        The `to` parameter from action_generator().
     * @param[in] all_tx    Whether action applies to a single transaction
     *                      (false) or the rest of the connection (true).
     * @param[in] rule_exec Rule execution environment.  Determines connection
     *                      and transaction.
     **/
    void action_execute(
        const char*           to,
        bool                  all_tx,
        const ib_rule_exec_t* rule_exec
    ) const;

    /**
     * Logging event: store current transaction.
     *
     * Looks for what to do in current connection data.  If active, marshals
     * the current transaction.  If in single transaction module, writes out
     * the input.  Otherwise, appends to it.
     *
     * @param[in] tx Current transaction.
     **/
    void on_logging(Transaction tx) const;

    /**
     * Connection close event: finish input.
     *
     * If active, writes out the current input.
     *
     * @param[in] connection Current connection.
     **/
    void on_connection_close(Connection connection) const;
};

/**
 * Begin a new input based on @a connection.
 *
 * @param[in] connection Connection to use for connection opened event.
 * @return New input with connection opened event only.
 **/
CLIPP::Input::input_p start_input(Connection connection);

/**
 * Finish an input.
 *
 * @param[in] input Input to write out.
 * @param[in] to    Where to write it.  Empty string means cerr.
 **/
void finish_input(
    const CLIPP::Input::input_p& input,
    const char*                  to
);

/**
 * Add headers starting with @a first to @a e.
 *
 * @param[in] mm    MemoryManager to allocate from.
 * @param[in] e     HeaderEvent to add headers to.
 * @param[in] first Beginning of list of headers.
 **/
void add_headers(
    MemoryManager              mm,
    CLIPP::Input::HeaderEvent& e,
    ConstParsedHeader          first
);

/**
 * Add transaction @a tx to @a input.
 *
 * @param[in] input Input to add transaction to.
 * @param[in] tx    Transaction to add.
 **/
void add_transaction(
    const CLIPP::Input::input_p& input,
    ConstTransaction             tx
);

/**
 * Convert ConstTransaction to Buffer.
 *
 * @param[in] mm Memory manager to allocate copy from.
 * @param[in] bs Bytestring.
 * @returns Buffer referring to copy of @a bs.
 **/
CLIPP::Input::Buffer bs_to_buf(
    MemoryManager   mm,
    ConstByteString bs
);

/**
 * Convert nul string to buffer.
 *
 * @param[in] mm Memory manager to allocate copy from.
 * @param[in] s  String.
 * @returns Buffer referring to copy of @a s.
 **/
CLIPP::Input::Buffer s_to_buf(
    MemoryManager mm,
    const char*   s
);

/**
 * Convert stream to buffer.
 *
 * @param[in] mm     Memory manager to allocate copy from.
 * @param[in] stream Stream to convert.
 * @returns Buffer referring to assembled copy of stream.
 **/
CLIPP::Input::Buffer stream_to_buf(
    MemoryManager      mm,
    const ib_stream_t* stream
);

} // Anonymous

IBPP_BOOTSTRAP_MODULE_DELEGATE("constant", Delegate)

// Implementation

// Reopen for doxygen; not needed by C++.
namespace {

Delegate::Delegate(IronBee::Module module) :
    IronBee::ModuleDelegate(module)
{
    using boost::bind;

    MemoryManager mm = module.engine().main_memory_mm();

    module.engine().register_hooks()
        .handle_logging(
            boost::bind(&Delegate::on_logging, this, _2)
        )
        .connection_closed(
            boost::bind(&Delegate::on_connection_close, this, _2)
        )
        ;

    Action::create(
        mm,
        c_tx_action,
        bind(&Delegate::action_generator, this, false, _1, _3)
    ).register_with(module.engine());

    Action::create(
        mm,
        c_conn_action,
        bind(&Delegate::action_generator, this, true, _1, _3)
    ).register_with(module.engine());
}

Action::action_instance_t Delegate::action_generator(
    bool          all_tx,
    MemoryManager mm,
    const char*   to
) const
{
    assert(to);

    return bind(
        &Delegate::action_execute,
        this, mm.strdup(to), all_tx, _1
    );
}

void Delegate::action_execute(
    const char*           to,
    bool                  all_tx,
    const ib_rule_exec_t* rule_exec
) const
{
    assert(rule_exec);

    Connection conn(rule_exec->tx->conn);
    per_connection_p per_connection;
    try {
        per_connection = conn.get_module_data<per_connection_p>(module());
    }
    catch (enoent) {
        per_connection.reset(new per_connection_t());
        conn.set_module_data(module(), per_connection);
    }
    per_connection->to = to;
    per_connection->all_tx = all_tx;
    if (! per_connection->all_tx) {
        // Possible that an earlier conn action has been overridden by
        // a tx action, in which case drop any earlier transactions
        // the conn action is storing.
        per_connection->input.reset();
    }
}

void Delegate::on_logging(Transaction tx) const
{
    per_connection_p per_connection;
    try {
        per_connection = tx.connection().get_module_data<per_connection_p>(module());
    }
    catch (enoent) {
        // Nothing to do.
        return;
    }

    if (! per_connection->active) {
        return;
    }

    if (! per_connection->input) {
        per_connection->input = start_input(tx.connection());
    }
    add_transaction(per_connection->input, tx);
    if (! per_connection->all_tx) {

        const char* to = per_connection->to;
        std::string to_s;

        if (VarExpand::test(to)) {
             to_s = VarExpand::acquire(
                tx.memory_manager(), to, tx.engine().var_config()
            ).execute_s(tx.memory_manager(), tx.var_store());
            to = to_s.c_str();
        }

        finish_input(per_connection->input, to);
        per_connection->active = false;
        per_connection->input.reset();
    }
}

void Delegate::on_connection_close(Connection connection) const
{
    per_connection_p per_connection;
    try {
        per_connection =
            connection.get_module_data<per_connection_p>(module());
    }
    catch (enoent) {
        // Nothing to do.
        return;
    }

    if (! per_connection->active || ! per_connection->input) {
        return;
    }

    const char* to = per_connection->to;
    std::string to_s;

    if (connection.transaction() && VarExpand::test(to)) {
         to_s = VarExpand::acquire(
            connection.transaction().memory_manager(),
            to,
            connection.transaction().engine().var_config()
        ).execute_s(
            connection.transaction().memory_manager(),
            connection.transaction().var_store()
        );
        to = to_s.c_str();
    }

    finish_input(per_connection->input, to);
    per_connection->active = false;
    per_connection->input.reset();
}


CLIPP::Input::Buffer bs_to_buf(
    MemoryManager   mm,
    ConstByteString bs
)
{
    return CLIPP::Input::Buffer(
        static_cast<const char*>(mm.memdup(bs.const_data(), bs.length())),
        bs.length()
    );
}

CLIPP::Input::Buffer s_to_buf(
    MemoryManager mm,
    const char*   s
)
{
    size_t len = strlen(s);
    return CLIPP::Input::Buffer(
        static_cast<const char*>(mm.memdup(s, len)),
        len
    );
}

CLIPP::Input::Buffer stream_to_buf(
    MemoryManager      mm,
    const ib_stream_t* stream
)
{
    assert(stream);

    // Special case zero and one chunk streams.
    if (stream->slen == 0) {
        return CLIPP::Input::Buffer();
    }
    char* buffer = static_cast<char*>(mm.alloc(stream->slen));
    char* cur = buffer;

    for (
        const ib_sdata_t* chunk = stream->head;
        chunk;
        chunk = chunk->next
    )
    {
        const char* chunk_data = static_cast<const char*>(chunk->data);
        copy(chunk_data, chunk_data + chunk->dlen, cur);
        cur += chunk->dlen;
    }

    return CLIPP::Input::Buffer(buffer, stream->slen);
}

CLIPP::Input::input_p start_input(Connection connection)
{
    CLIPP::Input::input_p input(new CLIPP::Input::Input(connection.id()));

    input->connection.connection_opened(
        s_to_buf(connection.memory_manager(), connection.local_ip_string()),
        connection.local_port(),
        s_to_buf(connection.memory_manager(), connection.remote_ip_string()),
        connection.remote_port()
    );

    return input;
}

void finish_input(const CLIPP::Input::input_p& input, const char* to)
{
    input->connection.connection_closed();

    if (! to[0]) {
        CLIPP::PBConsumer consumer(cerr);
        consumer(input);
    }
    else {
        CLIPP::PBConsumer consumer(to);
        consumer(input);
    }
}

void add_headers(
    MemoryManager              mm,
    CLIPP::Input::HeaderEvent& e,
    ConstParsedHeader          first
)
{
    for (
        ConstParsedHeader h = first;
        h;
        h = h.next()
    )
    {
        e.add(
            bs_to_buf(mm, h.name()),
            bs_to_buf(mm, h.value())
        );
    }
}

void add_transaction(const CLIPP::Input::input_p& input, ConstTransaction tx)
{
    CLIPP::Input::Transaction& clipp_tx = input->connection.add_transaction();
    MemoryManager mm = tx.connection().memory_manager();

    clipp_tx.request_started(
        bs_to_buf(mm, tx.request_line().raw()),
        bs_to_buf(mm, tx.request_line().method()),
        bs_to_buf(mm, tx.request_line().uri()),
        bs_to_buf(mm, tx.request_line().protocol())
    );

    add_headers(mm, clipp_tx.request_header(), tx.request_header());
    clipp_tx.request_header_finished();

    clipp_tx.request_body(
        stream_to_buf(mm, tx.ib()->request_body)
    );

    clipp_tx.request_finished();

    clipp_tx.response_started(
        bs_to_buf(mm, tx.response_line().raw()),
        bs_to_buf(mm, tx.response_line().protocol()),
        bs_to_buf(mm, tx.response_line().status()),
        bs_to_buf(mm, tx.response_line().message())
    );

    add_headers(mm, clipp_tx.response_header(), tx.response_header());
    clipp_tx.response_header_finished();

    clipp_tx.response_body(
        stream_to_buf(
            mm,
            tx.ib()->response_body
        )
    );

    clipp_tx.response_finished();
}

} // Anonymous
