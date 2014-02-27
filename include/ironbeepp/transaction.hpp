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
 * @brief IronBee++ --- Transaction
 *
 * This file defines (Const)Transaction, a wrapper for ib_tx_t.
 *
 * @remark Developers should be familiar with @ref ironbeepp to understand
 * aspects of this code, e.g., the public/non-virtual inheritance.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP__TRANSACTION__
#define __IBPP__TRANSACTION__

#include <ironbeepp/abi_compatibility.hpp>
#include <ironbeepp/common_semantics.hpp>
#include <ironbeepp/memory_manager.hpp>
#include <ironbeepp/module.hpp>

#include <ironbee/engine.h>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdelete-non-virtual-dtor"
#endif
#include <boost/date_time/posix_time/ptime.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <ostream>

namespace IronBee {

class Engine;
class ConstModule;
class Context;
class Transaction;
class Connection;
class ParsedHeader;
class ParsedRequestLine;
class ParsedResponseLine;
class VarStore;
class ConstVarStore;

/**
 * Const Transaction; equivalent to a const pointer to ib_tx_t.
 *
 * Provides operators ==, !=, <, >, <=, >= and evaluation as a boolean for
 * singularity via CommonSemantics.
 *
 * See Transaction for discussion of Transaction
 *
 * @sa Transaction
 * @sa ironbeepp
 * @sa ib_tx_t
 * @nosubgrouping
 **/
class ConstTransaction :
    public CommonSemantics<ConstTransaction>
{
public:
    //! C Type.
    typedef const ib_tx_t* ib_type;

    /**
     * Construct singular ConstTransaction.
     *
     * All behavior of a singular ConstTransaction is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    ConstTransaction();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! const ib_tx_t accessor.
    // Intentionally inlined.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct Transaction from ib_tx_t.
    explicit
    ConstTransaction(ib_type ib_transaction);

    ///@}

    //! Associated engine.
    Engine engine() const;

    //! Memory manager used.
    MemoryManager memory_manager() const;

    //! Identifier.
    const char* id() const;

    //! Auditlog Identifier.
    const char* audit_log_id() const;

    //! Associated connection.
    Connection connection() const;

    //! Associated connection.
    Context context() const;

    /**
     * @name Timestamps
     * Timestamps.
     **/
    ///@{

    //! Start of transaction.
    boost::posix_time::ptime started_time() const;

    //! Start of request.
    boost::posix_time::ptime request_started_time() const;

    //! Start of request header.
    boost::posix_time::ptime request_header_time() const;

    //! Start of request body.
    boost::posix_time::ptime request_body_time() const;

    //! Finish of request.
    boost::posix_time::ptime request_finished_time() const;

    //! Start of response.
    boost::posix_time::ptime response_started_time() const;

    //! Start of response header.
    boost::posix_time::ptime response_header_time() const;

    //! Start of response body.
    boost::posix_time::ptime response_body_time() const;

    //! Finish of response.
    boost::posix_time::ptime response_finished_time() const;

    //! Start of post processing.
    boost::posix_time::ptime postprocess_time() const;

    //! Start of event logging.
    boost::posix_time::ptime logtime_time() const;

    //! Finish of transaction.
    boost::posix_time::ptime finished_time() const;
    ///@}

    //! Next transaction in current sequence.
    Transaction next() const;

    //! Hostname used in request.
    const char* hostname() const;

    //! Effective remote IP string.
    const char* effective_remote_ip_string() const;

    //! Path used in request.
    const char* path() const;

    //! Parsed request line.
    ParsedRequestLine request_line() const;

    /**
     * Parsed request header.
     *
     * This method returns the first parsed header.  Later individual
     * headers can be accessed via ParsedHeader::next().
     **/
    ParsedHeader request_header() const;

    /**
     * Parsed response header.
     *
     * This method returns the first parsed header.  Later individual
     * headers can be accessed via ParsedNameValue::next().
     **/
    ParsedHeader response_header() const;

    //! Parsed request line.
    ParsedResponseLine response_line() const;

    /**
     * @name Flags
     * Transaction Flags
     *
     * The masks for the flags are defined by the flags_e enum.  All flags
     * as a set of bits can be accessed via flags().  Individual flags can be
     * checked either via flags() @c & @c flag_X or via @c is_X().
     **/
    ///@{

    //! Possible flags.  Treat as bit masks.
    enum flags_e {
        flag_none                    = IB_TX_FNONE,
        flag_http09                  = IB_TX_FHTTP09,
        flag_pipelined               = IB_TX_FPIPELINED,

        flag_request_started         = IB_TX_FREQ_STARTED,
        flag_request_line            = IB_TX_FREQ_LINE,
        flag_request_header          = IB_TX_FREQ_HEADER,
        flag_request_body            = IB_TX_FREQ_BODY,
        flag_request_trailer         = IB_TX_FREQ_TRAILER,
        flag_request_finished        = IB_TX_FREQ_FINISHED,
        flag_request_has_data        = IB_TX_FREQ_HAS_DATA,

        flag_response_started        = IB_TX_FRES_STARTED,
        flag_response_line           = IB_TX_FRES_LINE,
        flag_response_header         = IB_TX_FRES_HEADER,
        flag_response_body           = IB_TX_FRES_BODY,
        flag_response_trailer        = IB_TX_FRES_TRAILER,
        flag_response_finished       = IB_TX_FRES_FINISHED,
        flag_response_has_data       = IB_TX_FRES_HAS_DATA,

        flag_logging                 = IB_TX_FLOGGING,
        flag_postprocess             = IB_TX_FPOSTPROCESS,

        flag_error                   = IB_TX_FERROR,
        flag_suspicious              = IB_TX_FSUSPICIOUS,
        flag_blocked                 = IB_TX_FBLOCKED,

        flag_inspect_request_uri     = IB_TX_FINSPECT_REQURI,
        flag_inspect_request_params  = IB_TX_FINSPECT_REQPARAMS,
        flag_inspect_request_header  = IB_TX_FINSPECT_REQHDR,
        flag_inspect_request_body    = IB_TX_FINSPECT_REQBODY,
        flag_inspect_response_header = IB_TX_FINSPECT_RESHDR,
        flag_inspect_response_body   = IB_TX_FINSPECT_RESBODY,

        flag_blocking_mode           = IB_TX_FBLOCKING_MODE,
        flag_block_advisory          = IB_TX_FBLOCK_ADVISORY,
        flag_block_phase             = IB_TX_FBLOCK_PHASE,
        flag_block_immediate         = IB_TX_FBLOCK_IMMEDIATE,
        flag_allow_phase             = IB_TX_FALLOW_PHASE,
        flag_allow_request           = IB_TX_FALLOW_REQUEST,
        flag_allow_all               = IB_TX_FALLOW_ALL
    };

    //! All flags.
    ib_flags_t flags() const;

    //! flags() & flag_none
    bool is_none() const
    {
        return flags() & flag_none;
    }

    //! flags() & flag_http09
    bool is_http09() const
    {
        return flags() & flag_http09;
    }

    //! flags() & flag_pipelined
    bool is_pipelined() const
    {
        return flags() & flag_pipelined;
    }

    //! flags() & flag_request_started
    bool is_request_started() const
    {
        return flags() & flag_request_started;
    }

    //! flags() & flag_request_line
    bool is_request_line() const
    {
        return flags() & flag_request_line;
    }

    //! flags() & flag_request_header
    bool is_request_header() const
    {
        return flags() & flag_request_header;
    }

    //! flags() & flag_request_body
    bool is_request_body() const
    {
        return flags() & flag_request_body;
    }

    //! flags() & flag_request_finished
    bool is_request_finished() const
    {
        return flags() & flag_request_finished;
    }

    //! flags() & flag_request_has_data
    bool is_request_has_data() const
    {
        return flags() & flag_request_has_data;
    }

    //! flags() & flag_response_started
    bool is_response_started() const
    {
        return flags() & flag_response_started;
    }

    //! flags() & flag_response_line
    bool is_response_line() const
    {
        return flags() & flag_response_line;
    }

    //! flags() & flag_response_header
    bool is_response_header() const
    {
        return flags() & flag_response_header;
    }

    //! flags() & flag_response_body
    bool is_response_body() const
    {
        return flags() & flag_response_body;
    }

    //! flags() & flag_response_finished
    bool is_response_finished() const
    {
        return flags() & flag_response_finished;
    }

    //! flags() & flag_response_has_data
    bool is_response_has_data() const
    {
        return flags() & flag_response_has_data;
    }

    //! flags() & flag_logging
    bool is_logging() const
    {
        return flags() & flag_logging;
    }

    //! flags() & flag_postprocess
    bool is_postprocess() const
    {
        return flags() & flag_postprocess;
    }

    //! flags() & flag_error
    bool is_error() const
    {
        return flags() & flag_error;
    }

    //! flags() & flag_suspicious
    bool is_suspicious() const
    {
        return flags() & flag_suspicious;
    }

    //! flags() & flag_blocked
    bool is_blocked() const
    {
        return flags() & flag_blocked;
    }

    //! flags() & flag_inspect_request_uri
    bool is_inspect_request_uri() const
    {
        return flags() & flag_inspect_request_uri;
    }

    //! flags() & flag_inspect_request_params
    bool is_inspect_request_params() const
    {
        return flags() & flag_inspect_request_params;
    }

    //! flags() & flag_inspect_request_header
    bool is_inspect_request_header() const
    {
        return flags() & flag_inspect_request_header;
    }

    //! flags() & flag_inspect_request_body
    bool is_inspect_request_body() const
    {
        return flags() & flag_inspect_request_body;
    }

    //! flags() & flag_inspect_response_header
    bool is_inspect_response_header() const
    {
        return flags() & flag_inspect_response_header;
    }

    //! flags() & flag_inspect_response_body
    bool is_inspect_response_body() const
    {
        return flags() & flag_inspect_response_body;
    }

    //! flags() & flag_blocking_mode
    bool is_blocking_mode() const
    {
        return flags() & flag_blocking_mode;
    }

    //! flags() & flag_block_advisory
    bool is_block_advisory() const
    {
        return flags() & flag_block_advisory;
    }

    //! flags() & flag_block_phase
    bool is_block_phase() const
    {
        return flags() & flag_block_phase;
    }

    //! flags() & flag_block_immediate
    bool is_block_immediate() const
    {
        return flags() & flag_block_immediate;
    }

    //! flags() & flag_allow_phase
    bool is_allow_phase() const
    {
        return flags() & flag_allow_phase;
    }

    //! flags() & flag_allow_request
    bool is_allow_request() const
    {
        return flags() & flag_allow_request;
    }

    //! flags() & flag_allow_all
    bool is_allow_all() const
    {
        return flags() & flag_allow_all;
    }

    ib_block_method_t block_method() const
    {
        return ib()->block_method;
    }

    ///@}

    //! Access Var Store.
    ConstVarStore var_store() const;

private:
    ib_type m_ib;
};

/**
 * Transaction; equivalent to a pointer to ib_tx_t.
 *
 * Transaction can be treated as ConstTransaction.  See @ref ironbeepp for
 * details on IronBee++ object semantics.
 *
 * A transaction is a request/response pair within a connection.  Current
 * C++ support is limited.
 *
 * This class adds no functionality to ConstTransaction beyond providing a
 * non-const @c ib_tx_t* via ib().
 *
 * @sa ConstTransaction
 * @sa ironbeepp
 * @sa ib_tx_t
 * @nosubgrouping
 **/
class Transaction :
    public ConstTransaction
{
public:
    //! C Type.
    typedef ib_tx_t* ib_type;

    /**
     * Remove the constness of a ConstTransaction.
     *
     * @warning This is as dangerous as a @c const_cast, use carefully.
     *
     * @param[in] transaction ConstTransaction to remove const from.
     * @returns Transaction pointing to same underlying transaction as
     *          @a transaction.
     **/
    static Transaction remove_const(ConstTransaction transaction);

    /**
     * Construct singular Transaction.
     *
     * All behavior of a singular Transaction is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    Transaction();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! ib_tx_t accessor.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct Transaction from ib_tx_t.
    explicit
    Transaction(ib_type ib_transaction);

    ///@}


    /**
     * Copy @a t into the T module data.
     *
     * ConstTransaction::memory_manager() will be charged with
     * destroying the copy of @a t when the transaction is over.
     *
     * @param[in] m The module to store @a t for.
     * @param[in] t The module data.
     * @throws IronBee errors on C API failures.
     */
    template<typename T>
    void set_module_data(ConstModule m, T t);

    /**
     * Return a reference to the stored module transaction data.
     *
     * @param[in] m The module that the data is stored for.
     * @throws IronBee errors on C API failures.
     */
    template<typename T>
    T get_module_data(ConstModule m);

    /**
     * Create a new transaction.
     *
     * The C API provides a plugin context @c void* parameter for transaction
     * creation.  This is currently unsupported.  It is intended that C++
     * server plugins not need that context.
     *
     * @param[in] connection Connection to associate transaction with.
     * @returns Transaction
     **/
    static
    Transaction create(Connection connection);

    //! All flags.
    ib_flags_t& flags() const;

    /**
     * Destroy transaction.
     **/
    void destroy() const;

    //! Access Var Store.
    VarStore var_store() const;

private:
    ib_type m_ib;
};

template<typename T>
void Transaction::set_module_data(ConstModule m, T t) {
    void *v = value_to_data(t, memory_manager().ib());

    throw_if_error(
        ib_tx_set_module_data(ib(), m.ib(), v)
    );
}

template<typename T>
T Transaction::get_module_data(ConstModule m)
{
    void *v = NULL;

    throw_if_error(
        ib_tx_get_module_data(ib(), m.ib(), &v)
    );

    return data_to_value<T>(v);
}


/**
 * Output operator for Transaction.
 *
 * Output IronBee::Transaction[@e value] where @e value is the id.
 *
 * @param[in] o Ostream to output to.
 * @param[in] transaction Transaction to output.
 * @return @a o
 **/
std::ostream& operator<<(
    std::ostream&           o,
    const ConstTransaction& transaction
);

} // IronBee

#endif
