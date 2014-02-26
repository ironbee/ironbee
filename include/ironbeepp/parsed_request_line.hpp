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
 * @brief IronBee++ --- ParsedRequestLine
 *
 * This file defines (Const)ParsedRequestLine, a wrapper for
 * ib_parsed_req_line_t.
 *
 * @remark Developers should be familiar with @ref ironbeepp to understand
 * aspects of this code, e.g., the public/non-virtual inheritance.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP__PARSED_REQUEST_LINE__
#define __IBPP__PARSED_REQUEST_LINE__

#include <ironbeepp/abi_compatibility.hpp>
#include <ironbeepp/common_semantics.hpp>

#include <ostream>

// IronBee C Type
typedef struct ib_parsed_req_line_t ib_parsed_req_line_t;

namespace IronBee {

class MemoryManager;
class ByteString;

/**
 * Const ParsedRequestLine; equivalent to a const pointer to
 * ib_parsed_req_line_t.
 *
 * Provides operators ==, !=, <, >, <=, >= and evaluation as a boolean for
 * singularity via CommonSemantics.
 *
 * See ParsedRequestLine for discussion of Parsed RequestLine
 *
 * @sa ParsedRequestLine
 * @sa ironbeepp
 * @sa ib_parsed_req_line_t
 * @nosubgrouping
 **/
class ConstParsedRequestLine :
    public CommonSemantics<ConstParsedRequestLine>
{
public:
    //! C Type.
    typedef const ib_parsed_req_line_t* ib_type;

    /**
     * Construct singular ConstParsedRequestLine.
     *
     * All behavior of a singular ConstParsedRequestLine is undefined except
     * for assignment, copying, comparison, and evaluate-as-bool.
     **/
    ConstParsedRequestLine();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! const ib_parsed_req_line_t accessor.
    // Intentionally inlined.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct ParsedRequestLine from ib_parsed_req_line_t.
    explicit
    ConstParsedRequestLine(ib_type ib_parsed_request_line);

    ///@}

    //! Raw request line.
    ByteString raw() const;

    //! HTTP Method.
    ByteString method() const;

    //! HTTP URI.
    ByteString uri() const;

    //! HTTP Protocol.
    ByteString protocol() const;

private:
    ib_type m_ib;
};

/**
 * ParsedRequestLine; equivalent to a pointer to ib_parsed_req_line_t.
 *
 * ParsedRequestLine can be treated as ConstParsedRequestLine.  See @ref
 * ironbeepp for details on IronBee++ object semantics.
 *
 * A parsed request line is a representation of an HTTP request line.  It
 * consists of a method (e.g,. GET), path (URL, parameters, etc.), and the
 * HTTP Version (e.g., HTTP/1.0).
 *
 * ParsedRequestLine provides no functionality over ConstParsedRequestLine
 * except providing a non-const @c ib_parsed_req_line_t* via ib().
 *
 * @sa ConstParsedRequestLine
 * @sa ironbeepp
 * @sa ib_parsed_req_line_t
 * @nosubgrouping
 **/
class ParsedRequestLine :
    public ConstParsedRequestLine
{
public:
    //! C Type.
    typedef ib_parsed_req_line_t* ib_type;

    /**
     * Remove the constness of a ConstParsedRequestLine.
     *
     * @warning This is as dangerous as a @c const_cast, use carefully.
     *
     * @param[in] parsed_request_line ConstParsedRequestLine to remove const
     *                               from.
     * @returns ParsedRequestLine pointing to same underlying parsed
     *          request_line as @a parsed_request_line.
     **/
    static ParsedRequestLine remove_const(
         ConstParsedRequestLine parsed_request_line
    );

    /**
     * Construct singular ParsedRequestLine.
     *
     * All behavior of a singular ParsedRequestLine is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    ParsedRequestLine();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! ib_parsed_req_line_t accessor.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct ParsedRequestLine from ib_parsed_req_line_t.
    explicit
    ParsedRequestLine(ib_type ib_parsed_request_line);

    ///@}

   /**
    * Create a ParsedRequestLine, aliasing memory.
    *
    * @param[in] memory_manager  Memory manager to create from.
    * @param[in] raw             Raw request line.
    * @param[in] raw_length      Length of @a raw.
    * @param[in] method          HTTP method.
    * @param[in] method_length   Length of @a method.
    * @param[in] uri             HTTP URI.
    * @param[in] uri_length      Length of @a uri.
    * @param[in] protocol        HTTP protocol.
    * @param[in] protocol_length Length of @a protocol.
    * @return Parsed request line.
    **/
   static
   ParsedRequestLine create_alias(
       MemoryManager memory_manager,
       const char* raw,
       size_t raw_length,
       const char* method,
       size_t method_length,
       const char* uri,
       size_t uri_length,
       const char* protocol,
       size_t protocol_length
   );

private:
    ib_type m_ib;
};

/**
 * Output operator for ParsedRequestLine.
 *
 * Output IronBee::ParsedRequestLine[@e value] where @e value is the
 * method, uri, and protocol separated by space.
 *
 * @param[in] o Ostream to output to.
 * @param[in] parsed_request_line ParsedRequestLine to output.
 * @return @a o
 **/
std::ostream& operator<<(
    std::ostream&                 o,
    const ConstParsedRequestLine& parsed_request_line
);

} // IronBee

#endif
