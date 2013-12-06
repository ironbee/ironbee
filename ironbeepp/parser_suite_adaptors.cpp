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
 * @brief IronBee++ &mdash; ParserSuite Adapators Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbeepp/parser_suite_adaptors.hpp>

namespace IronBee {

PSHeaderToParsedHeader::PSHeaderToParsedHeader(MemoryPool memory_pool) :
    m_memory_pool(memory_pool)
{
    // nop
}

ParsedHeader PSHeaderToParsedHeader::operator()(
    const ParserSuite::parse_headers_result_t::header_t& header
) const
{
    ByteString value;
    if (header.value.size() == 1) {
        value = ByteString::create_alias(
            m_memory_pool,
            header.value.front().begin(), header.value.front().size()
        );
    }
    else {
        size_t total_size = 0;
        BOOST_FOREACH(const ParserSuite::span_t& span, header.value) {
            total_size += span.size();
        }
        char* buffer =
            reinterpret_cast<char*>(m_memory_pool.alloc(total_size));
        char* current = buffer;
        BOOST_FOREACH(const ParserSuite::span_t& span, header.value) {
            std::copy(span.begin(), span.end(), current);
            current += span.size();
        }
        value = ByteString::create_alias(m_memory_pool, buffer, total_size);
    }

    return ParsedHeader::create(
        m_memory_pool,
        ByteString::create_alias(
            m_memory_pool,
            header.key.begin(), header.key.size()
        ),
        value
    );
}

psheader_to_parsed_header_const_range_t psheaders_to_parsed_headers(
    IronBee::MemoryPool                                   memory_pool,
    const ParserSuite::parse_headers_result_t::headers_t& headers
)
{
    return psheader_to_parsed_header_const_range_t(
        psheader_to_parsed_header_const_iterator(
            headers.begin(), PSHeaderToParsedHeader(memory_pool)
        ),
        psheader_to_parsed_header_const_iterator(
            headers.end(), PSHeaderToParsedHeader(memory_pool)
        )
    );
}

} // IronBee
