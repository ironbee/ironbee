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
 * A wrapper around a C++ type and assumes another entity will call delete.
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
 */
template< typename T>
class MMPtr {
private:
    T* m_px;

    static void destroy(T* px) {
        delete px;
    }

public:
    MMPtr(): m_px(NULL)
    {
    }

    MMPtr(T* px, MemoryManager mm): m_px(px)
    {
        mm.register_cleanup(boost::bind(&MMPtr::destroy, px));
    }

    MMPtr(boost::shared_ptr<T> ptr) : m_px(ptr.get())
    {
    }

    T* get()
    {
        return m_px;
    }

    T& operator*()
    {
        return *m_px;
    }

    T& operator*() const
    {
        return *m_px;
    }

    T* operator->()
    {
        return m_px;
    }

    T* operator->() const
    {
        return m_px;
    }

    MMPtr<T>& operator=(MMPtr<T> const & ptr)
    {
        m_px = ptr.m_px;
        return *this;
    }

    void reset()
    {
        m_px = NULL;
    }

    void swap(MMPtr<T>& ptr)
    {
        T* px = m_px;
        m_px = ptr.m_px;
        m_px = px;
        return *this;
    }

    operator bool() const
    {
        return get() != 0;
    }
};
}

template<class T, class U>
bool operator==(boost::shared_ptr<T> const & a, boost::shared_ptr<U> const & b)
{
    return a.get() == b.get();
}
template<class T, class U>
bool operator!=(boost::shared_ptr<T> const & a, boost::shared_ptr<U> const & b)
{
    return a.get() != b.get();
}

template<class T, class U>
bool operator<(boost::shared_ptr<T> const & a, boost::shared_ptr<U> const & b)
{
    return a.get() == b.get();
}

#endif // __IBPP__MEMORY_MANAGER_PTR__