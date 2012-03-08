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
 * @brief IronBee++ &mdash; Memory Pool
 *
 * This file defines MemoryPool, a wrapper for ib_mpool_t.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP__MEMORY_POOL__
#define __IBPP__MEMORY_POOL__

#include <boost/function.hpp>
#include <boost/operators.hpp>
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
 * Memory pool; equivalent to ib_mpool_t.
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
 **/
class MemoryPool :
    boost::less_than_comparable<MemoryPool>,
    boost::equality_comparable<MemoryPool>
{
public:
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
     * @param[in] size Page size if non-0; otherwise uses 1024 byte pages.
     * @returns Memory pool.
     * @throw Appropriate IronBee++ exception on failure.
     **/
    static MemoryPool create(
        const char* name,
        size_t      size = 0
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
     * @param[in] size Page size if non-0; otherwise uses 1024 byte pages.
     * @returns Memory pool.
     * @throw Appropriate IronBee++ exception on failure.
     **/
    static MemoryPool create(
        const char* name,
        MemoryPool& parent,
        size_t      size = 0
    );

    /**
     * Create a subpool that will be destroyed when this is destroyed.
     *
     * Name is "SubPool" and page size is default.
     *
     * @returns Memory pool.
     * @throw Appropriate IronBee++ exception on failure.
     **/
    MemoryPool create_subpool();

    /**
     * Create a subpool that will be destroyed when this is destroyed.
     *
     * @param[in] subpool_name Name of pool; used for debugging.
     * @param[in] size         Page size if non-0; otherwise uses 1024 byte
     *                         pages.
     * @returns Memory pool.
     * @throw Appropriate IronBee++ exception on failure.
     **/
    MemoryPool create_subpool(
        const char* subpool_name,
        size_t      size = 0
    );
    //@}

    /**
     * The name of the memory pool.
     *
     * @returns Name or NULL if no name is set.
     **/
    const char* name() const;

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
    T* allocate(size_t number);

    /**
     * Allocate @a size bytes of memory.
     *
     * @param[in] size Number of bytes to allocate.
     * @returns Pointer to allocated memory.
     * @throw ealloc on failure to allocate.
     **/
    void* alloc(size_t size);

    /**
     * Allocate @a count * @a size bytes of memory and sets to 0.
     *
     * @param[in] count Number of elements to allocate memory for.
     * @param[in] size  Size of each element.
     * @returns Pointer to allocated memory.
     * @throw ealloc on failure to allocate.
     **/
    void* calloc(size_t count, size_t size);

    /**
     * Allocate @a size bytes and set to 0.
     * @param[in] size  Size of each element.
     * @returns Pointer to allocated memory.
     * @throw ealloc on failure to allocate.
     **/
    void* calloc(size_t size);
    //@}

    /**
     * Deallocate all memory associated with this pool and all child pools.
     **/
    void clear();

    /**
     * Destroy this pool and all child pools.
     **/
    void destroy();

    //! Type of a cleanup handler.
    typedef boost::function<void()> cleanup_t;

    /**
     * Register @a f to be called when the pool is destroyed.
     *
     * @param[in] f Function to call on destruction.
     **/
    void register_cleanup(cleanup_t f);

    /// @cond Internal
    typedef void (*unspecified_bool_type)(MemoryPool***);
    /// @endcond
    /**
     * Is not singular?
     *
     * This operator returns a type that converts to bool in appropriate
     * circumstances and is true iff this object is not singular.
     *
     * @returns true iff is not singular.
     **/
    operator unspecified_bool_type() const;

    /**
     * Equality operator.  Do they refer to the same underlying module.
     *
     * Two MemoryPools are considered equal if they refer to the same
     * underlying ib_mpool_t.
     *
     * @param[in] other MemoryPools to compare to.
     * @return true iff other.ib() == ib().
     **/
    bool operator==(const MemoryPool& other) const;

    /**
     * Less than operator.
     *
     * MemoryPools are totally ordered with all singular MemoryPools as the
     * minimal element.
     *
     * @param[in] other MemoryPools to compare to.
     * @return true iff this and other are singular or  ib() < other.ib().
     **/
    bool operator<(const MemoryPool& other) const;

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! Non-const ib_mpool_t accessor.
    ib_mpool_t*       ib();

    //! Const ib_mpool_t accessor.
    const ib_mpool_t* ib() const;

    //! Construct MemoryPools from ib_mpool_t.
    explicit
    MemoryPool(ib_mpool_t* ib_mpool);

    ///@}

private:
    ib_mpool_t* m_ib;

    // Used for unspecified_bool_type.
    static void unspecified_bool(MemoryPool***) {};
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
     * @param[in] size Page size if non-0; otherwise uses 1024 byte pages.
     * @throw Appropriate IronBee++ exception on failure.
     **/
    explicit
    ScopedMemoryPool(const char* name, size_t size = 0);

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
std::ostream& operator<<(std::ostream& o, const MemoryPool& pool);

// Template Definition
template <typename T>
T* MemoryPool::allocate(size_t number)
{
    return static_cast<T*>(alloc(number * sizeof(T)));
}

};

#endif
