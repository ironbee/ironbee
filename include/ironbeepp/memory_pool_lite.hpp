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
 * @brief IronBee++ --- Memory Pool Lite
 *
 * This file defines MemoryPoolLite, a wrapper for ib_mpool_lite_t.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP__MEMORY_POOL_LITE__
#define __IBPP__MEMORY_POOL_LITE__

#include <ironbeepp/abi_compatibility.hpp>
#include <ironbeepp/common_semantics.hpp>

#include <boost/function.hpp>
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdelete-non-virtual-dtor"
#endif
#include <boost/utility.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <ostream>

// IronBee C
typedef struct ib_mpool_lite_t ib_mpool_lite_t;

namespace IronBee {

/**
 * Const Memory Pool Lite; equivalent to a const pointer to ib_mpool_lite_t.
 *
 * Provides operators ==, !=, <, >, <=, >= and evaluation as a boolean for
 * singularity via CommonSemantics.
 *
 * See MemoryPoolLite for discussion of lite memory pools.
 *
 * @sa MemoryPoolLite
 * @sa ironbeepp
 * @sa ib_bytestr_t
 * @nosubgrouping
 **/
class ConstMemoryPoolLite :
    public CommonSemantics<ConstMemoryPoolLite>
{
public:
    //! C Type.
    typedef const ib_mpool_lite_t* ib_type;

    /**
     * Construct singular ConstMemoryPoolLite.
     *
     * All behavior of a singular ConstMemoryPoolLite is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    ConstMemoryPoolLite();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! Non-const ib_mpool_lite_t accessor.
    // Intentionally inlined.
    const ib_mpool_lite_t* ib() const
    {
        return m_ib;
    }

    //! Construct MemoryPools from ib_mpool_lite_t.
    explicit
    ConstMemoryPoolLite(const ib_mpool_lite_t* ib_mpool);

    ///@}

private:
    const ib_mpool_lite_t* m_ib;
};

/**
 * Lite memory pool; equivalent to ib_mpool_lite_t.
 *
 * Lite memory pools can be treated as ConstMemoryPoolLites.  See @ref
 * ironbeepp for details on IronBee++ object semantics.
 *
 * A lite memory pool is similar to a memory pool but is significantly
 * simpler both in interface and implementation.  It is a good choice when
 * only a few allocations are expected, such as for memory pools local to a
 * single function body.
 *
 * If you want RAII semantics for creating memory pools, see
 * ScopedMemoryPoolLite.
 *
 * @sa ironbeepp
 * @sa ib_mpool_lite_t
 * @sa ConstMemoryPoolLite
 * @nosubgrouping
 **/
class MemoryPoolLite :
    public ConstMemoryPoolLite // Slicing is intentional; see apidoc.hpp
{
public:
    //! C Type.
    typedef ib_mpool_lite_t* ib_type;

    /**
     * Remove the constness of a ConstMemoryPoolLite.
     *
     * @warning This is as dangerous as a @c const_cast, use carefully.
     *
     * @param[in] const_pool ConstMemoryPoolLite to remove const from.
     * @returns MemoryPool pointing to same underlying memory pool as
     *          @a const_pool.
     **/
    static MemoryPoolLite remove_const(const ConstMemoryPoolLite& const_pool);

    /**
     * Construct singular MemoryPoolLite.
     *
     * All behavior of a singular MemoryPoolLite is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    MemoryPoolLite();

    /**
     * @name Creation
     * Routines for creating new memory pools.
     *
     * These routines create a new memory pool.  The pool must be explicitly
     * destroyed via destroy().
     *
     * For RAII semantics, see ScopedMemoryPoolLite.
     **/
    //@{

    /**
     * Create MemoryPoolLite.
     *
     * @returns Memory pool lite.
     * @throw Appropriate IronBee++ exception on failure.
     **/
    static MemoryPoolLite create();

    //@}

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

    //@}

    /**
     * Destroy this pool.
     **/
    void destroy() const;

    //! Type of a cleanup handler.
    typedef boost::function<void()> cleanup_t;

    /**
     * Register @a f to be called when the pool is destroyed.
     *
     * @param[in] f Function to call on destruction.
     **/
    void register_cleanup(cleanup_t f) const;

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! Non-const ib_mpool_lite_t accessor.
    // Intentionally inlined.
    ib_mpool_lite_t* ib() const
    {
        return m_ib;
    }

    //! Construct MemoryPoolLites from ib_mpool_lite_t.
    explicit
    MemoryPoolLite(ib_mpool_lite_t* ib_mpool);

    ///@}

private:
    ib_mpool_lite_t* m_ib;
};

/**
 * Enables RAII semantics for MemoryPoolLite.
 *
 * This non-copyable class creates a new MemoryPoolLite on construction and
 * destroys it on destruction.
 *
 * A ScopedMemoryPoolLite can be used anywhere a MemoryPoolLite can be.
 **/
class ScopedMemoryPoolLite :
    boost::noncopyable
{
public:
    /**
     * Constructs memory pool.
     *
     * @throw ealloc on failure.
     **/
    ScopedMemoryPoolLite();
    /**
     * Destroy associated pool.
     **/
    ~ScopedMemoryPoolLite();

    //! Implicit conversion to MemoryPoolLite.
    // Intentionally inlined.
    operator MemoryPoolLite() const
    {
        return m_pool;
    }

private:
    MemoryPoolLite m_pool;
};

//! Ostream output for MemoryPool.
std::ostream& operator<<(std::ostream& o, const ConstMemoryPoolLite& pool);

// Template Definition
template <typename T>
T* MemoryPoolLite::allocate(size_t number) const
{
    return static_cast<T*>(alloc(number * sizeof(T)));
}

};

#endif
