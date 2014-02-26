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
 * @brief IronBee++ --- ParsedResponseLine
 *
 * This file defines (Const)ParsedResponseLine, a wrapper for
 * ib_parsed_resp_line_t.
 *
 * @remark Developers should be familiar with @ref ironbeepp to understand
 * aspects of this code, e.g., the public/non-virtual inheritance.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP__PARSED_RESPONSE_LINE__
#define __IBPP__PARSED_RESPONSE_LINE__

#include <ironbeepp/abi_compatibility.hpp>
#include <ironbeepp/common_semantics.hpp>

#include <ostream>

// IronBee C Type
typedef struct ib_parsed_resp_line_t ib_parsed_resp_line_t;

namespace IronBee {

class MemoryManager;
class ByteString;

/**
 * Const ParsedResponseLine; equivalent to a const pointer to
 * ib_parsed_resp_line_t.
 *
 * Provides operators ==, !=, <, >, <=, >= and evaluation as a boolean for
 * singularity via CommonSemantics.
 *
 * See ParsedResponseLine for discussion of Parsed ResponseLine
 *
 * @sa ParsedResponseLine
 * @sa ironbeepp
 * @sa ib_parsed_resp_line_t
 * @nosubgrouping
 **/
class ConstParsedResponseLine :
    public CommonSemantics<ConstParsedResponseLine>
{
public:
    //! C Type.
    typedef const ib_parsed_resp_line_t* ib_type;

    /**
     * Construct singular ConstParsedResponseLine.
     *
     * All behavior of a singular ConstParsedResponseLine is undefined except
     * for assignment, copying, comparison, and evaluate-as-bool.
     **/
    ConstParsedResponseLine();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! const ib_parsed_resp_line_t accessor.
    // Intentionally inlined.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct ParsedResponseLine from ib_parsed_resp_line_t.
    explicit
    ConstParsedResponseLine(ib_type ib_parsed_response_line);

    ///@}

    //! Raw response line.
    ByteString raw() const;

    //! HTTP Protocol (protocol/version).
    ByteString protocol() const;

    //! HTTP Status.
    ByteString status() const;

    //! HTTP Message.
    ByteString message() const;

private:
    ib_type m_ib;
};

/**
 * ParsedResponseLine; equivalent to a pointer to ib_parsed_resp_line_t.
 *
 * ParsedResponseLine can be treated as ConstParsedResponseLine.  See @ref
 * ironbeepp for details on IronBee++ object semantics.
 *
 * A parsed response line is a representation of an HTTP response line. It
 * consists of a code and message.
 *
 * ParsedResponseLine provides no functionality over ConstParsedResponseLine
 * except providing a non-const @c ib_parsed_resp_line_t* via ib().
 *
 * @sa ConstParsedResponseLine
 * @sa ironbeepp
 * @sa ib_parsed_resp_line_t
 * @nosubgrouping
 **/
class ParsedResponseLine :
    public ConstParsedResponseLine
{
public:
    //! C Type.
    typedef ib_parsed_resp_line_t* ib_type;

    /**
     * Remove the constness of a ConstParsedResponseLine.
     *
     * @warning This is as dangerous as a @c const_cast, use carefully.
     *
     * @param[in] parsed_response_line ConstParsedResponseLine to remove const
     *                               from.
     * @returns ParsedResponseLine pointing to same underlying parsed
     *          response_line as @a parsed_response_line.
     **/
    static ParsedResponseLine remove_const(
         ConstParsedResponseLine parsed_response_line
    );

    /**
     * Construct singular ParsedResponseLine.
     *
     * All behavior of a singular ParsedResponseLine is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    ParsedResponseLine();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! ib_parsed_resp_line_t accessor.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct ParsedResponseLine from ib_parsed_resp_line_t.
    explicit
    ParsedResponseLine(ib_type ib_parsed_response_line);

    ///@}

   /**
    * Create a ParsedResponseLine, aliasing memory.
    *
    * @param[in] memory_manager  Memory manager to create from.
    * @param[in] raw             Raw response line.
    * @param[in] raw_length      Length of @a raw.
    * @param[in] protocol        HTTP protocol.
    * @param[in] protocol_length Length of @a protocol.
    * @param[in] status          HTTP status.
    * @param[in] status_length   Length of @a status.
    * @param[in] message         HTTP message.
    * @param[in] message_length  Length of @a protocol.
    * @return Parsed request line.
    **/
   static
   ParsedResponseLine create_alias(
       MemoryManager memory_manager,
       const char* raw,
       size_t raw_length,
       const char* protocol,
       size_t protocol_length,
       const char* status,
       size_t status_length,
       const char* message,
       size_t message_length
   );

private:
    ib_type m_ib;
};

/**
 * Output operator for ParsedResponseLine.
 *
 * Output IronBee::ParsedResponseLine[@e value] where @e value is the
 * status and message separated by space.
 *
 * @param[in] o Ostream to output to.
 * @param[in] parsed_response_line ParsedResponseLine to output.
 * @return @a o
 **/
std::ostream& operator<<(
    std::ostream&                 o,
    const ConstParsedResponseLine& parsed_response_line
);

} // IronBee

#endif
