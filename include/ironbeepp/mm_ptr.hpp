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
 * @brief IronBee++ --- Memory Manager Ptr Object
 *
 * Associate a C++ object with memory manager for destruction.
 * Utility constructors are provided for inter-operating with other
 * pointer types.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#ifndef __IBPP__MEMORY_MANAGER_PTR__
#define __IBPP__MEMORY_MANAGER_PTR__

#include <ironbeepp/abi_compatibility.hpp>
#include <ironbeepp/memory_manager.hpp>

#ifdef __clang__
#pragma clang diagnostic push
#if __has_warning("-Wunused-local-typedef")
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
#endif
#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

namespace IronBee {


/**
 * A wrapper around a C++ pointer and assumes another entity will call delete.
 *
 * Other pointer implementation take responsibility for destroying the
 * pointer they wrap. MMPtr is different in that it
 * expects another entity, such as a MemoryPool, to do the object
 * destruction. Otherwise, MMPtr operations almost identically to
 * a boost::shared_ptr.
 *
 * This class is desirable for a few reasons, most of which can
 * be classified as "code management." First, if this is a drop-in
 * substitute for boost::smart_ptr, then modifying pointer
 * types will be faster than managing naked pointers.
 * Second, this allows a light abstraction to the concept of a
 * pointer whose lifetime is intentionally managed by some other
 * entity.
 *
 * @tparam T The type of the pointer to be stored.
 */
template< typename T>
class MMPtr {
private:
    /**
     * The pointer.
     */
    T* m_px;

    /**
     * Destroy @a px using the keyword delete.
     *
     * This function may be bound using boost::bind and passed to
     * a memory manager's cleanup list for alter dispatch.
     *
     * @param px A non-null pointer to a C++ object. This object should not
     * be an array.
     */
    static void destroy(T* px) {
        delete px;
    }

public:

    /**
     * Construct a pointer to NULL.
     */
    MMPtr(): m_px(NULL)
    {
    }

    /**
     * Construct a pointer to @a px that is deleted when @a mm is destroyed.
     *
     * @param[in] px The pointer to track.
     * @param[in] mm The destruction of @a px is schedule with this.
     */
    MMPtr(T* px, MemoryManager mm): m_px(px)
    {
        mm.register_cleanup(boost::bind(&MMPtr::destroy, px));
    }

    /**
     * Track the pointer managed by @a ptr.
     *
     * @param[in] ptr The object containing a pointer to track.
     */
    explicit MMPtr(boost::shared_ptr<T>& ptr) : m_px(ptr.get())
    {
    }

    /**
     * Return the tracked pointer.
     * @return the tracked pointer.
     */
    T* get()
    {
        return m_px;
    }

    /**
     * Return a reference to the object pointed to in this object.
     * @return a reference to the object pointed to in this object.
     */
    T& operator*()
    {
        return *m_px;
    }

    /**
     * Return a reference to the object pointed to in this object.
     * @return a reference to the object pointed to in this object.
     */
    T& operator*() const
    {
        return *m_px;
    }

    /**
     * Return the pointer to the object pointed to in this object.
     * @return the pointer to the object pointed to in this object.
     */
    T* operator->()
    {
        return m_px;
    }

    /**
     * Return the pointer to the object pointed to in this object.
     * @return the pointer to the object pointed to in this object.
     */
    T* operator->() const
    {
        return m_px;
    }

    /**
     * Assign the pointer of another object to this one.
     * @param[in] ptr The other point to assign to this one.
     * @return *this.
     */
    MMPtr<T>& operator=(MMPtr<T> const & ptr)
    {
        m_px = ptr.m_px;
        return *this;
    }

    /**
     * Set this object to point to NULL.
     */
    void reset()
    {
        m_px = NULL;
    }

    /**
     * Exchange the pointer stored in @a ptr with the one stored in this.
     *
     * @param[in] ptr The holder of the pointer to exchange with the pointer
     *            stored in this.
     */
    void swap(MMPtr<T>& ptr)
    {
        T* px = m_px;
        m_px = ptr.m_px;
        m_px = px;
        return *this;
    }

    /**
     * Return false if `get()` returns 0 (NULL).
     * @return false if `get()` returns 0 (NULL).
     */
    operator bool() const
    {
        return get() != 0;
    }
};
}

/**
 * Test if the pointers stored by @a a and @a b are the same.
 *
 * @param[in] a A pointer to compare.
 * @param[in] b A pointer to compare.
 *
 * @return True if `a.get() == b.get()`, false otherwise.
 */
template<class T, class U>
bool operator==(IronBee::MMPtr<T> const & a, IronBee::MMPtr<U> const & b)
{
    return a.get() == b.get();
}

/**
 * Test if the pointers stored by @a a and @a b are different.
 *
 * @param[in] a A pointer to compare.
 * @param[in] b A pointer to compare.
 *
 * @return True if `a.get() != b.get()`, true otherwise.
 */
template<class T, class U>
bool operator!=(IronBee::MMPtr<T> const & a, IronBee::MMPtr<U> const & b)
{
    return a.get() != b.get();
}

/**
 * Compare if @a a is less then @a b.
 *
 * @param[in] a A pointer to compare.
 * @param[in] b A pointer to compare.
 *
 * @return True if `a.get() < b.get()`, false otherwise.
 */
template<class T, class U>
bool operator<(IronBee::MMPtr<T> const & a, IronBee::MMPtr<U> const & b)
{
    return a.get() < b.get();
}

#endif // __IBPP__MEMORY_MANAGER_PTR__