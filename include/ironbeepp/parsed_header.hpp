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
 * @brief IronBee++ --- ParsedHeader
 *
 * This file defines (Const)ParsedHeader, a wrapper for
 * ib_parsed_header_t.
 *
 * @remark Developers should be familiar with @ref ironbeepp to understand
 * aspects of this code, e.g., the public/non-virtual inheritance.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP__PARSED_NAME_VALUE__
#define __IBPP__PARSED_NAME_VALUE__


#include <ironbeepp/abi_compatibility.hpp>
#include <ironbeepp/byte_string.hpp>
#include <ironbeepp/common_semantics.hpp>
#include <ironbeepp/memory_pool.hpp>
#include <ironbeepp/throw.hpp>

#include <ironbee/parsed_content.h>

#include <boost/foreach.hpp>

#include <ostream>

// IronBee C Type
typedef struct ib_parsed_header_t ib_parsed_header_t;

namespace IronBee {

class ParsedHeader;
class MemoryPool;

/**
 * Const ParsedHeader; equivalent to a const pointer to ib_parsed_header_t.
 *
 * Provides operators ==, !=, <, >, <=, >= and evaluation as a boolean for
 * singularity via CommonSemantics.
 *
 * See ParsedHeader for discussion of ParsedHeader
 *
 * @sa ParsedHeader
 * @sa ironbeepp
 * @sa ib_parsed_header_t
 * @nosubgrouping
 **/
class ConstParsedHeader :
    public CommonSemantics<ConstParsedHeader>
{
public:
    //! C Type.
    typedef const ib_parsed_header_t* ib_type;

    /**
     * Construct singular ConstParsedHeader.
     *
     * All behavior of a singular ConstParsedHeader is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    ConstParsedHeader();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! const ib_parsed_header_t accessor.
    // Intentionally inlined.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct ParsedHeader from ib_parsed_header_t.
    explicit
    ConstParsedHeader(ib_type ib_parsed_header);

    ///@}

    //! Name.
    ByteString name() const;

    //! Value.
    ByteString value() const;

    //! Next name/value.
    ParsedHeader next() const;

private:
    ib_type m_ib;
};

/**
 * ParsedHeader; equivalent to a pointer to
 * ib_parsed_header_t.
 *
 * ParsedHeader can be treated as ConstParsedHeader.  See @ref
 * ironbeepp for details on IronBee++ object semantics.
 *
 * ParsedHeader is forms a simple linked list of pairs of byte strings.  It
 * is used as part of the parsed content interface which provides a very
 * simple (minimal dependency) API for external input providers.
 *
 * ParsedHeader adds no functionality to ConstParsedHeader except for
 * exposing a non-const @c ib_parsed_header_t* via ib().
 *
 * @sa ConstParsedHeader
 * @sa ironbeepp
 * @sa ib_parsed_header_t
 * @nosubgrouping
 **/
class ParsedHeader :
    public ConstParsedHeader
{
public:
    //! C Type.
    typedef ib_parsed_header_t* ib_type;

    /**
     * Remove the constness of a ConstParsedHeader.
     *
     * @warning This is as dangerous as a @c const_cast, use carefully.
     *
     * @param[in] parsed_header ConstParsedHeader to remove const from.
     * @returns ParsedHeader pointing to same underlying parsed_header
     *          as @a parsed_header.
     **/
    static ParsedHeader remove_const(
        ConstParsedHeader parsed_header
    );

    /**
     * Construct singular ParsedHeader.
     *
     * All behavior of a singular ParsedHeader is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    ParsedHeader();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! ib_parsed_header_t accessor.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct ParsedHeader from ib_parsed_header_t.
    explicit
    ParsedHeader(ib_type ib_parsed_header);

    ///@}

    /**
     * Create a ParsedHeader.
     *
     * @param[in] pool  Memory pool to use for allocations.
     * @param[in] name  Name.
     * @param[in] value Value.
     * @returns ParsedHeader
     **/
    static
    ParsedHeader create(
        MemoryPool pool,
        ByteString name,
        ByteString value
    );

private:
    ib_type m_ib;
};

/**
 * Output operator for ParsedHeader.
 *
 * Output IronBee::ParsedHeader[@e value] where @e value is the name and
 * value joined by a colon.
 *
 * @param[in] o Ostream to output to.
 * @param[in] parsed_header ParsedHeader to output.
 * @return @a o
 **/
std::ostream& operator<<(
    std::ostream&               o,
    const ConstParsedHeader& parsed_header
);


namespace Internal {
/// @cond Internal

/**
 * Turn a sequence of ParsedHeaders into a C API appropriate type.
 *
 * @param[in] memory_pool Memory pool to allocate from.
 * @param[in] begin       Beginning of sequence.
 * @param[in] end         End of sequence.
 * @returns ib_parsed_headers_t for use in C API.
 **/
template <typename Iterator>
ib_parsed_headers_t* make_pnv_list(
    MemoryPool memory_pool,
    Iterator   begin,
    Iterator   end
)
{
    ib_parsed_headers_t* ib_pnv_list;
    throw_if_error(
        ib_parsed_headers_create(
            &ib_pnv_list,
            memory_pool.ib()
        )
    );

    BOOST_FOREACH(
        ParsedHeader pnv,
        std::make_pair(begin, end)
    ) {
        // This will reconstruct the bytestrings but not copy the data.
        // The C API is currently asymmetric: named values are consumed as
        // structures but added to list as members.  IronBee++ hides that
        // asymmetry.
        throw_if_error(
            ib_parsed_headers_add(
                ib_pnv_list,
                pnv.name().const_data(),
                pnv.name().length(),
                pnv.value().const_data(),
                pnv.value().length()
            )
        );
    }

    return ib_pnv_list;
}

/// @endcond
} // Internal

} // IronBee

#endif
