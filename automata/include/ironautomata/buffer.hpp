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

#ifndef _IA_BUFFER_
#define _IA_BUFFER_

/**
 * @file
 * @brief IronAutomata --- Buffer Support
 *
 * This header file defines a buffer type and code to build buffers.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <algorithm>
#include <string>
#include <vector>

#include <stdint.h>

namespace IronAutomata {

/**
 * Buffer of bytes.
 *
 * IronAutomata uses vector<char> as its buffer type.  This has means that,
 * during construction, the vector is periodically enlarged and data copied.
 * Besides time and space costs, this behavior means that pointers retrieved
 * from a buffer are not stable across buffer expansion.  To deal with this,
 * buffer indexes are used.
 */
typedef std::vector<char> buffer_t;

/**
 * Build buffers up incrementally.
 *
 * The BufferAssembler wraps a buffer_t and provides routines useful for
 * building buffers up over time.
 */
class BufferAssembler
{
public:
    /**
     * Constructor.
     *
     * @param[in] buffer Reference to buffer to assemble.
     */
    explicit
    BufferAssembler(buffer_t& buffer);

    /**
     * Alias for buffer().size().
     *
     * Intentionally inlined.
     *
     * @return Size of buffer.
     */
    size_t size() const
    {
        return m_buffer.size();
    }

    /**
     * Buffer accessor.
     *
     * Intentionally inlined.
     *
     * @return Buffer.
     */
    buffer_t& buffer()
    {
        return m_buffer;
    }

    /**
     * Buffer accessor.
     *
     * Intentionally inlined.
     *
     * @return Buffer.
     */
    const buffer_t& buffer() const
    {
        return m_buffer;
    }

    /**
     * Convert a pointer into the buffer into an index.
     *
     * Unlike pointers (see ptr()), indexes are stable across appending to
     * the  buffer, whereas pointers are not.  If you need to maintain a
     * reference across append operations, convert to an index and use ptr()
     * to convert back to a pointer when needed.
     *
     * @note Does not do range checking.
     *
     * Intentionally inlined.
     *
     * @tparam T Type of @a p.
     * @param[in] p Valid pointer into buffer.
     * @return Index of location in buffer with @a p points.
     */
    template <typename T>
    size_t index(const T* p) const
    {
        return reinterpret_cast<const char*>(p) - buffer().data();
    }

    /**
     * Convert an index into a pointer.
     *
     * Pointers are not stable across appending to the buffer.  Use index()
     * to get a stable reference.
     *
     * @note Does not do range checking.
     *
     * Intentionally inlined.
     * @tparam T Type of return.
     * @param[in] i Index of location of buffer.
     * @return Pointer of type @c T* to location at @a i.
     */
    template <typename T>
    T* ptr(size_t i)
    {
        return reinterpret_cast<T*>(&buffer()[i]);
    }

    /**
     * Add @a n bytes of capacity to end of buffer.
     *
     * May invalidate all pointers into buffer.
     *
     * @param[in] n Number of bytes to add.
     * @return Index of first new byte.
     */
    size_t extend(size_t n);

    /**
     * Add bytes of any object to end of buffer.
     *
     * @tparam T Type of @a object.
     * @param[in] object Object to append to buffer.
     * @return Pointer to copy.
     */
    template <typename T>
    T* append_object(const T& object);

    /**
     * Add a zero-initialized array of @a T to end of buffer.
     *
     * @tparam T Type of element of array.
     * @param[in] n     Number of elements in array.
     * @return Pointer to beginning of array in buffer.
     */
    template <typename T>
    T* append_array(size_t n);

    /**
     * Append string to buffer.  Does not NUL terminate.
     *
     * @param[in] s String to append.
     * @return Pointer to first byte of string in buffer.
     */
    char* append_string(const std::string& s);

    /**
     * Append bytes to buffer.
     *
     * @param[in] v Bytes to append.
     * @param[in] n Number of bytes.
     * @return Pointer to first byte of bytes in buffer.
     */
    uint8_t* append_bytes(const uint8_t* v, size_t n);

private:
    buffer_t& m_buffer;
};

/* Implementation of templates */

template <typename T>
T* BufferAssembler::append_object(const T& object)
{
    size_t i = extend(sizeof(object));

    const char* src = reinterpret_cast<char const *>(&object);
    m_buffer.insert(m_buffer.begin() + i, src, src + sizeof(object));

    return ptr<T>(i);
}

template <typename T>
T* BufferAssembler::append_array(size_t n)
{
    size_t i = size();
    m_buffer.resize(i + n * sizeof(T), 0);

    return ptr<T>(i);
}

} // IronAutomata

#endif
