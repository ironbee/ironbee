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
 * @brief IronBee++ --- ParserSuite adaptors
 *
 * This file provides code for adapting ParserSuite results to IronBee++.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP__PARSERSUITE_ADAPTORS__
#define __IBPP__PARSERSUITE_ADAPTORS__

#include <modules/parser_suite.hpp>

#include <ironbeepp/parsed_header.hpp>

#include <boost/iterator/transform_iterator.hpp>

namespace IronBee {

/**
 * Functional to translate a ParserSuite header to a ParsedHeader.
 *
 * Headers with multiple values (extended headers) will have their values
 * concatenated.  Headers with a single value will have their header directly
 * aliased.
 **/
class PSHeaderToParsedHeader :
    public std::unary_function<
        const ParserSuite::parse_headers_result_t::header_t&,
        ParsedHeader
    >
{
public:
    /**
     * Constructor.
     *
     * @param[in] memory_pool Memory pool to use for allocations.
     **/
    explicit
    PSHeaderToParsedHeader(MemoryPool memory_pool);

    /**
     * Call operator.
     *
     * @param[in] header Header to translate.
     * @return ParsedHeader aliasing members of @a header.
     **/
    ParsedHeader operator()(
        const ParserSuite::parse_headers_result_t::header_t& header
    ) const;

private:
    //! Memory pool to allocate bytestrings and ParserHeader from.
    MemoryPool m_memory_pool;
};

//! Type of iterator from adapt_headers().
typedef boost::transform_iterator<
    PSHeaderToParsedHeader,
    ParserSuite::parse_headers_result_t::headers_t::const_iterator
> psheader_to_parsed_header_const_iterator;

//! Range of @ref parsed_headers_const_iterator.
typedef boost::iterator_range<
    psheader_to_parsed_header_const_iterator
> psheader_to_parsed_header_const_range_t;

/**
 * Adapt @ref parse_header_result_t::headers_t to a sequence of ParsedHeader.
 *
 * @param[in] memory_pool Memory pool to use.
 * @param[in] headers    Headers to adapt.
 * @return Range of ParsedHeader.
 **/
psheader_to_parsed_header_const_range_t psheaders_to_parsed_headers(
    IronBee::MemoryPool                                   memory_pool,
    const ParserSuite::parse_headers_result_t::headers_t& headers
);

} // IronBee

#endif
