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
 * @brief IronBee++ --- Memory Pool
 *
 * This file defines MemoryPool, a wrapper for ib_mpool_t.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP__MEMORY_POOL__
#define __IBPP__MEMORY_POOL__

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
typedef struct ib_mpool_t ib_mpool_t;

namespace IronBee {

/**
 * Const Memory Pool; equivalent to a const pointer to ib_mpool_t.
 *
 * Provides operators ==, !=, <, >, <=, >= and evaluation as a boolean for
 * singularity via CommonSemantics.
 *
 * See MemoryPool for discussion of memory pools.
 *
 * @sa MemoryPool
 * @sa ironbeepp
 * @sa ib_bytestr_t
 * @nosubgrouping
 **/
class ConstMemoryPool :
    public CommonSemantics<ConstMemoryPool>
{
public:
    //! C Type.
    typedef const ib_mpool_t* ib_type;

    /**
     * Construct singular ConstMemoryPool.
     *
     * All behavior of a singular ConstMemoryPool is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    ConstMemoryPool();

    /**
     * The name of the memory pool.
     *
     * @returns Name or NULL if no name is set.
     **/
    const char* name() const;

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! Non-const ib_mpool_t accessor.
    // Intentionally inlined.
    const ib_mpool_t* ib() const
    {
        return m_ib;
    }

    //! Construct MemoryPools from ib_mpool_t.
    explicit
    ConstMemoryPool(const ib_mpool_t* ib_mpool);

    ///@}

private:
    const ib_mpool_t* m_ib;
};

/**
 * Memory pool; equivalent to ib_mpool_t.
 *
 * Memory Pools can be treated as ConstMemoryPools.  See @ref ironbeepp for
 * details on IronBee++ object semantics.
 *
 * IronBee makes frequent use of memory pools to manage memory and object
 * lifetime.  The engine, each transaction, context, etc. all have associated
 * memory pools that are used to allocate memory for objects whose lifetime
 * is a subset of the memory pool holder.  This class represents such a
 * memory pool and provides low level routines.
 *
 * As per the IronBee++ reference semantics, an object of this class is best
 * viewed as a reference to a MemoryPool, i.e., as equivalent to
 * @c ib_mpool_t* rather than ib_mpool_t.  As such, they can not be directly
 * created.  Similarly, destroying one does not destroy the underlying
 * memory pool.  There are static create() methods available to create new
 * memory pools and a destroy() method to destroy them.
 *
 * There is no requirement that you use MemoryPool to allocate memory for
 * your own C++ objects.  And doing so has risks as the (C) memory pool
 * implementation is not aware of destructors and will not call them on
 * pool destruction.
 *
 * If your goal is to do cleanup tasks when a memory pool is destroyed, use
 * register_cleanup().
 *
 * Unfortunately, the semantics of IronBee memory pools and the stringent
 * requirements on standard allocators makes it impossible to write a
 * conforming standard allocator based on MemoryPool.
 *
 * If you want RAII semantics for creating memory pools, see ScopedMemoryPool.
 *
 * @sa ironbeepp
 * @sa ib_mpool_t
 * @sa ConstMemoryPool
 * @nosubgrouping
 **/
class MemoryPool :
    public ConstMemoryPool // Slicing is intentional; see apidoc.hpp
{
public:
    //! C Type.
    typedef ib_mpool_t* ib_type;

    /**
     * Remove the constness of a ConstMemoryPool.
     *
     * @warning This is as dangerous as a @c const_cast, use carefully.
     *
     * @param[in] const_pool ConstMemoryPool to remove const from.
     * @returns MemoryPool pointing to same underlying memory pool as
     *          @a const_pool.
     **/
    static MemoryPool remove_const(const ConstMemoryPool& const_pool);

    /**
     * Construct singular MemoryPool.
     *
     * All behavior of a singular MemoryPool is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    MemoryPool();

    /**
     * @name Creation
     * Routines for creating new memory pools.
     *
     * These routines create a new memory pool.  The pool must be explicitly
     * destroyed via destroy().
     *
     * For RAII semantics, see ScopedMemoryPool.
     **/
    //@{

    /**
     * Create MemoryPool with default settings.
     *
     * Creates a memory pool with name "MemoryPool", no parent, and the
     * default page size.
     *
     * @returns Memory pool.
     * @throw Appropriate IronBee++ exception on failure.
     **/
    static MemoryPool create();

    /**
     * Create MemoryPool.
     *
     * @param[in] name Name of pool; used for debugging.
     * @returns Memory pool.
     * @throw Appropriate IronBee++ exception on failure.
     **/
    static MemoryPool create(
        const char* name
    );

    /**
     * Create MemoryPool with parent.
     *
     * The MemoryPool will be destroyed when the parent is destroyed.
     *
     * @sa create_subpool()
     *
     * @param[in] name Name of pool; used for debugging.
     * @param[in] parent Parent memory pool.
     * @returns Memory pool.
     * @throw Appropriate IronBee++ exception on failure.
     **/
    static MemoryPool create(
        const char* name,
        MemoryPool  parent
    );

    /**
     * Create a subpool that will be destroyed when this is destroyed.
     *
     * Name is "SubPool" and page size is default.
     *
     * @returns Memory pool.
     * @throw Appropriate IronBee++ exception on failure.
     **/
    MemoryPool create_subpool() const;

    /**
     * Create a subpool that will be destroyed when this is destroyed.
     *
     * @param[in] subpool_name Name of pool; used for debugging.
     * @returns Memory pool.
     * @throw Appropriate IronBee++ exception on failure.
     **/
    MemoryPool create_subpool(
        const char* subpool_name
    ) const;
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
     * Deallocate all memory associated with this pool and all child pools.
     **/
    void clear() const;

    /**
     * Destroy this pool and all child pools.
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

    //! Non-const ib_mpool_t accessor.
    // Intentionally inlined.
    ib_mpool_t* ib() const
    {
        return m_ib;
    }

    //! Construct MemoryPools from ib_mpool_t.
    explicit
    MemoryPool(ib_mpool_t* ib_mpool);

    ///@}

private:
    ib_mpool_t* m_ib;
};

/**
 * Enables RAII semantics for MemoryPool.
 *
 * This non-copyable class creates a new MemoryPool on construction and
 * destroys it on destruction.
 *
 * ScopedMemoryPools can not have parents as their destruction is exactly
 * bound to the object destruction.
 *
 * A ScopedMemoryPool can be used anywhere a MemoryPool can be.
 **/
class ScopedMemoryPool :
    boost::noncopyable
{
public:
    /**
     * Constructs memory pool with name "ScopedMemoryPool" and default page
     * size.
     *
     * @throw ealloc on failure.
     **/
    ScopedMemoryPool();

    /**
     * Construct memory pool.
     *
     * @param[in] name Name of pool; used for debugging.
     * @throw Appropriate IronBee++ exception on failure.
     **/
    explicit
    ScopedMemoryPool(const char* name);

    /**
     * Destroy associated pool.
     **/
    ~ScopedMemoryPool();

    //! Implicit conversion to MemoryPool.
    // Intentionally inlined.
    operator MemoryPool() const
    {
        return m_pool;
    }

private:
    MemoryPool m_pool;
};

//! Ostream output for MemoryPool.
std::ostream& operator<<(std::ostream& o, const ConstMemoryPool& pool);

// Template Definition
template <typename T>
T* MemoryPool::allocate(size_t number) const
{
    return static_cast<T*>(alloc(number * sizeof(T)));
}

};

#endif
