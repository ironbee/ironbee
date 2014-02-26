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
 * @brief IronBee++ Internals --- Memory Pool Lite Implementation
 *
 * @sa memory_pool.hpp
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbeepp/memory_pool_lite.hpp>
#include <ironbeepp/catch.hpp>
#include <ironbeepp/data.hpp>
#include <ironbeepp/throw.hpp>

#include <ironbee/mpool_lite.h>
#include <ironbee/util.h>

#include <cassert>

namespace IronBee {

extern "C" {

void ibpp_memory_pool_lite_cleanup(
    void* cbdata
)
{
    MemoryPoolLite::cleanup_t callback =
        data_to_value<MemoryPoolLite::cleanup_t>(cbdata);

    // Now we need to clear our own callback data.
    delete reinterpret_cast<boost::any*>(cbdata);

    ib_status_t rc = IB_OK;
    try {
        callback();
    }
    catch (...) {
        rc = convert_exception();
    }
    if (rc != IB_OK) {
        // Could we do something better.
        ib_util_log_error("Failure cleanup; no good remedy.");
    }
}

} // extern "C"

/* ConstMemoryPoolLite */

ConstMemoryPoolLite::ConstMemoryPoolLite() :
    m_ib(NULL)
{
    // nop
}

ConstMemoryPoolLite::ConstMemoryPoolLite(
    const ib_mpool_lite_t* ib_mpool
) :
    m_ib(ib_mpool)
{
    // nop
}

/* MemoryPool */

// See api documentation for discussion of const_cast.

MemoryPoolLite MemoryPoolLite::remove_const(const ConstMemoryPoolLite& cmp)
{
    return MemoryPoolLite(const_cast<ib_mpool_lite_t*>(cmp.ib()));
}

MemoryPoolLite::MemoryPoolLite() :
    m_ib(NULL)
{
    // nop
}

MemoryPoolLite MemoryPoolLite::create()
{
    ib_mpool_lite_t* ib_mpool = NULL;
    ib_status_t rc = ib_mpool_lite_create(&ib_mpool);
    throw_if_error(rc);
    assert(ib_mpool != NULL);

    return MemoryPoolLite(ib_mpool);
}

void* MemoryPoolLite::alloc(size_t size) const
{
    void* memory = ib_mpool_lite_alloc(ib(), size);
    if (! memory) {
        BOOST_THROW_EXCEPTION(
          ealloc() << errinfo_what(
            "ib_mpool_lite_alloc() returned NULL"
          )
        );
    }
    return memory;
}

void MemoryPoolLite::destroy() const
{
    ib_mpool_lite_destroy(ib());
}

void MemoryPoolLite::register_cleanup(cleanup_t f) const
{
    // We can't use this as the memory pool for value_to_data because then
    // the callback would be deleted before it is called.  The callback
    // itself will free it's own data.
    ib_status_t rc = ib_mpool_lite_register_cleanup(
        ib(),
        ibpp_memory_pool_lite_cleanup,
        value_to_data(f)
    );
    throw_if_error(rc);
}

MemoryPoolLite::MemoryPoolLite(ib_mpool_lite_t* ib_mpool) :
    ConstMemoryPoolLite(ib_mpool),
    m_ib(ib_mpool)
{
    // nop
}

/* Global */

std::ostream& operator<<(std::ostream& o, const ConstMemoryPoolLite& memory_pool)
{
    if (! memory_pool) {
        o << "IronBee::MemoryPoolLite[!singular!]";
    }
    else {
        o << "IronBee::MemoryPoolLite[]";
    }
    return o;
}

/* ScopedMemoryPool */

ScopedMemoryPoolLite::ScopedMemoryPoolLite() :
    m_pool(MemoryPoolLite::create())
{
    // nop
}

ScopedMemoryPoolLite::~ScopedMemoryPoolLite()
{
    m_pool.destroy();
}

};
