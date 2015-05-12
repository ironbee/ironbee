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
 * @brief IronBee++ Internals --- Memory Manager Implementation
 *
 * @sa memory_manager.hpp
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbeepp/memory_manager.hpp>

#include <ironbeepp/c_trampoline.hpp>
#include <ironbeepp/catch.hpp>
#include <ironbeepp/data.hpp>
#include <ironbeepp/memory_pool.hpp>
#include <ironbeepp/memory_pool_lite.hpp>
#include <ironbeepp/throw.hpp>

#include <ironbee/mm.h>

#ifdef __clang__
#pragma clang diagnostic push
#if __has_warning("-Wunused-local-typedef")
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
#endif
#include <boost/bind.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <cassert>

using namespace std;

namespace IronBee {

extern "C" {

ib_status_t ibpp_mm_register_cleanup(
    ib_mm_cleanup_fn_t fn,
    void*              fndata,
    void*              cbdata
)
{
    try {
        data_to_value<MemoryManager::register_cleanup_t>(cbdata)(
            boost::bind(fn, fndata)
        );
    }
    catch (...) {
        return convert_exception();
    }
    return IB_OK;
}

} // extern "C"

MemoryManager::MemoryManager() :
    m_ib(IB_MM_NULL)
{
    // nop
}

MemoryManager::MemoryManager(ib_type ib) :
    m_ib(ib)
{
    // nop
}

MemoryManager::MemoryManager(
    alloc_t            alloc,
    register_cleanup_t register_cleanup
)
{
    pair<ib_mm_alloc_fn_t, void*> alloc_trampoline =
        make_c_trampoline<void*(size_t)>(alloc);
    void* register_cleanup_data = value_to_data(register_cleanup);

    m_ib.alloc = alloc_trampoline.first;
    m_ib.alloc_data = alloc_trampoline.second;
    m_ib.register_cleanup = &ibpp_mm_register_cleanup;
    m_ib.register_cleanup_data = register_cleanup_data;

    // Important that cleanups are called in reverse order.  The last cleanup
    // will then destroy the data that we use to call into it, but only after
    // we've already used that data to call into it.
    register_cleanup(
        boost::bind(Internal::ibpp_data_cleanup, register_cleanup_data)
    );
    register_cleanup(
        boost::bind(delete_c_trampoline, alloc_trampoline.second)
    );
}

MemoryManager::MemoryManager(MemoryPool memory_pool) :
    m_ib(ib_mm_mpool(memory_pool.ib()))
{
    // nop
}

MemoryManager::MemoryManager(ScopedMemoryPool& memory_pool) :
    m_ib(ib_mm_mpool(MemoryPool(memory_pool).ib()))
{
    // nop
}

MemoryManager::MemoryManager(MemoryPoolLite memory_pool_lite) :
    m_ib(ib_mm_mpool_lite(memory_pool_lite.ib()))
{
    // nop
}

MemoryManager::MemoryManager(ScopedMemoryPoolLite& memory_pool_lite) :
    m_ib(ib_mm_mpool_lite(MemoryPoolLite(memory_pool_lite).ib()))
{
    // nop
}


void* MemoryManager::alloc(size_t size) const
{
    return ib_mm_alloc(ib(), size);
}

void* MemoryManager::calloc(size_t count, size_t size) const
{
    return ib_mm_calloc(ib(), count, size);
}

void* MemoryManager::calloc(size_t size) const
{
    return calloc(1, size);
}

char* MemoryManager::strdup(const char* cstr) const
{
    return ib_mm_strdup(ib(), cstr);
}

void* MemoryManager::memdup(const void* data, size_t size) const
{
    return ib_mm_memdup(ib(), data, size);
}

char* MemoryManager::memdup_to_str(const void* data, size_t size) const
{
    return ib_mm_memdup_to_str(ib(), data, size);
}

void MemoryManager::register_cleanup(cleanup_t cleanup) const
{
    pair<ib_mm_cleanup_fn_t, void*> cleanup_trampoline =
        make_c_trampoline<void()>(cleanup);

    // register first so its called after being used.
    throw_if_error(ib_mm_register_cleanup(
        ib(),
        delete_c_trampoline, cleanup_trampoline.second
    ));
    throw_if_error(ib_mm_register_cleanup(
        ib(),
        cleanup_trampoline.first, cleanup_trampoline.second
    ));
}

MemoryManager::operator unspecified_bool_type() const
{
    return ib_mm_is_null(ib()) ? NULL : unspecified_bool;
}

std::ostream& operator<<(
    std::ostream& o,
    const MemoryManager& memory_manager
)
{
    o << "MemoryManager" << (memory_manager ? "[]" : "[singular!]");
    return o;
}

} // IronBee
