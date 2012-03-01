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
 * @brief IronBee++ Byte String Implementation
 * @internal
 *
 * @sa byte_string.hpp
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#define IBPP_EXPOSE_C
#include <ironbeepp/byte_string.hpp>
#include <ironbeepp/internal/catch.hpp>

#include "throw.hpp"

#include <ironbee/bytestr.h>

#include <boost/make_shared.hpp>

#include <cstring>
#include <cassert>

namespace IronBee {

namespace Internal {

struct ByteStringData
{
    ib_bytestr_t* ib_bytestr;
};

} // Internal

ByteString::ByteString()
{
    // nop
}

ByteString ByteString::create( MemoryPool pool )
{
    ib_bytestr_t* bs = NULL;

    ib_status_t rc = ib_bytestr_create( &bs, pool.ib(), 0 );
    Internal::throw_if_error( rc );

    return ByteString( bs );
}

ByteString ByteString::create(
    MemoryPool pool,
    const char* data,
    size_t      length
)
{
    ib_bytestr_t* bs = NULL;

    ib_status_t rc = ib_bytestr_dup_mem(
        &bs,
        pool.ib(),
        reinterpret_cast<const uint8_t*>( data ),
        length
    );
    Internal::throw_if_error( rc );

    return ByteString( bs );
}

ByteString ByteString::create( MemoryPool pool, const char *cstring )
{
    ib_bytestr_t* bs = NULL;

    ib_status_t rc = ib_bytestr_dup_nulstr( &bs, pool.ib(), cstring );
    Internal::throw_if_error( rc );

    return ByteString( bs );
}

ByteString ByteString::create( MemoryPool pool, const std::string& s )
{
    return ByteString::create( pool, s.data(), s.length() );
}

ByteString ByteString::create_alias(
    MemoryPool  pool,
    const char* data,
    size_t      length
)
{
    ib_bytestr_t* bs = NULL;

    ib_status_t rc = ib_bytestr_alias_mem(
        &bs,
        pool.ib(),
        reinterpret_cast<const uint8_t*>( data ),
        length
    );
    Internal::throw_if_error( rc );

    return ByteString( bs );
}

ByteString ByteString::create_alias(
    MemoryPool  pool,
    const char* cstring
)
{
    ib_bytestr_t* bs = NULL;

    ib_status_t rc = ib_bytestr_alias_nulstr( &bs, pool.ib(), cstring );
    Internal::throw_if_error( rc );

    return ByteString( bs );
}

ByteString ByteString::create_alias(
    MemoryPool pool,
    const std::string& s
)
{
    return create_alias( pool, s.data(), s.length() );
}

ByteString ByteString::alias( MemoryPool pool ) const
{
    ib_bytestr_t* bs = NULL;

    ib_status_t rc = ib_bytestr_alias( &bs, pool.ib(), ib() );
    Internal::throw_if_error( rc );

    return ByteString( bs );
}

ByteString ByteString::alias() const
{
    return alias( memory_pool() );
}

ByteString ByteString::dup( MemoryPool pool ) const
{
    ib_bytestr_t* bs = NULL;

    ib_status_t rc = ib_bytestr_dup( &bs, pool.ib(), ib() );
    Internal::throw_if_error( rc );

    return ByteString( bs );
}

ByteString ByteString::dup() const
{
    return dup( memory_pool() );
}

std::string ByteString::to_s() const
{
    return std::string( const_data(), length() );
}

MemoryPool ByteString::memory_pool() const
{
    return MemoryPool( ib_bytestr_mpool( ib() ) );
}

bool ByteString::read_only() const
{
    return ib_bytestr_read_only( ib() ) == 1;
}

size_t ByteString::length() const
{
    return ib_bytestr_length( ib() );
}

size_t ByteString::size() const
{
    return ib_bytestr_size( ib() );
}

const char* ByteString::const_data() const
{
    return reinterpret_cast<const char*>( ib_bytestr_const_ptr( ib() ) );
}

char* ByteString::data()
{
    return reinterpret_cast<char*>( ib_bytestr_ptr( ib() ) );
}

void ByteString::make_read_only()
{
    ib_bytestr_make_read_only( ib() );
}

void ByteString::clear()
{
    set( static_cast<char*>( NULL ), 0 );
}

void ByteString::set( char* new_data, size_t new_length )
{
    Internal::throw_if_error(
        ib_bytestr_setv(
            ib(),
            reinterpret_cast<uint8_t*>( new_data ),
            new_length
        )
    );
}

void ByteString::set( const char* new_data, size_t new_length )
{
    Internal::throw_if_error(
        ib_bytestr_setv_const(
            ib(),
            reinterpret_cast<const uint8_t*>( new_data ),
            new_length
        )
    ) ;
}

void ByteString::set( char* cstring )
{
    set( cstring, strlen( cstring ) );
}

void ByteString::set( const char* cstring )
{
    set( cstring, strlen( cstring ) );
}

void ByteString::set( const std::string& s )
{
    set( s.data(), s.length() );
}

void ByteString::append( const ByteString& tail )
{
    Internal::throw_if_error( ib_bytestr_append( ib(), tail.ib() ) );
}

void ByteString::append( const char* new_data, size_t new_length )
{
    Internal::throw_if_error(
        ib_bytestr_append_mem(
            ib(),
            reinterpret_cast<const uint8_t*>( new_data ),
            new_length
        )
    );
}

void ByteString::append( const char* cstring )
{
    Internal::throw_if_error( ib_bytestr_append_nulstr( ib(), cstring ) );
}

void ByteString::append( const std::string& s )
{
    append( s.data(), s.length() );
}

int ByteString::index_of( const char* cstring )
{
    return ib_bytestr_index_of_c( ib(), cstring );
}

int ByteString::index_of( const std::string& s )
{
    return index_of( s.c_str() );
}

bool ByteString::operator==( const ByteString& other ) const
{
    return ( ! *this && ! other ) || ( *this && other && ib() == other.ib() );
}

bool ByteString::operator<( const ByteString& other ) const
{
    if ( ! *this ) {
        return other;
    } else if ( ! other ) {
        return this;
    } else {
        return ib() < other.ib();
    }
}

ib_bytestr_t* ByteString::ib()
{
    return m_data->ib_bytestr;
}

const ib_bytestr_t* ByteString::ib() const
{
    return m_data->ib_bytestr;
}

ByteString::ByteString( ib_bytestr_t* ib_bytestr ) :
    m_data( boost::make_shared<Internal::ByteStringData>() )
{
    m_data->ib_bytestr = ib_bytestr;
}

ByteString::operator unspecified_bool_type() const
{
    return m_data ? unspecified_bool : 0;
}

std::ostream& operator<<( std::ostream& o, const ByteString& bytestr )
{
    if ( ! bytestr ) {
        o << "IronBee::ByteString[!singular!]";
    } else {
        o << "IronBee::ByteString[" << bytestr.to_s() << "]";
    }

    return o;
}

} // IronBee
