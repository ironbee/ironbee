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
 * @brief IronBee++ &mdash; Field
 *
 * This file defines Field, a wrapper for ib_field_t.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP__FIELD__
#define __IBPP__FIELD__

#include <ironbeepp/common_semantics.hpp>
#include <ironbeepp/exception.hpp>
#include <ironbeepp/memory_pool.hpp>
#include <ironbeepp/byte_string.hpp>

#include <ironbee/field.h>

#include <boost/mpl/or.hpp>
#include <boost/type_traits/is_signed.hpp>
#include <boost/type_traits/is_unsigned.hpp>
#include <boost/type_traits/is_convertible.hpp>

#include <ostream>

namespace IronBee {

class Field;

/**
 * Const field; equivalent to a const pointer to ib_field_t.
 *
 * Provides operators ==, !=, <, >, <=, >= and evaluation as a boolean for
 * singularity via CommonSemantics.
 *
 * See Field for discussion of fields.
 *
 * @sa Field
 * @sa ironbeepp
 * @sa ib_field_t
 * @nosubgrouping
 **/
class ConstField :
    public CommonSemantics<ConstField>
{
public:
    //! Types of field values.
    enum type_e {
        //! Generic &mdash; Currently unsupported in IronBee++
        GENERIC         = IB_FTYPE_GENERIC,
        //! Signed Number
        NUMBER          = IB_FTYPE_NUM,
        //! Unsigned Number
        UNSIGNED_NUMBER = IB_FTYPE_UNUM,
        //! Null terminated string
        NULL_STRING     = IB_FTYPE_NULSTR,
        //! ByteString
        BYTE_STRING     = IB_FTYPE_BYTESTR,
        //! List &mdash; Currently unsupported in IronBee++
        LIST            = IB_FTYPE_LIST,
        //! Stream Buffer &mdash; Currently unsupported in IronBee++
        STREAM_BUFFER   = IB_FTYPE_SBUFFER
    };

    /**
     * Derive appropriate type_e for @a T.
     *
     * This function provides the appropriate type_e value for a given C++
     * type, @a T.  If no value is appropriate, a compiler error results.
     *
     * - Signed integral types result in NUMBER.
     * - Unsigned integral types result in UNSIGNED_NUMBER.
     * - Types convertible to @c const @c char* result in NULL_STRING.
     * - Types convertible to ConstByteString result in BYTE_STRING.
     * - All other types result in a compiler error.
     *
     * @tparam T Type to derive field type for.
     * @returns Appropriate field type.
     **/
    // Intentionally inlined.
    template <typename T>
    static type_e field_type_for_type()
    {
        BOOST_STATIC_ASSERT((
            boost::mpl::or_<
                boost::is_signed<T>,
                boost::is_unsigned<T>,
                boost::is_convertible<T,const char*>,
                boost::is_convertible<T,ConstByteString>
            >::value
        ));
        if (boost::is_signed<T>::value) {
            return NUMBER;
        }
        else if (boost::is_unsigned<T>::value) {
            return UNSIGNED_NUMBER;
        }
        else if (boost::is_convertible<T,const char*>::value) {
            return NULL_STRING;
        }
        else if (boost::is_convertible<T,ConstByteString>::value) {
            return BYTE_STRING;
        }
    }

    /**
     * Construct singular ConstField.
     *
     * All behavior of a singular ConstField is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    ConstField();

   /**
    * @name Creation
    * Routines for creating new fields.
    *
    * See Field for detailed discussion.
    * @sa Field
    **/
    ///@{

    /**
     * Create copy using @a pool.
     *
     * Creates a new Field using @a pool to allocate memory and set
     * contents to a copy of this fields name and value.
     *
     * @param[in] pool Memory pool to allocate memory from.
     * @returns New Field with copy of this' name and value.
     * @throws IronBee++ exception on any error.
     **/
    Field dup(MemoryPool pool) const;
    //! As above, but use same memory pool.
    Field dup() const;

    /**
     * Create copy using @a pool, changing name.
     *
     * Creates a new Field using @a pool to allocate memory and set
     * contents to a copy of this fields value.
     *
     * @param[in] pool            Memory pool to allocate memory from.
     * @param[in] new_name        New fields name.
     * @param[in] new_name_length Length of @a name.
     * @returns New Field with copy of this' value.
     * @throws IronBee++ exception on any error.
     **/
    Field dup(
        MemoryPool pool,
        const char* new_name,
        size_t new_name_length
    ) const;
    //! As above, but use same memory pool.
    Field dup(const char* new_name, size_t new_name_length) const;

    ///@}

    /**
     * @name Queries
     * Query aspects of the field.
     **/
    ///@{

    //! Name of field.
    const char* name() const;

    //! Length of name.
    size_t name_length() const;

    //! Name as string.
    std::string name_as_s() const;

    //! Type of field.
    type_e type() const;

    //! Memory pool of field..
    MemoryPool memory_pool() const;

    /**
     * Create string version of value.
     *
     * This will create a string of the value no matter what it's type.
     *
     * @returns string representing value.
     **/
    std::string to_s() const;

    //! True if field is dynamic.
    bool is_dynamic() const;

    /// @}

    /**
     * @name Value Getters
     * Query the value.
     *
     * Only the methods that correspond to type() will return the value.
     * Others will throw einval.  Similarly, the forms that take an argument
     * will throw einval if the field is not dynamic.
     *
     * Note the non-dynamic versions can be used with dynamic fields.
     **/
    ///@{

    //! Number value accessor.
    int64_t value_as_number() const;
    //! Number value accessor -- dynamic.
    int64_t value_as_number(const std::string& arg) const;
    //! Number value accessor -- dynamic.
    int64_t value_as_number(
        const char* arg,
        size_t      arg_length
    ) const;

    //! Unsigned number value accessor.
    uint64_t value_as_unsigned_number() const;
    //! Unsigned number value accessor -- dynamic.
    uint64_t value_as_unsigned_number(const std::string& arg) const;
    //! Unsigned number value accessor -- dynamic.
    uint64_t value_as_unsigned_number(
        const char* arg,
        size_t      arg_length
    ) const;

    //! Null string value accessor.
    const char* value_as_null_string() const;
    //! Null string value accessor -- dynamic.
    const char* value_as_null_string(const std::string& arg) const;
    //! Null string value accessor -- dynamic.
    const char* value_as_null_string(
        const char* arg,
        size_t      arg_length
    ) const;

    //! ByteString value accessor.
    ConstByteString value_as_byte_string() const;
    //! ByteString value accessor -- dynamic.
    ConstByteString value_as_byte_string(const std::string& arg) const;
    //! ByteString value accessor -- dynamic.
    ConstByteString value_as_byte_string(
        const char* arg,
        size_t      arg_length
    ) const;

    ///@}

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! Const ib_field_t accessor.
    // Intentionally inlined.
    const ib_field_t* ib() const
    {
        return m_ib;
    }

    //! Construct Field from ib_field_t.
    explicit
    ConstField(const ib_field_t* ib_field);

    ///@}

private:
    const ib_field_t* m_ib;
};

/**
 * Field; equivalent to a pointer to ib_field_t.
 *
 * Fields can be treated as ConstFields.  See @ref ironbeepp for details on
 * IronBee++ object semantics.
 *
 * Fields are used by IronBee to represent key/value pairs where the value
 * can be one of a preset set of types and the key is a string literal.
 *
 * The C API supports seven types (see type_e).  The C++ API provides full
 * support for four of these (NUMBER, UNSIGNED_NUMBER, NULL_STRING, and
 * BYTE_STRING).  Support for other types may come in the future.
 *
 * Fields can also be dynamic where set and get operations are forwarded to
 * functions.  Dynamic fields can have arguments passed to them which are
 * also forwarded to set and get.
 *
 * @sa ironbeepp
 * @sa ib_field_t
 * @sa ConstField
 * @nosubgrouping
 **/
class Field :
    public ConstField
{
public:
    /**
     * Remove the constness of a ConstField
     *
     * @warning This is as dangerous as a @c const_cast, use carefully.
     *
     * @param[in] field ConstField to remove const from.
     * @returns Field pointing to same underlying byte string as @a field.
     **/
    static Field remove_const(const ConstField& field);

   /**
    * Construct singular Field.
    *
    * All behavior of a singular Field is undefined except for
    * assignment, copying, comparison, and evaluate-as-bool.
    **/
    Field();

   /**
    * @name Creation
    * Routines for creating new fields.
    *
    * These routines create new fields.  The fields are destroyed when the
    * corresponding memory pool is cleared or destroyed.
    *
    * Alias fields refer to their underlying values by pointer/reference
    * rather than copy.  Changes to the underlying values are reflected by
    * the field and changes to the field, change the underlying value.
    * The lifetime of the value must be a superset of the lifetime of the
    * field.
    *
    * Note: There is no equivalent to ib_field_alias_mem_ex() as this is
    * easily replaced with, e.g.,
    * @code
    * Field::create(pool, name, name_length, ByteString::create_alias(...))
    * @endcode
    **/
    ///@{

    /**
     * Create (signed) number field.
     *
     * @param[in] pool Pool to use for memory allocation.
     * @param[in] name        Name of key.
     * @param[in] name_length Length of @a name.
     * @param[in] value       Value of field.
     * @throws IronBee++ exception on any error.
     **/
    static Field create_number(
        MemoryPool  pool,
        const char* name,
        size_t      name_length,
        int64_t     value
    );

    /**
     * Create unsigned number field.
     *
     * @param[in] pool Pool to use for memory allocation.
     * @param[in] name        Name of key.
     * @param[in] name_length Length of @a name.
     * @param[in] value       Value of field.
     * @throws IronBee++ exception on any error.
     **/
    static Field create_unsigned_number(
        MemoryPool  pool,
        const char* name,
        size_t      name_length,
        uint64_t    value
    );

    /**
     * Create null string number field.
     *
     * @param[in] pool Pool to use for memory allocation.
     * @param[in] name        Name of key.
     * @param[in] name_length Length of @a name.
     * @param[in] value       Value of field.
     * @throws IronBee++ exception on any error.
     **/
    static Field create_null_string(
        MemoryPool  pool,
        const char* name,
        size_t      name_length,
        const char* value
    );

    /**
     * Create ByteString field.
     *
     * @param[in] pool Pool to use for memory allocation.
     * @param[in] name        Name of key.
     * @param[in] name_length Length of @a name.
     * @param[in] value       Value of field.
     * @throws IronBee++ exception on any error.
     **/
    static Field create_byte_string(
        MemoryPool       pool,
        const char*      name,
        size_t           name_length,
        ConstByteString  value
    );

    /**
     * Create (signed) number alias field.
     *
     * @param[in] pool Pool to use for memory allocation.
     * @param[in] name        Name of key.
     * @param[in] name_length Length of @a name.
     * @param[in] value       Value of field.
     * @throws IronBee++ exception on any error.
     **/
    static Field create_alias_number(
        MemoryPool  pool,
        const char* name,
        size_t      name_length,
        int64_t&    value
    );

    /**
     * Create unsigned number alias field.
     *
     * @param[in] pool Pool to use for memory allocation.
     * @param[in] name        Name of key.
     * @param[in] name_length Length of @a name.
     * @param[in] value       Value of field.
     * @throws IronBee++ exception on any error.
     **/
    static Field create_alias_unsigned_number(
        MemoryPool  pool,
        const char* name,
        size_t      name_length,
        uint64_t&   value
    );

    /**
     * Create null string alias field.
     *
     * @param[in] pool Pool to use for memory allocation.
     * @param[in] name        Name of key.
     * @param[in] name_length Length of @a name.
     * @param[in] value       Value of field.
     * @throws IronBee++ exception on any error.
     **/
    static Field create_alias_null_string(
        MemoryPool  pool,
        const char* name,
        size_t      name_length,
        const char* value
    );

    /**
     * Create ByteString alias field.
     *
     * @sa create_bytestr_alias()
     *
     * @param[in] pool Pool to use for memory allocation.
     * @param[in] name        Name of key.
     * @param[in] name_length Length of @a name.
     * @param[in] value       Value of field.
     * @throws IronBee++ exception on any error.
     **/
    static Field create_alias_byte_string(
        MemoryPool      pool,
        const char*     name,
        size_t          name_length,
        ConstByteString value
    );

    ///@}

    /**
     * @name Value Setters
     * Set the value.
     *
     * Only the methods that correspond to type() will set the value.
     * Others will throw einval.  Similarly, the forms that take an argument
     * will throw einval if the field is not dynamic.
     *
     * Note the non-dynamic versions can be used with dynamic fields.
     *
     * The static setters will set the value directly, skipping any dynamic
     * setters.  They also make the field non-dynamic.
     **/
    ///@{

    //! Set (signed) number value.
    void set_number(int64_t value) const;
    //! Set (signed) number value -- dynamic.
    void set_number(int64_t value, const std::string& arg) const;
    //! Set (signed) number value -- dynamic.
    void set_number(int64_t value, const char* arg, size_t arg_length) const;

    //! Set unsigned number value.
    void set_unsigned_number(uint64_t value) const;
    //! Set unsigned number value -- dynamic.
    void set_unsigned_number(uint64_t value, const std::string& arg) const;
    //! Set unsigned number value -- dynamic.
    void set_unsigned_number(
        uint64_t value,
        const char* arg, size_t arg_length
    ) const;

    //! Set null string value.
    void set_null_string(const char* value) const;
    //! Set null string value -- dynamic.
    void set_null_string(const char* value, const std::string& arg) const;
    //! Set null string value -- dynamic.
    void set_null_string(
        const char* value,
        const char* arg, size_t arg_length
    ) const;

    //! Set ByteString value.
    void set_byte_string(ConstByteString value) const;
    //! Set ByteString value -- dynamic.
    void set_byte_string(ConstByteString value, const std::string& arg) const;
    //! Set ByteString value -- dynamic.
    void set_byte_string(
        ConstByteString value,
        const char* arg, size_t arg_length
    ) const;

    //! Set (signed) number value statically.
    void set_static_number(int64_t value) const;
    //! Set unsigned number value statically.
    void set_static_unsigned_number(uint64_t value) const;
    //! Set null string value statically.
    void set_static_null_string(const char* value) const;
    //! Set ByteString value statically.
    void set_static_byte_string(ConstByteString value) const;
    ///@}

    /**
     * @name Dynamic Fields
     * Methods relating to dynamic fields.
     *
     * Calling a @c register_ method with a functional that does not match
     * the field type will result in an einval exception.
     **/
    /// @{

    //! (Signed) Number field getter.
    typedef boost::function<
        int64_t(ConstField, const char*, size_t)
    > number_get_t;
    //! Unsigned Number field getter.
    typedef boost::function<
        uint64_t(ConstField, const char*, size_t)
    > unsigned_number_get_t;
    //! Null string field getter.
    typedef boost::function<
        const char*(ConstField, const char*, size_t)
    > null_string_get_t;
    //! ByteString field getter.
    typedef boost::function<
        ConstByteString(ConstField, const char*, size_t)
    > byte_string_get_t;

    //! Register getter.
    void register_dynamic_get_number(number_get_t                   f) const;
    //! Register getter.
    void register_dynamic_get_unsigned_number(unsigned_number_get_t f) const;
    //! Register getter.
    void register_dynamic_get_null_string(null_string_get_t         f) const;
    //! Register getter.
    void register_dynamic_get_byte_string(byte_string_get_t         f) const;

    //! (Signed) Number field setter.
    typedef boost::function<
        void(Field, const char*, size_t, int64_t)
    > number_set_t;
    //! Unsigned Number field setter.
    typedef boost::function<
        void(Field, const char*, size_t, uint64_t)
    > unsigned_number_set_t;
    //! Null string field setter.
    typedef boost::function<
        void(Field, const char*, size_t, const char*)
    > null_string_set_t;
    //! ByteString field setter.
    typedef boost::function<
        void(Field, const char*, size_t, ConstByteString)
    > byte_string_set_t;

    //! Register setter.
    void register_dynamic_set_number(number_set_t                   f) const;
    //! Register setter.
    void register_dynamic_set_unsigned_number(unsigned_number_set_t f) const;
    //! Register setter.
    void register_dynamic_set_null_string(null_string_set_t         f) const;
    //! Register setter.
    void register_dynamic_set_byte_string(byte_string_set_t         f) const;

    ///@}

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! Non-const ib_field_t accessor.
    // Intentionally inlined.
    ib_field_t* ib() const
    {
        return m_ib;
    }

    //! Construct Field from ib_field_t.
    explicit
    Field(ib_field_t* ib_field);

    ///@}

private:
    ib_field_t* m_ib;
};

/**
 * Output operator for Field.
 *
 * Outputs Field[@e name = @e value] to @a o where @e value is replaced with
 * the Field::to_s().
 *
 * @param[in] o     Ostream to output to.
 * @param[in] field Field to output.
 * @return @a o
 **/
std::ostream& operator<<(std::ostream& o, const ConstField& field);

} // IronBee

#endif
