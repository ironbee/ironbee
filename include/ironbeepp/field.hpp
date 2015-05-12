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
 * @brief IronBee++ --- Field
 *
 * This file defines Field, a wrapper for ib_field_t.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP__FIELD__
#define __IBPP__FIELD__

#include <ironbeepp/abi_compatibility.hpp>
#include <ironbeepp/byte_string.hpp>
#include <ironbeepp/common_semantics.hpp>
#include <ironbeepp/data.hpp>
#include <ironbeepp/exception.hpp>
#include <ironbeepp/list.hpp>
#include <ironbeepp/memory_manager.hpp>

#include <ironbee/field.h>


#include <ostream>

#include <boost/mpl/or.hpp>
#include <boost/type_traits/is_convertible.hpp>
#include <boost/type_traits/is_float.hpp>
#include <boost/type_traits/is_signed.hpp>
#include <boost/type_traits/is_unsigned.hpp>

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
    //! C Type.
    typedef const ib_field_t* ib_type;

    //! Types of field values.
    enum type_e {
        //! Generic --- Currently unsupported in IronBee++
        GENERIC         = IB_FTYPE_GENERIC,
        //! Signed Number
        NUMBER          = IB_FTYPE_NUM,
        //! Time --- This is represented by an unsigned 64bit int.
        TIME            = IB_FTYPE_TIME,
        //! Floating Point
        FLOAT           = IB_FTYPE_FLOAT,
        //! Null terminated string
        NULL_STRING     = IB_FTYPE_NULSTR,
        //! ByteString
        BYTE_STRING     = IB_FTYPE_BYTESTR,
        //! List &mdash.
        LIST            = IB_FTYPE_LIST,
        //! Stream Buffer --- Currently unsupported in IronBee++
        STREAM_BUFFER   = IB_FTYPE_SBUFFER
    };

    /**
     * Derive appropriate type_e for @a T.
     *
     * This function provides the appropriate type_e value for a given C++
     * type, @a T.  If no value is appropriate, a compiler error results.
     *
     * - Signed integral types result in NUMBER.
     * - Unsigned integral types result in TIME.
     * - Float types result in FLOAT.
     * - Types convertible to @c const @c char* result in NULL_STRING.
     * - Types convertible to ConstByteString result in BYTE_STRING.
     * - IronBee::List<T> results in LIST.
     * - All other types result in a compiler error.
     *
     * @tparam T Type to derive field type for.
     * @returns Appropriate field type.
     **/
    // Intentionally inlined.
    template <typename T>
    static type_e field_type_for_type()
    {
#ifdef __clang__
#pragma clang diagnostic push
#if __has_warning("-Wunused-local-typedef")
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
#endif
        // or_ has limited number of arguments, so cascade.
        BOOST_STATIC_ASSERT((
            boost::mpl::or_<
                /* Check number types. */
                boost::mpl::or_<
                    boost::is_float<T>,
                    boost::is_signed<T>,
                    boost::is_unsigned<T>
                >,
                /* Check other types. */
                boost::mpl::or_<
                    boost::is_convertible<T,const char*>,
                    boost::is_convertible<T,ConstByteString>,
                    is_list<T>
                >
            >::value
        ));
#ifdef __clang__
#pragma clang diagnostic pop
#endif
        if (boost::is_float<T>::value) {
            return FLOAT;
        }
        if (boost::is_signed<T>::value) {
            return NUMBER;
        }
        if (boost::is_unsigned<T>::value) {
            return TIME;
        }
        else if (boost::is_convertible<T,const char*>::value) {
            return NULL_STRING;
        }
        else if (boost::is_convertible<T,ConstByteString>::value) {
            return BYTE_STRING;
        }
        else if (is_list<T>::value) {
            return LIST;
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
     * Create copy using @a mm.
     *
     * Creates a new Field using @a mm to allocate memory and set
     * contents to a copy of this fields name and value.
     *
     * @param[in] mm Memory manager to allocate memory from.
     * @returns New Field with copy of this' name and value.
     * @throws IronBee++ exception on any error.
     **/
    Field dup(MemoryManager mm) const;
    //! As above, but use same memory mm.
    Field dup() const;

    /**
     * Create copy using @a mm, changing name.
     *
     * Creates a new Field using @a mm to allocate memory and set
     * contents to a copy of this fields value.
     *
     * @param[in] mm              Memory manager to allocate memory from.
     * @param[in] new_name        New fields name.
     * @param[in] new_name_length Length of @a name.
     * @returns New Field with copy of this' value.
     * @throws IronBee++ exception on any error.
     **/
    Field dup(
        MemoryManager mm,
        const char*   new_name,
        size_t        new_name_length
    ) const;
    //! As above, but use same memory mm.
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

    //! Memory manager of field..
    MemoryManager memory_manager() const;

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

    //! Time value accessor.
    uint64_t value_as_time() const;
    //! Time value accessor -- dynamic.
    uint64_t value_as_time(const std::string& arg) const;
    //! Time value accessor -- dynamic.
    uint64_t value_as_time(
        const char* arg,
        size_t      arg_length
    ) const;

    //! Float value accessor.
    long double value_as_float() const;
    //! Float value accessor -- dynamic.
    long double value_as_float(const std::string& arg) const;
    //! Float value accessor -- dynamic.
    long double value_as_float(
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

    //! List value accessor.
    template <typename T>
    ConstList<T> value_as_list() const;
    //! List value accessor -- dynamic.
    template <typename T>
    ConstList<T> value_as_list(const std::string& arg) const;
    //! List value accessor -- dynamic.
    template <typename T>
    ConstList<T> value_as_list(
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
 * support for three of these (NUMBER, NULL_STRING, and
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
    //! C Type.
    typedef ib_field_t* ib_type;

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
    * corresponding memory mm is cleared or destroyed.
    *
    * Alias fields refer to their underlying values by pointer/reference
    * rather than copy.  Changes to the underlying values are reflected by
    * the field and changes to the field, change the underlying value.
    * The lifetime of the value must be a superset of the lifetime of the
    * field.
    *
    * Note: There is no equivalent to ib_field_create_bytestr_alias() as this is
    * easily replaced with, e.g.,
    * @code
    * Field::create(mm, name, name_length, ByteString::create_alias(...))
    * @endcode
    **/
    ///@{

    /**
     * Create time field.
     *
     * @param[in] mm          Manager to use for memory allocation.
     * @param[in] name        Name of key.
     * @param[in] name_length Length of @a name.
     * @param[in] value       Value of field.
     * @throws IronBee++ exception on any error.
     **/
    static
    Field create_time(
        MemoryManager mm,
        const char*   name,
        size_t        name_length,
        uint64_t      value
    );

    /**
     * Create (signed) number field.
     *
     * @param[in] mm          Manager to use for memory allocation.
     * @param[in] name        Name of key.
     * @param[in] name_length Length of @a name.
     * @param[in] value       Value of field.
     * @throws IronBee++ exception on any error.
     **/
    static
    Field create_number(
        MemoryManager mm,
        const char*   name,
        size_t        name_length,
        int64_t       value
    );

    /**
     * Create float field.
     *
     * @param[in] mm          Manager to use for memory allocation.
     * @param[in] name        Name of key.
     * @param[in] name_length Length of @a name.
     * @param[in] value       Value of field.
     * @throws IronBee++ exception on any error.
     **/
    static
    Field create_float(
        MemoryManager mm,
        const char*   name,
        size_t        name_length,
        long double   value
    );

    /**
     * Create null string number field.
     *
     * @param[in] mm          Manager to use for memory allocation.
     * @param[in] name        Name of key.
     * @param[in] name_length Length of @a name.
     * @param[in] value       Value of field.
     * @throws IronBee++ exception on any error.
     **/
    static
    Field create_null_string(
        MemoryManager mm,
        const char*   name,
        size_t        name_length,
        const char*   value
    );

    /**
     * Create ByteString field.
     *
     * @param[in] mm          Manager to use for memory allocation.
     * @param[in] name        Name of key.
     * @param[in] name_length Length of @a name.
     * @param[in] value       Value of field.
     * @throws IronBee++ exception on any error.
     **/
    static
    Field create_byte_string(
        MemoryManager   mm,
        const char*     name,
        size_t          name_length,
        ConstByteString value
    );

    /**
     * Create null string field without copy.
     *
     * @param[in] mm          Manager to use for memory allocation.
     * @param[in] name        Name of key.
     * @param[in] name_length Length of @a name.
     * @param[in] value       Value of field.
     * @throws IronBee++ exception on any error.
     **/
    static
    Field create_no_copy_null_string(
        MemoryManager mm,
        const char*   name,
        size_t        name_length,
        char*         value
    );

    /**
     * Create ByteString field without copy.
     *
     * @sa create_bytestr_alias()
     *
     * @param[in] mm          Manager to use for memory allocation.
     * @param[in] name        Name of key.
     * @param[in] name_length Length of @a name.
     * @param[in] value       Value of field.
     * @throws IronBee++ exception on any error.
     **/
    static
    Field create_no_copy_byte_string(
        MemoryManager mm,
        const char*   name,
        size_t        name_length,
        ByteString    value
    );

    /**
     * Create List field without copy.
     *
     * Note: At present, IronBee does not support copy-in list fields.
     *
     * @param[in] mm          Manager to use for memory allocation.
     * @param[in] name        Name of key.
     * @param[in] name_length Length of @a name.
     * @param[in] value       Value of field.
     * @throws IronBee++ exception on any error.
     **/
    template <typename T>
    static
    Field create_no_copy_list(
        MemoryManager mm,
        const char*   name,
        size_t        name_length,
        List<T>       value
    );

    /**
     * Create Time alias.
     *
     * @param[in] mm          Manager to use for memory allocation.
     * @param[in] name        Name of key.
     * @param[in] name_length Length of @a name.
     * @param[in] value       Where to store value.
     **/
    static
    Field create_alias_time(
        MemoryManager mm,
        const char*   name,
        size_t        name_length,
        uint64_t&     value
    );

    /**
     * Create Number alias.
     *
     * @param[in] mm          Manager to use for memory allocation.
     * @param[in] name        Name of key.
     * @param[in] name_length Length of @a name.
     * @param[in] value       Where to store value.
     **/
    static
    Field create_alias_number(
        MemoryManager mm,
        const char*   name,
        size_t        name_length,
        int64_t&      value
    );

    /**
     * Create Float alias.
     *
     * @param[in] mm          Manager to use for memory allocation.
     * @param[in] name        Name of key.
     * @param[in] name_length Length of @a name.
     * @param[in] value       Where to store value.
     **/
    static
    Field create_alias_float(
        MemoryManager mm,
        const char*   name,
        size_t        name_length,
        long double&  value
    );

    /**
     * Create null string alias.
     *
     * @param[in] mm          Manager to use for memory allocation.
     * @param[in] name        Name of key.
     * @param[in] name_length Length of @a name.
     * @param[in] value       Where to store value.
     **/
    static
    Field create_alias_null_string(
        MemoryManager mm,
        const char*   name,
        size_t        name_length,
        char*&        value
    );

    /**
     * Create ByteString alias.
     *
     * Note that this uses the C type, ib_bytestr_t, instead of ByteString.
     * This is because @a value represents a location to store the field
     * value which is a C type.  You can turn that into a ByteString when you
     * use it via @c ByteString(value).
     *
     * @param[in] mm          Manager to use for memory allocation.
     * @param[in] name        Name of key.
     * @param[in] name_length Length of @a name.
     * @param[in] value       Where to store value.
     **/
    static
    Field create_alias_byte_string(
        MemoryManager  mm,
        const char*    name,
        size_t         name_length,
        ib_bytestr_t*& value
    );

    /**
     * Create List alias.
     *
     * Note that this uses the C type, ib_list_t, instead of List.
     * This is because @a value represents a location to store the field
     * value which is a C type.  You can turn that into a List when you
     * use it via @c List(value).
     *
     * @param[in] mm          Manager to use for memory allocation.
     * @param[in] name        Name of key.
     * @param[in] name_length Length of @a name.
     * @param[in] value       Where to store value.
     **/
    static
    Field create_alias_list(
        MemoryManager mm,
        const char*   name,
        size_t        name_length,
        ib_list_t*&   value
    );

    /**
     * @name Dynamic Fields
     * Methods relating to dynamic fields.
     **/
    /// @{

    //! Time field getter.
    typedef boost::function<
        uint64_t(ConstField, const char*, size_t)
    > time_get_t;
    //! (Signed) Number field getter.
    typedef boost::function<
        int64_t(ConstField, const char*, size_t)
    > number_get_t;
    //! Float field getter.
    typedef boost::function<
        long double(ConstField, const char*, size_t)
    > float_get_t;
    //! Null string field getter.
    typedef boost::function<
        const char*(ConstField, const char*, size_t)
    > null_string_get_t;
    //! ByteString field getter.
    typedef boost::function<
        ConstByteString(ConstField, const char*, size_t)
    > byte_string_get_t;

    //! Time field setter.
    typedef boost::function<
        void(Field, const char*, size_t, uint64_t)
    > time_set_t;
    //! (Signed) Number field setter.
    typedef boost::function<
        void(Field, const char*, size_t, int64_t)
    > number_set_t;
    //! Float field setter.
    typedef boost::function<
        void(Field, const char*, size_t, long double)
    > float_set_t;
    //! Null string field setter.
    typedef boost::function<
        void(Field, const char*, size_t, const char*)
    > null_string_set_t;
    //! ByteString field setter.
    typedef boost::function<
        void(Field, const char*, size_t, ConstByteString)
    > byte_string_set_t;

    //! As create_number() but with dynamic setter/getter.
    static Field create_dynamic_number(
        MemoryManager   mm,
        const char*  name,
        size_t       name_length,
        number_get_t get,
        number_set_t set
    );

    //! As create_time() but with dynamic setter/getter.
    static Field create_dynamic_time(
        MemoryManager   mm,
        const char*  name,
        size_t       name_length,
        time_get_t get,
        time_set_t set
    );

    //! As create_float() but with dynamic setter/getter.
    static Field create_dynamic_float(
        MemoryManager  mm,
        const char* name,
        size_t      name_length,
        float_get_t get,
        float_set_t set
    );

    //! As create_null_string() but with dynamic setter/getter.
    static Field create_dynamic_null_string(
        MemoryManager        mm,
        const char*       name,
        size_t            name_length,
        null_string_get_t get,
        null_string_set_t set
    );

    //! As create_byte_string() but with dynamic setter/getter.
    static Field create_dynamic_byte_string(
        MemoryManager        mm,
        const char*       name,
        size_t            name_length,
        byte_string_get_t get,
        byte_string_set_t set
    );
    //! As create_no_copy_list() but with dynamic setter/getter.
    template <typename T>
    static Field create_dynamic_list(
        MemoryManager        mm,
        const char*       name,
        size_t            name_length,
        boost::function<ConstList<T>(ConstField, const char*, size_t)>  get,
        boost::function<void(Field, const char*, size_t, ConstList<T>)> set
    );
    ///@}
    ///@}

    /**
     * Make field static.
     *
     * This should be immediately followed by a @c set_* call.  Will throw
     * an einval if field is not dynamic.
     **/
    void make_static() const;

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

    //! Set time value.
    void set_time(uint64_t value) const;
    //! Set time value -- dynamic.
    void set_time(uint64_t value, const std::string& arg) const;
    //! Set time value -- dynamic.
    void set_time(uint64_t value, const char* arg, size_t arg_length) const;

    //! Set number value.
    void set_number(int64_t value) const;
    //! Set number value -- dynamic.
    void set_number(int64_t value, const std::string& arg) const;
    //! Set number value -- dynamic.
    void set_number(int64_t value, const char* arg, size_t arg_length) const;

    //! Set float value.
    void set_float(long double value) const;
    //! Set float value -- dynamic.
    void set_float(long double value, const std::string& arg) const;
    //! Set float value -- dynamic.
    void set_float(
        long double value,
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

    //! Set null string without copy.
    void set_no_copy_null_string(char* value) const;
    //! Set byte string without copy..
    void set_no_copy_byte_string(ByteString value) const;

    //! Set List value.
    template <typename T>
    void set_no_copy_list(List<T> value) const;
    //! Set List value -- dynamic.
    template <typename T>
    void set_no_copy_list(List<T> value, const std::string& arg) const;
    //! Set List value -- dynamic.
    template <typename T>
    void set_no_copy_list(
        List<T> value,
        const char* arg, size_t arg_length
    ) const;

    ///@}

    /**
     * @name Mutable Value Getters
     * Query the value.
     *
     * Only the methods that correspond to type() will return the value.
     * Others will throw einval.  Similarly, the forms that take an argument
     * will throw einval if the field is not dynamic.
     *
     * Can not be used with dynamic fields.
     **/
    ///@{

    //! Number mutable value accessor.
    uint64_t& mutable_value_as_time() const;
    //! Number mutable value accessor.
    int64_t& mutable_value_as_number() const;
    //! Unsigned number mutable value accessor.
    long double& mutable_value_as_float() const;
    //! Null string mutable value accessor.
    char* mutable_value_as_null_string() const;
    //! ByteString mutable value accessor.
    ByteString mutable_value_as_byte_string() const;
    //! List mutable value accessor.
    template <typename T>
    List<T> mutable_value_as_list() const;

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

namespace Internal {
/// @cond Internal

void check_type(Field::type_e expected, Field::type_e actual);

Field create_no_copy(
    MemoryManager    mm,
    const char*   name,
    size_t        name_length,
    Field::type_e type,
    void*         mutable_in_value
);

void set_value_no_copy(ib_field_t* f, void* mutable_in_value);

void set_value(
    ib_field_t* f,
    void* in_value,
    const char* arg,
    size_t arg_length
);

typedef boost::function<
    const ib_list_t*(ConstField, const char*, size_t)
> dynamic_list_getter_translator_t;

template <typename T>
class dynamic_list_getter_translator
{
public:
    typedef boost::function<
        ConstList<T>(ConstField, const char*, size_t)
    > get_t;
    dynamic_list_getter_translator(get_t get) :
        m_get(get)
    {
        // nop
    }

    const ib_list_t* operator()(
        ConstField field,
        const char* arg,
        size_t arg_length
    ) const
    {
        return m_get(field, arg, arg_length).ib();
    }

private:
    get_t m_get;
};

typedef boost::function<
    void(Field, const char*, size_t, const ib_list_t* value)
> dynamic_list_setter_translator_t;

template <typename T>
class dynamic_list_setter_translator
{
public:
    typedef boost::function<
        void(Field, const char*, size_t, ConstList<T>)
    > set_t;
    dynamic_list_setter_translator(set_t set) :
        m_set(set)
    {
        // nop
    }

    void operator()(
        Field field,
        const char* arg,
        size_t arg_length,
        const ib_list_t* value
    ) const
    {
        m_set(field, arg, arg_length, ConstList<T>(value));
    }

private:
    set_t m_set;
};

Field create_dynamic_field(
    MemoryManager    mm,
    const char*   name,
    size_t        name_length,
    Field::type_e type,
    void*         cbdata_get,
    void*         cbdata_set
);

/// @endcond
} // Internal

// Definitions

template <typename T>
ConstList<T> ConstField::value_as_list() const
{
    Internal::check_type(LIST, type());
    const ib_list_t* v;
    throw_if_error(ib_field_value(ib(), ib_ftype_list_out(&v)));
    return ConstList<T>(v);
}

template <typename T>
ConstList<T> ConstField::value_as_list(const std::string& arg) const
{
    return value_as_list<T>(arg.data(),arg.length());
}

template <typename T>
ConstList<T> ConstField::value_as_list(
    const char* arg,
    size_t      arg_length
) const
{
    Internal::check_type(LIST, type());
    const ib_list_t* v;
    throw_if_error(ib_field_value_ex(
        ib(), ib_ftype_list_out(&v),
        arg, arg_length
    ));
    return ConstList<T>(v);
}

template <typename T>
Field Field::create_no_copy_list(
    MemoryManager  mm,
    const char* name,
    size_t      name_length,
    List<T>     value
)
{
    return Internal::create_no_copy(
        mm,
        name, name_length,
        Field::LIST,
        ib_ftype_list_mutable_in(value.ib())
    );
}

template <typename T>
void Field::set_no_copy_list(List<T> value) const
{
    Internal::check_type(LIST, type());
    Internal::set_value_no_copy(
        ib(), ib_ftype_list_mutable_in(value.ib())
    );
}

template <typename T>
void Field::set_no_copy_list(List<T> value, const std::string& arg) const
{
    return set_no_copy_list(value, arg.data(), arg.length());
}

template <typename T>
void Field::set_no_copy_list(
    List<T> value,
    const char* arg, size_t arg_length
) const
{
    Internal::check_type(LIST, type());
    Internal::set_value(ib(), ib_ftype_list_in(value.ib()), arg, arg_length);
}

template <typename T>
Field Field::create_dynamic_list(
    MemoryManager   mm,
    const char*  name,
    size_t       name_length,
    boost::function<ConstList<T>(ConstField, const char*, size_t)>  get,
    boost::function<void(Field, const char*, size_t, ConstList<T>)> set
)
{
    Internal::dynamic_list_getter_translator_t getter =
      Internal::dynamic_list_getter_translator<T>(get);
    Internal::dynamic_list_setter_translator_t setter =
      Internal::dynamic_list_setter_translator<T>(set);

    return Internal::create_dynamic_field(
        mm,
        name, name_length,
        Field::LIST,
        value_to_data(getter, mm.ib()),
        value_to_data(setter, mm.ib())
    );
}

template <typename T>
List<T> Field::mutable_value_as_list() const
{
    Internal::check_type(LIST, type());
    ib_list_t* l;
    throw_if_error(ib_field_mutable_value(ib(),
        ib_ftype_list_mutable_out(&l)
    ));
    return List<T>(l);;
}

} // IronBee

#endif
