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
 * @brief IronBee++ &mdash; TransactionData
 *
 * This file defines (Const)TransactionData, a wrapper for ib_txdata_t.
 *
 * @remark Developers should be familiar with @ref ironbeepp to understand
 * aspects of this code, e.g., the public/non-virtual inheritance.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP__TRANSACTION_DATA__
#define __IBPP__TRANSACTION_DATA__

#include <ironbeepp/common_semantics.hpp>

#include <ironbee/engine.h>

#include <ostream>

// IronBee C Type
typedef struct ib_txdata_t ib_txdata_t;

namespace IronBee {

class Engine;
class MemoryPool;
class Transaction;

/**
 * Const TransactionData; equivalent to a const pointer to ib_txdata_t.
 *
 * Provides operators ==, !=, <, >, <=, >= and evaluation as a boolean for
 * singularity via CommonSemantics.
 *
 * See TransactionData for discussion of Transaction Data
 *
 * @sa TransactionData
 * @sa ironbeepp
 * @sa ib_txdata_t
 * @nosubgrouping
 **/
class ConstTransactionData :
    public CommonSemantics<ConstTransactionData>
{
public:
    //! C Type.
    typedef const ib_txdata_t* ib_type;

    /**
     * Construct singular ConstTransactionData.
     *
     * All behavior of a singular ConstTransactionData is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    ConstTransactionData();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! const ib_txdata_t accessor.
    // Intentionally inlined.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct TransactionData from ib_txdata_t.
    explicit
    ConstTransactionData(ib_type ib_transaction_data);

    ///@}

    //! Possible types of data.
    enum type_e {
        META         = IB_DTYPE_META,
        RAW          = IB_DTYPE_RAW,
        HTTP_LINE    = IB_DTYPE_HTTP_LINE,
        HTTP_HEADER  = IB_DTYPE_HTTP_HEADER,
        HTTP_BODY    = IB_DTYPE_HTTP_BODY,
        HTTP_TRAILER = IB_DTYPE_HTTP_TRAILER
    };

    //! Type of data.
    type_e type() const;

    //! Length of data.
    size_t length() const;

    //! Pointer to data.
    char* data() const;

private:
    ib_type m_ib;
};

/**
 * TransactionData; equivalent to a pointer to ib_txdata_t.
 *
 * TransactionData can be treated as ConstTransactionData.  See @ref ironbeepp for
 * details on IronBee++ object semantics.
 *
 * This class provides no functionality over ConstTransactionData except for
 * providing a non-const @c ib_txdata_t* via ib().
 *
 * @sa ConstTransactionData
 * @sa ironbeepp
 * @sa ib_txdata_t
 * @nosubgrouping
 **/
class TransactionData :
    public ConstTransactionData
{
public:
    //! C Type.
    typedef ib_txdata_t* ib_type;

    /**
     * Remove the constness of a ConstTransactionData.
     *
     * @warning This is as dangerous as a @c const_cast, use carefully.
     *
     * @param[in] transaction_data ConstTransactionData to remove const from.
     * @returns TransactionData pointing to same underlying transaction data
     *          as @a transaction_data.
     **/
    static TransactionData remove_const(ConstTransactionData transaction_data);

    /**
     * Construct singular TransactionData.
     *
     * All behavior of a singular TransactionData is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    TransactionData();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! ib_txdata_t accessor.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct TransactionData from ib_txdata_t.
    explicit
    TransactionData(ib_type ib_transaction_data);

    ///@}

    /**
     * Create TransactionData aliasing memory.
     *
     * The memory pointed to by @a data must exceed the usetime of
     * transaction data (usually a transaction).  It is recommended that
     * @a mp be the memory pool of the current transaction.
     *
     * @param[in] mp          Memory pool to use for allocations.
     * @param[in] type        Type of data.
     * @param[in] data        Data to alias.
     * @param[in] data_length Length of @a data.
     * @returns TransactionData.
     **/
    static
    TransactionData create_alias(
        MemoryPool mp,
        type_e     type,
        char*      data,
        size_t     data_length
    );

private:
    ib_type m_ib;
};

/**
 * Output operator for TransactionData.
 *
 * Output IronBee::TransactionData[@e value] where @e value is the transaction
 * data.
 *
 * @param[in] o Ostream to output to.
 * @param[in] transaction_data TransactionData to output.
 * @return @a o
 **/
std::ostream& operator<<(
    std::ostream& o,
    const ConstTransactionData& transaction_data
);

} // IronBee

#endif
