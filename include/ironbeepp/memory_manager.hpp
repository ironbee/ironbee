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
 * @brief IronBee++ --- Memory Manager
 *
 * This file defines MemoryManager, a wrapper for ib_mm_t.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP__MEMORY_MANAGER__
#define __IBPP__MEMORY_MANAGER__

#include <ironbeepp/abi_compatibility.hpp>

#include <ironbee/mm.h>
#include <ironbee/mm_mpool.h>
#include <ironbee/mm_mpool_lite.h>

#include <boost/function.hpp>

#include <ostream>

namespace IronBee {

class MemoryPool;
class ScopedMemoryPool;
class MemoryPoolLite;
class ScopedMemoryPoolLite;

/**
 * Memory Manager; equivalent to a *value* of @ref ib_mm_t.
 *
 * Note: As a by-value, MemoryManager does not provide common semantics.
 *
 * A MemoryManager is a simple interface to memory management systems.  It
 * requires implementation of allocation and cleanup function registration.
 *
 * @sa ironbeepp
 * @sa ib_mm_t
 * @nosubgrouping
 **/
class MemoryManager
{
public:
    //! C Type.
    typedef ib_mm_t ib_type;

    /**
     * @name Construction.
     * Methods to access consturct MemoryManagers.
     **/
    ///@{

    //! Default constructor.  Singular.
    MemoryManager();

    //! Construct from C type.
    explicit
    MemoryManager(ib_type ib);

    //! Allocation function.
    typedef boost::function<void*(size_t)> alloc_t;
    //! Cleanup function.
    typedef boost::function<void()> cleanup_t;
    //! Cleanup registration function.
    typedef boost::function<void(cleanup_t)> register_cleanup_t;

    //! Constructor from functionals.
    MemoryManager(alloc_t alloc, register_cleanup_t register_cleanup);

    //! Conversion from MemoryPool. Implicit.
    MemoryManager(MemoryPool memory_pool);
    //! Conversion from MemoryPoolLite.  Implicit.
    MemoryManager(MemoryPoolLite memory_pool_lite);
    //! Conversion from ScopedMemoryPool. Implicit.
    MemoryManager(ScopedMemoryPool& memory_pool);
    //! Conversion from ScopedMemoryPoolLite.  Implicit.
    MemoryManager(ScopedMemoryPoolLite& memory_pool_lite);


    ///@}

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! @ref ib_mm_t accessor.  Note copy-out.
    ib_mm_t ib() const
    {
        return m_ib;
    }

    ///@}

    /**
     * @name Allocation
     * Routines to allocate memory.
     **/
    //@{

    /**
     * Allocate sufficient memory for a @a number @a Ts
     *
     * Note: This does not construct any Ts, simply allocates memory.  You
     * will need to use placement new to construct the T.
     *
     * @tparam T Type to allocate memory for.
     * @param[in] number Number of Ts.
     * @returns Pointer to allocated memory.
     * @throw ealloc on failure to allocate.
     **/
    template <typename T>
    T* allocate(size_t number = 1) const;

    /**
     * Allocate @a size bytes of memory.
     *
     * @param[in] size Number of bytes to allocate.
     * @returns Pointer to allocated memory.
     * @throw ealloc on failure to allocate.
     **/
    void* alloc(size_t size) const;

    /**
     * Allocate @a count * @a size bytes of memory and sets to 0.
     *
     * @param[in] count Number of elements to allocate memory for.
     * @param[in] size  Size of each element.
     * @returns Pointer to allocated memory.
     * @throw ealloc on failure to allocate.
     **/
    void* calloc(size_t count, size_t size) const;

    /**
     * Allocate @a size bytes and set to 0.
     * @param[in] size  Size of each element.
     * @returns Pointer to allocated memory.
     * @throw ealloc on failure to allocate.
     **/
    void* calloc(size_t size) const;

    /**
     * Duplicate a C string.
     * @param[in] cstr C string to duplicate.
     * @returns Pointer to copy of @a cst.
     * @throw ealloc on failure to allocate.
     **/
    char* strdup(const char* cstr) const;

    /**
     * Duplicate a region of memory.
     * @param[in] data Data to duplicate.
     * @param[in] size Size of @a data.
     * @throw ealloc on failure to allocate.
     **/
    void* memdup(const void* data, size_t size) const;

    /**
     * Duplicate a region to memory, adding a NUL at the end.
     * @param[in] data Data to duplicate.
     * @param[in] size Size of @a data.
     * @throw ealloc on failure to allocate.
     **/
    char* memdup_to_str(const void* data, size_t size) const;

    //@}

    /**
     * Register a function to be called at memory destruction.
     *
     * Cleanup functions are called in reverse order of registration and
     * before memory is released.
     *
     * @param[in] cleanup Function to register.
     * @throw ealloc on Allocation failure.
     **/
    void register_cleanup(cleanup_t cleanup) const;

    ///@cond Internal
    typedef void (*unspecified_bool_type)(MemoryManager***);
    ///@endcond

    /**
     * Test singularity.
     *
     * Evaluates as truthy if ! ib_mm_is_null(this->ib()).  Does so in a way
     * that prevents implicit conversion to integral types.
     *
     * @returns truthy iff ! ib_mm_is_null(this->ib()).
     **/
    operator unspecified_bool_type() const;

private:
    //! Used for unspecified_bool_type.
    static void unspecified_bool(MemoryManager***) {};

    //! Underlying @ref ib_mm_t.
    ib_mm_t m_ib;
};

//! Ostream output for MemoryPool.
std::ostream& operator<<(
    std::ostream& o,
    const MemoryManager& memory_manager
);

// Template Definition
template <typename T>
T* MemoryManager::allocate(size_t number) const
{
    return static_cast<T*>(alloc(number * sizeof(T)));
}

} // IronBee

#endif
