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
 * @brief IronBee++ --- ByteString
 *
 * This file defines (Const)ByteString, a wrapper for ib_bytestr_t.
 *
 * @remark Developers should be familiar with @ref ironbeepp to understand
 * aspects of this code, e.g., the public/non-virtual inheritance.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP__BYTE_STRING__
#define __IBPP__BYTE_STRING__

#include <ironbeepp/abi_compatibility.hpp>
#include <ironbeepp/common_semantics.hpp>
#include <ironbeepp/exception.hpp>
#include <ironbeepp/memory_manager.hpp>

#include <ostream>

// IronBee C
typedef struct ib_bytestr_t ib_bytestr_t;

namespace IronBee {

class ByteString;

/**
 * Const Byte String; equivalent to a const pointer to ib_bytestr_t.
 *
 * Provides operators ==, !=, <, >, <=, >= and evaluation as a boolean for
 * singularity via CommonSemantics.
 *
 * See ByteString for discussion of byte strings.
 *
 * @sa ByteString
 * @sa ironbeepp
 * @sa ib_bytestr_t
 * @nosubgrouping
 **/
class ConstByteString :
    public CommonSemantics<ConstByteString>
{
public:
    //! C Type.
    typedef const ib_bytestr_t* ib_type;

    /**
     * Construct singular ConstByteString.
     *
     * All behavior of a singular ConstByteString is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    ConstByteString();

    /**
     * @name Creation
     * Routines for creating new byte strings.
     *
     * These routines create new byte strings.  The byte strings are destroyed
     * when the corresponding memory manager is cleared or destroyed.
     **/
    /// @{

    /**
     * Create a (read-only) alias of @c this.
     *
     * This creates a new byte string that is an alias of @c this.  The new
     * byte string will share the same data as @c this.  As such, any changes
     * to @c this will be reflected in the alias.
     *
     * The alias is read only.
     *
     * @param[in] mm Memory manager to allocate memory from.
     * @returns Alias
     **/
    ByteString alias(MemoryManager mm) const;
    //! As above, but use same memory manager.
    ByteString alias() const;

    /**
     * Create copy using @a mm.
     *
     * Creates a new byte string using @a mm to allocate memory and set
     * contents to a copy of byte string.
     *
     * @param[in] mm Memory manager to allocate memory from.
     * @returns New byte string with copy of @a this' data.
     * @throws IronBee++ exception on any error.
     **/
    ByteString dup(MemoryManager mm) const;
    //! As above, but use same memory manager.
    ByteString dup() const;
    /// @}

    /**
     * @name Queries
     * Query aspects of the byte string.
     **/
    ///@{
    /**
     * Create string version.
     *
     * This will create a string that is a copy of the byte string.
     *
     * @returns string with same content as @c this.
     **/
    std::string to_s() const;

    /**
     * Memory manager.
     *
     * @returns Memory manager used.
     **/
    MemoryManager memory_manager() const;

    /**
     * Is read-only?
     *
     * Byte strings include a run-time check of mutability.  Read-only byte
     * strings can not be mutated.  Trying to do so will result in an
     * exception.
     *
     * Note that this is run-time (vs. compile-time) constness.
     * @returns true iff read-only.
     **/
    bool read_only() const;

    /**
     * Length of data.
     *
     * This is the actual length of the data.  For the amount of memory
     * allocated, use size().
     *
     * @sa size()
     * @returns Length of data.
     **/
    size_t length() const;

    /**
     * Amount of memory allocated for data.
     *
     * This is the amount of memory currently allocated for data.  It is at
     * least as large as length() but may be larger.
     *
     * @sa length()
     * @returns Size of data allocation.
     **/
    size_t size() const;

    /**
     * Underlying data.
     *
     * A const pointer to the underlying data.  Always available.
     *
     * Zero length byte-strings are allowed to return NULL, but not required
     * to.
     *
     * @returns Pointer to data or, possibly, NULL if zero-length.
     **/
    const char* const_data() const;
    ///@}

    /**
     * @name Algorithms
     * Algorithms involving the byte string.
     **/
    /// @{

    /**
     * Returns index of @a cstring in byte string.
     *
     * This uses a naive algorithm that has no preprocessing time but a large
     * matching time.  If you will be doing many searches of the byte string,
     * consider alternatives.
     *
     * @param[in] cstring String to search for.
     * @returns index of @a cstring or -1 if not a substring.
     **/
    int index_of(const char* cstring) const;

    //! As above, but for a string.
    int index_of(const std::string& s) const;

    /// @}

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! const ib_bytestr_t accessor.
    // Intentionally inlined.
    const ib_bytestr_t* ib() const
    {
        return m_ib;
    }

    //! Construct ByteString from ib_bytestr_t.
    explicit
    ConstByteString(const ib_bytestr_t* ib_bytestr);

    ///@}

private:
    const ib_bytestr_t* m_ib;
};

/**
 * Byte String; equivalent to a pointer to ib_bytestr_t.
 *
 * ByteStrings can be treated as ConstByteStrings.  See @ref ironbeepp for
 * details on IronBee++ object semantics.
 *
 * IronBee uses byte strings to represent sequences of bytes (possibly
 * including NULLs).  They are, fundamentally, a pointer to data and the
 * length of that data.  They are intended to be used with memory managers
 * controlling data lifetime.  As such, they have a variety of routines for
 * aliasing (vs. copying) regions of memory.
 *
 * An additional functionality of bytestring is runtime support for
 * non-mutability.  That is, a byte string can be marked, at run time, as
 * read only and this will be enforced.  Note that a read only bytestring
 * only prevents mutation of its data through itself.  It is quite possible
 * that other code with different access to the data will still change it.
 *
 * The C API uses @c uint8_t* as the type of the data.  In the C++ API,
 * @c char* is used instead.  This is because the most natural analogue to
 * byte strings in C++ is std::string which uses a @c char* data type (e.g.,
 * std::string::data() returns a @c const @c char*).  Using @c char* instead
 * of uint8_t* allows for easier interoperability.
 *
 * It is recommended that you do use byte strings to alias C++ managed
 * memory.  While doing so is possible, you must then insure that the memory
 * lifetime exceeds the lifetime of any byte strings aliasing it in the
 * Engine.  A task that is difficult without a thorough understanding of the
 * Engine internals.  Instead, create a bytestring using a memory mm and
 * copy your data into it.  E.g., with ByteString::create().
 *
 * @sa ironbeepp
 * @sa ib_bytestr_t
 * @sa ConstByteString
 * @nosubgrouping
 **/
class ByteString :
    public ConstByteString // Slicing is intentional; see apidoc.hpp
{
public:
    //! C Type.
    typedef ib_bytestr_t* ib_type;

    /**
     * Remove the constness of a ConstByteString.
     *
     * @warning This is as dangerous as a @c const_cast, use carefully.
     *
     * @param[in] bs ConstByteString to remove const from.
     * @returns ByteString pointing to same underlying byte string as @a bs.
     **/
    static ByteString remove_const(ConstByteString bs);

    /**
     * Construct singular ByteString.
     *
     * All behavior of a singular ByteString is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    ByteString();

    /**
     * @name Creation
     * Routines for creating new byte strings.
     *
     * These routines create new byte strings.  The byte strings are destroyed
     * when the corresponding memory mm is cleared or destroyed.
     **/
    /// @{

    /**
     * Create byte string using @a mm.
     *
     * Creates a new byte string using @a mm to allocate memory.  Byte
     * string will last as long as the underlying memory mm does.
     *
     * @param[in] mm Memory manager to allocate memory from.
     * @returns New empty byte string.
     * @throws IronBee++ exception on any error.
     **/
    static ByteString create(MemoryManager mm);

    /**
     * Create copy of @a data using @a mm.
     *
     * Creates a new byte string using @a mm to allocate memory and set
     * contents to a copy of @a data.
     *
     * @param[in] mm     Memory manager to allocate memory from.
     * @param[in] data   Data to copy into byte string.
     * @param[in] length Length of @a data.
     * @returns New byte string with copy of @a data.
     * @throws IronBee++ exception on any error.
     **/
    static
    ByteString create(
        MemoryManager mm,
        const char*   data,
        size_t        length
    );

    /**
     * Create copy of @a cstring using @a mm.
     *
     * Creates a new byte string using @a mm to allocate memory and set
     * contents to a copy of @a cstring.
     *
     * @param[in] mm      Memory manager to allocate memory from.
     * @param[in] cstring Null terminated string to copy into byte string.
     * @returns New byte string with copy of @a cstring.
     * @throws IronBee++ exception on any error.
     **/
    static ByteString create(MemoryManager mm, const char *cstring);

    /**
     * Create copy of @a s using @a mm.
     *
     * Creates a new byte string using @a mm to allocate memory and set
     * contents to a copy of @a s
     *
     * @param[in] mm Memory manager to allocate memory from.
     * @param[in] s  String to copy into byte string.
     * @returns New byte string with copy of @a s.
     * @throws IronBee++ exception on any error.
     **/
    static ByteString create(MemoryManager mm, const std::string& s);

    /**
     * Create a byte string pointing to @a data.
     *
     * Creates a new byte string that uses @a data as the underlying data.
     * If data changes, the byte string reflects that change, and the lifetime
     * of @a data must exceed the lifetime of the byte string.
     *
     * @param[in] mm     Memory manager to allocate memory from.
     * @param[in] data   Data of byte string.
     * @param[in] length Length of @a data.
     * @returns New byte string with alias of @a data.
     * @throws IronBee++ exception on any error.
     **/
    static
    ByteString create_alias(
        MemoryManager mm,
        const char*   data,
        size_t        length
    );

    /**
     * Create a byte string pointing to @a cstring.
     *
     * Creates a new byte string that uses @a cstring as the underlying data.
     * If @a cstring changes, the byte string reflects that change, and the
     * lifetime of @a cstring must exceed the lifetime of the byte string.
     *
     * @param[in] mm      Memory manager to allocate memory from.
     * @param[in] cstring Data of byte string.
     * @returns New byte string with alias of @a cstring.
     * @throws IronBee++ exception on any error.
     **/
    static
    ByteString create_alias(
        MemoryManager mm,
        const char*   cstring
    );

    /**
     * Create a byte string pointing to @a s.
     *
     * Creates a new byte string that uses @a s (actually s.data()) as the
     * underlying data.  If @a s  changes, the byte string reflects that
     * change, and the lifetime of @a s must exceed the lifetime of the byte
     * string.
     *
     * @param[in] mm Memory manager to allocate memory from.
     * @param[in] s  Data of byte string.
     * @returns New byte string with alias of @a s.
     * @throws IronBee++ exception on any error.
     **/
    static
    ByteString create_alias(
        MemoryManager      mm,
        const std::string& s
    );

    /// @}

    /**
     * Underlying data (read-write).
     *
     * If read only, NULL will be returned.  Otherwise, a non-const pointer to
     * the underlying data will be returned.  As with the const version, a
     * zero-length byte string is allowed to return NULL.
     *
     * @returns
     * - NULL if read only.
     * - Possibly NULL if zero-length.
     * - Pointer to data, otherwise.
     **/
    char* data() const;

    /**
     * @name Mutators
     * Change the byte string.
     **/
    /// @{

    /**
     * Make read-only.
     *
     * This marks the byte string as read only.  If the byte string is already
     * read-only, nothing is done.
     **/
    void make_read_only() const;

    /**
     * Clear the byte string.
     *
     * After calling this, the byte string will be zero-length.  Will throw
     * exception on a read only byte string.
     *
     * @throws einval if byte string is read only.
     **/
    void clear() const;

    /**
     * As create_alias() but modifies current byte string.  This will work
     * on a read only byte string as it does modify the previous data.  After
     * this call, the byte string will not be read only.
     *
     * @sa create_alias()
     **/
    void set(char* data, size_t length) const;
    //! As above, but result is read only.
    void set(const char* data, size_t length) const;
    //! As above, but for null terminated string.
    void set(char* cstring) const;
    //! As above, but result is read only.
    void set(const char* cstring) const;
    //! As above, but for string.  There is no non-read-only version.
    void set(const std::string& s) const;

    /**
     * Append the data in @a tail to byte string.
     *
     * @param[in] tail Data to append.
     * @throws einval if read-only; ealloc on allocation failure.
     **/
    void append(const ConstByteString& tail) const;
    //! As above, but for data and length.
    void append(const char* data, size_t length) const;
    //! As above, but for NULL terminated string.
    void append(const char* cstring) const;
    //! As above, but for string.
    void append(const std::string& s) const;
    /// @}

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! ib_bytestr_t accessor.
    // Intentionally inlined.
    ib_bytestr_t* ib() const
    {
        return m_ib;
    }

    //! Construct ByteString from ib_bytestr_t.
    explicit
    ByteString(ib_bytestr_t* ib_bytestr);

    ///@}

private:
    ib_bytestr_t* m_ib;
};

/**
 * Output operator for ByteString.
 *
 * Outputs ByteString[@e value] to @a o where @e value is replaced with
 * the value of the bytestring.
 *
 * @param[in] o           Ostream to output to.
 * @param[in] byte_string ByteString to output.
 * @return @a o
 **/
std::ostream& operator<<(std::ostream& o, const ConstByteString& byte_string);

} // IronBee

#endif
