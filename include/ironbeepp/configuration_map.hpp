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
 * @brief IronBee++ --- Configuration Map
 *
 * This file defines code related to configuration map.  Unlike other portions
 * of IronBee++, the code in this file diverges significantly from the C API.
 * At present, this code should not be used directly, but rather as described
 * in Module::set_configuration_data() and
 * Module::set_configuration_data_pod().  In the future, APIs to access other
 * modules configuration via configuration maps may be added.
 *
 * @sa Module
 * @sa Module::set_configuration_data_pod()
 * @sa Module::set_configuration_data()
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP__CONFIGURATION_MAP__
#define __IBPP__CONFIGURATION_MAP__

#include <ironbeepp/abi_compatibility.hpp>
#include <ironbeepp/field.hpp>
#include <ironbeepp/memory_manager.hpp>

#include <ironbee/cfgmap.h>

#include <boost/mpl/or.hpp>
#include <boost/static_assert.hpp>
#include <boost/type_traits/is_class.hpp>
#include <boost/type_traits/is_convertible.hpp>
#include <boost/utility/enable_if.hpp>

#include <list>

#include <cassert>

namespace IronBee {

/**
 * Helper template for Module::set_configuration_data() and
 * Module::set_configuration_data_pod().
 *
 * This general form is to allow cases where @a ConfigurationData is not a
 * class.  This provides a valid but empty template.  The following
 * specialization is the primary case, when @a ConfigurationData is a class.
 *
 * Future work may expand this code to allow for functional configuration map
 * entries.  This would be useful for modules that want to provide
 * configuration maps but not make use of the module configuration data
 * support.
 *
 * @sa Module
 * @sa Module::set_configuration_data_pod()
 * @sa Module::set_configuration_data()
 *
 * @tparam ConfigurationData ConfigurationData
 * @tparam Enable Implementation Detail
 **/
template <typename ConfigurationData, typename Enable = void>
class ConfigurationMapInit
{
public:
    //! Null Constructor
    ConfigurationMapInit(
        const ib_cfgmap_init_t*&,
    MemoryManager
    )
    {
        // nop
    }
};

/**
 * Helper template for Module::set_configuration_data() and
 * Module::set_configuration_data_pod().
 *
 * You probably do not want to use this template directly.  Instead, call
 * Module::set_configuration_data() or Module::set_configuration_data_pod(),
 * both of which return this to facilitate configuration map creation.
 *
 * This code is roughly analogous to the IB_CFGMAP_INIT* macros in the C API.
 *
 * This template is instantiated with a specific @a ConfigurationData type and
 * @c ib_cfgmap_init_t* pointer for a initiation structure to fill.  It then
 * provides a variety of API calls for initializing that structure in a
 * natural and typesafe way.
 *
 * This code uses a call chaining syntax, e.g.,
 * @code
 * module.set_configuration_data_pod(global_data)
 *   .number         ("my_number",          &my_data::number)
 *   ;
 * @endcode
 *
 * In the previous example, the actual initialization structure is created
 * when the temporary is destroyed.  If you want to initialize the structure
 * before destruction, call finish() explicitly.
 *
 * Configuration maps are used by IronBee for two purposes: first, they
 * provide a uniform syntax in configuration files for reading and writing
 * module configuration values; two, they provide a uniform API to other
 * modules for reading and writing module configuration values.  They achieve
 * both purposes by creating a number of entries that connect string
 * identifiers with configuration values, either by direct member access or
 * via setter and getter functionals.
 *
 * Every configuration value has a runtime type based on the Field code.  The
 * currently supported runtime types are Field::NUMBER,
 * Field::UNSIGNED_NUMBER, Field::NULL_STRING, and Field::BYTE_STRING.
 *
 * There are three types of configuration map entries available.  All require
 * an explicit type (as method name) and a name.
 * - Direct data member access: Provide a data member pointer for your
 *   configuration data.
 * - Direct function member access: Provide member pointers to getter and
 *   setter members of your configuration data.  Getter must take the name as
 *   the only argument and return the value; setters must take the name and
 *   new value as arguments and return void.
 * - Functional member access: Provide setter and getter functionals.
 *   Functionals are as above, except in addition take a reference to the
 *   configuration data as the first argument.
 *
 * In addition to direct conversions, string to/from ByteString is allowed
 * via the byte_string_s() methods.
 *
 * @sa Module
 * @sa Module::set_configuration_data_pod()
 * @sa Module::set_configuration_data()
 *
 * @tparam ConfigurationData Type of configuration data.
 * @nosubgrouping
 **/
template <typename ConfigurationData>
class ConfigurationMapInit<
    ConfigurationData,
    typename boost::enable_if<boost::is_class<ConfigurationData> >::type
>
{
public:
    //! @a ConfigurationData template parameter.
    typedef ConfigurationData configuration_data_t;
    //! Type of this class.
    typedef ConfigurationMapInit<
        configuration_data_t,
        typename boost::enable_if<boost::is_class<ConfigurationData> >::type
    > this_t;

    /**
     * Constructor.
     *
     * You will usually not construct this directly; instead use
     * Module::set_configuration_data_pod() or
     * Module::set_configuration_data().
     *
     * @param[in] ib_init        Where to fill in the (C) initialization
     *                           structure.
     * @param[in] memory_manager Memory manager to use as needed.
     * @param[in] data_is_handle If true, configuration data from C API
     *                           will be treated as handle rather than
     *                           pointer.
     **/
    ConfigurationMapInit(
        const ib_cfgmap_init_t*& ib_init,
        MemoryManager            memory_manager,
        bool                     data_is_handle = false
    );

    /**
     * Destructor.
     *
     * Calls finish().
     **/
    ~ConfigurationMapInit();

    /**
     * Initialize C API.
     *
     * @warning The behavior of this object is undefined after this method,
     * except for destruction.
     *
     * This method communicates all entries to the C API using the @a ib_init
     * parameter of the constructor.  It is also called on destruction and,
     * as such, is unnecessary when chained directly from
     * Module::set_configuration_data() and
     * Module::set_configuration_data_pod().
     **/
    void finish();

    /**
     * @name Typed Entry Creators
     * Create configuration entries.
     *
     * The following methods allow creation of entries for various field
     * types.  Except for the specific value types involved, they are
     * identical across number, null string, and byte string.
     *
     * @sa Field
     **/
    /// @{
    // Number

    //! Number Getter Functional
    typedef boost::function<
        int64_t(const configuration_data_t& data, const std::string&)
    > number_getter_t;
    //! Number Setter Functional
    typedef boost::function<
        void(configuration_data_t& data, const std::string&, int64_t value)
    > number_setter_t;

    // Number Getter Member
    typedef int64_t (configuration_data_t::*number_member_getter_t)(
        const std::string&
    ) const;
    // Number Setter Member
    typedef void (configuration_data_t::*number_member_setter_t)(
        const std::string&,
        int64_t
    );

    /**
     * Create number entry --- data member.
     *
     * @a FieldType must be convertible to int64_t.
     *
     * @tparam FieldType Type of field.
     * @param[in] name   Name of entry.
     * @param[in] member Member pointer.
     *
     * @return @c *this for call chaining.
     **/
    template <typename FieldType>
    this_t& number(
        const char*                       name,
        FieldType configuration_data_t::* member
    );

    /**
     * Create number entry --- function member.
     *
     * @param[in] name   Name of entry.
     * @param[in] getter Getter member.
     * @param[in] setter Setter member.
     * @returns @c *this for call chaining.
     **/
     this_t& number(
         const char*            name,
         number_member_getter_t getter,
         number_member_setter_t setter
     );

    /**
     * Create number entry.
     *
     * @param[in] name   Name of entry.
     * @param[in] getter Getter functional.
     * @param[in] setter Setter functional.
     * @returns @c *this for call chaining.
     **/
    this_t& number(
        const char*     name,
        number_getter_t getter,
        number_setter_t setter
    );

    // real (float)

    //! real Getter Functional
    typedef boost::function<
        long double(const configuration_data_t& data, const std::string&)
    > real_getter_t;
    //! real Setter Functional
    typedef boost::function<
        void(configuration_data_t& data, const std::string&, long double value)
    > real_setter_t;

    // real Getter Member
    typedef long double (configuration_data_t::*real_member_getter_t)(
        const std::string&
    ) const;
    // real Setter Member
    typedef void (configuration_data_t::*real_member_setter_t)(
        const std::string&,
        long double
    );

    /**
     * Create real entry --- data member.
     *
     * @a FieldType must be convertible to long double.
     *
     * @tparam FieldType Type of field.
     * @param[in] name   Name of entry.
     * @param[in] member Member pointer.
     *
     * @return @c *this for call chaining.
     **/
    template <typename FieldType>
    this_t& real(
        const char*                       name,
        FieldType configuration_data_t::* member
    );

    /**
     * Create real entry --- function member.
     *
     * @param[in] name   Name of entry.
     * @param[in] getter Getter member.
     * @param[in] setter Setter member.
     * @returns @c *this for call chaining.
     **/
     this_t& real(
         const char*            name,
         real_member_getter_t getter,
         real_member_setter_t setter
     );

    /**
     * Create real entry.
     *
     * @param[in] name   Name of entry.
     * @param[in] getter Getter functional.
     * @param[in] setter Setter functional.
     * @returns @c *this for call chaining.
     **/
    this_t& real(
        const char*     name,
        real_getter_t getter,
        real_setter_t setter
    );

    // Null String

    //! Null String Getter Functional
    typedef boost::function<
        const char*(const configuration_data_t& data, const std::string&)
    > null_string_getter_t;
    //! Null String Setter Functional
    typedef boost::function<
        void(
            configuration_data_t& data,
            const std::string&,
            const char* value
        )
    > null_string_setter_t;

    // Null String Getter Member
    typedef const char* (configuration_data_t::*null_string_member_getter_t)(
        const std::string&
    ) const;
    // Null String Setter Member
    typedef void (configuration_data_t::*null_string_member_setter_t)(
        const std::string&,
        const char*
    );

    /**
     * Create null string entry --- data member.
     *
     * @a FieldType must be convertible to const char*.
     *
     * @tparam FieldType Type of field.
     * @param[in] name   Name of entry.
     * @param[in] member Member pointer.
     *
     * @return @c *this for call chaining.
     **/
    template <typename FieldType>
    this_t& null_string(
        const char*                       name,
        FieldType configuration_data_t::* member
    );

    /**
     * Create null string entry --- function member.
     *
     * @param[in] name   Name of entry.
     * @param[in] getter Getter member.
     * @param[in] setter Setter member.
     * @returns @c *this for call chaining.
     **/
     this_t& null_string(
         const char*                 name,
         null_string_member_getter_t getter,
         null_string_member_setter_t setter
     );

    /**
     * Create null string entry.
     *
     * @param[in] name   Name of entry.
     * @param[in] getter Getter functional.
     * @param[in] setter Setter functional.
     * @returns @c *this for call chaining.
     **/
    this_t& null_string(
        const char*          name,
        null_string_getter_t getter,
        null_string_setter_t setter
    );

    // ByteString

    //! Byte String Getter Functional
    typedef boost::function<
        ConstByteString(const configuration_data_t& data, const std::string&)
    > byte_string_getter_t;
    //! Byte String Setter Functional
    typedef boost::function<
        void(
            configuration_data_t& data,
            const std::string&,
            ConstByteString value
        )
    > byte_string_setter_t;

    // Byte String Getter Member
    typedef ConstByteString
        (configuration_data_t::*byte_string_member_getter_t)(
            const std::string&
        ) const;
    // Byte String Setter Member
    typedef void (configuration_data_t::*byte_string_member_setter_t)(
        const std::string&,
        ConstByteString
    );

    //! Byte String Getter Functional &mdash std::string version
    typedef boost::function<
        std::string(
            const configuration_data_t& data, const std::string&
        )
    > byte_string_getter_s_t;
    //! Byte String Setter Functional &mdash std::string version
    typedef boost::function<
        void(
            configuration_data_t& data,
            const std::string&,
            const std::string&
        )
    > byte_string_setter_s_t;

    // Byte String Getter Member &mdash std::string version
    typedef const std::string&
        (configuration_data_t::*byte_string_member_getter_s_t)(
            const std::string&
        ) const;
    // Byte String Setter Member &mdash std::string version
    typedef void (configuration_data_t::*byte_string_member_setter_s_t)(
        const std::string&,
        const std::string&
    );

    /**
     * Create byte string entry --- data member.
     *
     * @a FieldType must be convertible to ConstByteString.
     *
     * @tparam FieldType Type of field.
     * @param[in] name   Name of entry.
     * @param[in] member Member pointer.
     *
     * @return @c *this for call chaining.
     **/
    template <typename FieldType>
    this_t& byte_string(
        const char*                       name,
        FieldType configuration_data_t::* member
    );

    /**
     * Create byte string entry --- function member.
     *
     * @param[in] name   Name of entry.
     * @param[in] getter Getter member.
     * @param[in] setter Setter member.
     * @returns @c *this for call chaining.
     **/
     this_t& byte_string(
         const char*                 name,
         byte_string_member_getter_t getter,
         byte_string_member_setter_t setter
     );

    /**
     * Create byte string entry.
     *
     * @param[in] name   Name of entry.
     * @param[in] getter Getter functional.
     * @param[in] setter Setter functional.
     * @returns @c *this for call chaining.
     **/
    this_t& byte_string(
        const char*          name,
        byte_string_getter_t getter,
        byte_string_setter_t setter
    );

    /**
     * Create byte string entry --- string data member.
     *
     * @a FieldType must be convertible to std::string.
     *
     * @tparam FieldType Type of field.
     * @param[in] name   Name of entry.
     * @param[in] member Member pointer.
     *
     * @return @c *this for call chaining.
     **/
    template <typename FieldType>
    this_t& byte_string_s(
        const char*                       name,
        FieldType configuration_data_t::* member
    );

    /**
     * Create byte string entry --- string function member.
     *
     * @param[in] name   Name of entry.
     * @param[in] getter Getter member.
     * @param[in] setter Setter member.
     * @returns @c *this for call chaining.
     **/
     this_t& byte_string_s(
         const char*                   name,
         byte_string_member_getter_s_t getter,
         byte_string_member_setter_s_t setter
     );

    /**
     * Create byte string entry.  String version.
     *
     * @param[in] name   Name of entry.
     * @param[in] getter Getter functional.
     * @param[in] setter Setter functional.
     * @returns @c *this for call chaining.
     **/
    this_t& byte_string_s(
        const char*          name,
        byte_string_getter_s_t getter,
        byte_string_setter_s_t setter
    );

    ///@}

private:
    /**
     * Adding an entry.
     *
     * This routine handles all the details of adding an entry.  The public
     * method translate their parameters and call this method.
     *
     * @tparam Getter Type of @a getter.
     * @tparam Setter Type of @a setter.
     * @param[in] name       Name of entry.
     * @param[in] getter     Getter for entry.
     * @param[in] setter     Setter for entry.
     * @param[in] field_type Field type for entry.
     **/
    template <typename Getter, typename Setter>
    void add_init(
        const char*   name,
        Getter        getter,
        Setter        setter,
        Field::type_e field_type
    );

    const ib_cfgmap_init_t*&    m_ib_init;
    MemoryManager               m_memory_manager;
    bool                        m_data_is_handle;
    std::list<ib_cfgmap_init_t> m_inits;
};

// Implementation.

namespace Internal {
/// @cond Internal

//! Type of getter translator.
typedef boost::function<
    void(const void*, void*, const ib_field_t*)
> configuration_map_init_getter_translator_t;

//! Type of setter translator.
typedef boost::function<
    void(void*, ib_field_t*, const void*)
> configuration_map_init_setter_translator_t;

/**
 * Set getter/setter translators for @a init.
 *
 * Translators are used to map between a generic C function signature suitable
 * for the C API and a typed (and templated) C++ functional signature.
 *
 * If @a data_is_handle is true then the data pointer passed to the callbacks
 * to the C API will be dereferenced once.  This is useful for supporting
 * Module::set_configuration_data() which uses handles.
 *
 * @param[in] init              Entry to set for.
 * @param[in] mm             Memory manager to use.
 * @param[in] getter_translator Getter translator.
 * @param[in] setter_translator Setter translator.
 * @param[in] data_is_handle    Is data from C API is a handle?
 **/
void set_configuration_map_init_translators(
    ib_cfgmap_init_t&                          init,
    ib_mm_t                                    mm,
    configuration_map_init_getter_translator_t getter_translator,
    configuration_map_init_setter_translator_t setter_translator,
    bool                                       data_is_handle
);

/**
 * Data member getter.
 *
 * This template implements a getter for use with data members.
 *
 * @tparam ConfigurationData Type of configuration data.
 * @tparam FieldType         Type of field.
 **/
template <typename ConfigurationData, typename FieldType>
class configuration_map_member_get
{
public:
    //! Member type.
    typedef FieldType ConfigurationData::* member_t;

    /**
     * Constructor.
     *
     * @param[in] member Pointer to member.
     **/
    explicit
    configuration_map_member_get(member_t member) :
        m_member(member)
    {
        // nop
    }

    //! Get data.
    FieldType operator()(
        const ConfigurationData& data,
        const std::string&
    ) const
    {
        return data.*m_member;
    }

private:
    member_t m_member;
};

/**
 * Data member setter.
 *
 * This template implements a setter for use with data members.
 *
 * @tparam ConfigurationData Type of configuration data.
 * @tparam FieldType         Type of field.
 **/
template <typename ConfigurationData, typename FieldType>
class configuration_map_member_set
{
public:
    //! Member type.
    typedef FieldType ConfigurationData::* member_t;

    /**
     * Constructor.
     *
     * @param[in] member Pointer to member.
     **/
    configuration_map_member_set(member_t member) :
        m_member(member)
    {
        // nop
    }

    //! Set data.
    void operator()(
        ConfigurationData& data,
        const std::string& name,
        FieldType          value
    ) const
    {
        data.*m_member = value;
    }

private:
    member_t m_member;
};

//! Specialization of above for ByteString.
template <typename ConfigurationData>
class configuration_map_member_set<ConfigurationData, ByteString>
{
public:
    //! Member type.
    typedef ByteString ConfigurationData::* member_t;

    /**
     * Constructor.
     *
     * @param[in] member Pointer to member.
     **/
    configuration_map_member_set(member_t member) :
        m_member(member)
    {
        // nop
    }

    //! Set data.
    void operator()(
        ConfigurationData& data,
        const std::string& name,
        ConstByteString    value
    ) const
    {
        (data.*m_member).clear();
        (data.*m_member).append(value);
    }

private:
    member_t m_member;
};

/**
 * Function member getter.
 *
 * This templates implements a setter for use with function members.
 *
 * @tparam ConfigurationData Type of configuration data.
 * @tparam FieldType         Type of field.
 **/
template <typename ConfigurationData, typename FieldType>
class configuration_map_internal_get
{
public:
    //! Member type.
    typedef FieldType (ConfigurationData::*getter_t)(
        const char*
    ) const;

    /**
     * Constructor.
     *
     * @param[in] getter Pointer to getter member.
     **/
    explicit
    configuration_map_internal_get(getter_t getter) :
        m_getter(getter)
    {
        // nop
    }

    //! Get data.
    FieldType operator()(
        const ConfigurationData& data,
        const std::string&       name
    ) const
    {
        return data->m_getter(name);
    }

private:
    getter_t m_getter;
};

/**
 * Function member getter.
 *
 * This templates implements a setter for use with function members.
 *
 * @tparam ConfigurationData Type of configuration data.
 * @tparam FieldType         Type of field.
 **/
template <typename ConfigurationData, typename FieldType>
class configuration_map_internal_set
{
public:
    //! Member type.
    typedef void (ConfigurationData::*setter_t)(
        const char*,
        FieldType value
    );

    /**
     * Constructor.
     *
     * @param[in] setter Pointer to setter member.
     **/
    configuration_map_internal_set(setter_t setter) :
        m_setter(setter)
    {
        // nop
    }

    //! Set data.
    void operator()(
        const ConfigurationData& data,
        const std::string&       name,
        FieldType                value
    ) const
    {
        data->m_setter(name, value);
    }

private:
    setter_t m_setter;
};

/**
 * Getter translator.
 *
 * This template facilitates storing type information for the C API callbacks.
 * It provides a generic interface suitable for being called from the C
 * engine, applies the type information, and calls the C++ functional.
 *
 * @tparam ConfigurationData Type of configuration data.
 **/
template <typename ConfigurationData>
class cfgmap_number_getter_translator
{
public:
    //! Getter type.
    typedef typename
        ConfigurationMapInit<
            ConfigurationData,
            typename boost::enable_if<boost::is_class<ConfigurationData> >::type
        >::number_getter_t getter_t;

    /**
     * Constructor.
     *
     * @param[in] getter Getter functional.
     **/
    explicit
    cfgmap_number_getter_translator(getter_t getter) :
        m_getter(getter)
    {
        // nop
    }

    //! Translate parameters and call getter.
    void operator()(
        const void*       base,
        void*             out_value,
        const ib_field_t* field
    ) const
    {
        assert(base != NULL);
        assert(field != NULL);
        assert(field->type == IB_FTYPE_NUM);
        assert(out_value != NULL);

        ib_num_t* n = reinterpret_cast<ib_num_t*>(out_value);
        *n = m_getter(
            *reinterpret_cast<const ConfigurationData*>(base),
            std::string(field->name, field->nlen)
        );
    }

private:
    getter_t m_getter;
};

/**
 * Setter translator.
 *
 * See cfgmap_number_getter_translator for discussion.
 *
 * @tparam ConfigurationData Type of configuration data.
 **/
template <typename ConfigurationData>
class cfgmap_number_setter_translator
{
public:
    //! Getter type.
    typedef typename
        ConfigurationMapInit<
            ConfigurationData,
            typename boost::enable_if<boost::is_class<ConfigurationData> >::type
        >::number_setter_t setter_t;

    /**
     * Constructor.
     *
     * @param[in] setter Setter functional.
     **/
    explicit
    cfgmap_number_setter_translator(setter_t setter) :
        m_setter(setter)
    {
        // nop
    }

    //! Translate parameters and call setter.
    void operator()(
        void*       base,
        ib_field_t* field,
        const void* value
    ) const
    {
        assert(base != NULL);
        assert(field != NULL);
        assert(value != NULL);
        assert(field->type == IB_FTYPE_NUM);

        m_setter(
            *reinterpret_cast<ConfigurationData*>(base),
            std::string(field->name, field->nlen),
            *reinterpret_cast<const int64_t*>(value)
        );
    }

private:
    setter_t m_setter;
};

/**
 * Getter translator.
 *
 * This template facilitates storing type information for the C API callbacks.
 * It provides a generic interface suitable for being called from the C
 * engine, applies the type information, and calls the C++ functional.
 *
 * @tparam ConfigurationData Type of configuration data.
 **/
template <typename ConfigurationData>
class cfgmap_real_getter_translator
{
public:
    //! Getter type.
    typedef typename
        ConfigurationMapInit<
            ConfigurationData,
            typename boost::enable_if<boost::is_class<ConfigurationData> >::type
        >::real_getter_t getter_t;

    /**
     * Constructor.
     *
     * @param[in] getter Getter functional.
     **/
    explicit
    cfgmap_real_getter_translator(getter_t getter) :
        m_getter(getter)
    {
        // nop
    }

    //! Translate parameters and call getter.
    void operator()(
        const void*       base,
        void*             out_value,
        const ib_field_t* field
    ) const
    {
        assert(base != NULL);
        assert(field != NULL);
        assert(field->type == IB_FTYPE_FLOAT);
        assert(out_value != NULL);

        ib_float_t* n = reinterpret_cast<ib_float_t*>(out_value);
        *n = m_getter(
            *reinterpret_cast<const ConfigurationData*>(base),
            std::string(field->name, field->nlen)
        );
    }

private:
    getter_t m_getter;
};

/**
 * Setter translator.
 *
 * See cfgmap_real_getter_translator for discussion.
 *
 * @tparam ConfigurationData Type of configuration data.
 **/
template <typename ConfigurationData>
class cfgmap_real_setter_translator
{
public:
    //! Getter type.
    typedef typename
        ConfigurationMapInit<
            ConfigurationData,
            typename boost::enable_if<boost::is_class<ConfigurationData> >::type
        >::real_setter_t setter_t;

    /**
     * Constructor.
     *
     * @param[in] setter Setter functional.
     **/
    explicit
    cfgmap_real_setter_translator(setter_t setter) :
        m_setter(setter)
    {
        // nop
    }

    //! Translate parameters and call setter.
    void operator()(
        void*       base,
        ib_field_t* field,
        const void* value
    ) const
    {
        assert(base != NULL);
        assert(field != NULL);
        assert(value != NULL);
        assert(field->type == IB_FTYPE_FLOAT);

        m_setter(
            *reinterpret_cast<ConfigurationData*>(base),
            std::string(field->name, field->nlen),
            *reinterpret_cast<const long double*>(value)
        );
    }

private:
    setter_t m_setter;
};

/**
 * Getter translator.
 *
 * See cfgmap_number_getter_translator for discussion.
 *
 * @tparam ConfigurationData Type of configuration data.
 **/
template <typename ConfigurationData>
class cfgmap_null_string_getter_translator
{
public:
    //! Getter type.
    typedef typename ConfigurationMapInit<
        ConfigurationData
    >::null_string_getter_t getter_t;

    /**
     * Constructor.
     *
     * @param[in] getter Getter functional.
     **/
    explicit
    cfgmap_null_string_getter_translator(getter_t getter) :
        m_getter(getter)
    {
        // nop
    }

    //! Translate parameters and call getter.
    void operator()(
        const void*       base,
        void*             out_value,
        const ib_field_t* field
    ) const
    {
        assert(base != NULL);
        assert(field != NULL);
        assert(field->type == IB_FTYPE_NULSTR);
        assert(out_value != NULL);

        const char** c = reinterpret_cast<const char**>(out_value);
        *c = m_getter(
            *reinterpret_cast<const ConfigurationData*>(base),
            std::string(field->name, field->nlen)
        );
    }

private:
    getter_t m_getter;
};

/**
 * Setter translator.
 *
 * See cfgmap_number_getter_translator for discussion.
 *
 * @tparam ConfigurationData Type of configuration data.
 **/
template <typename ConfigurationData>
class cfgmap_null_string_setter_translator
{
public:
    //! Getter type.
    typedef typename ConfigurationMapInit<
        ConfigurationData
    >::null_string_setter_t setter_t;

    /**
     * Constructor.
     *
     * @param[in] setter Setter functional.
     **/
    explicit
    cfgmap_null_string_setter_translator(setter_t setter) :
        m_setter(setter)
    {
        // nop
    }

    //! Translate parameters and call setter.
    void operator()(
        void*       base,
        ib_field_t* field,
        const void* value
    ) const
    {
        assert(base != NULL);
        assert(field != NULL);
        assert(value != NULL);
        assert(field->type == IB_FTYPE_NULSTR);

        m_setter(
            *reinterpret_cast<ConfigurationData*>(base),
            std::string(field->name, field->nlen),
            *reinterpret_cast<const char* const*>(value)
        );
    }

private:
    setter_t m_setter;
};

/**
 * Getter translator.
 *
 * See cfgmap_number_getter_translator for discussion.
 *
 * @tparam ConfigurationData Type of configuration data.
 **/
template <typename ConfigurationData>
class cfgmap_byte_string_getter_translator
{
public:
    //! Getter type.
    typedef typename ConfigurationMapInit<
        ConfigurationData
    >::byte_string_getter_t getter_t;

    /**
     * Constructor.
     *
     * @param[in] getter Getter functional.
     **/
    explicit
    cfgmap_byte_string_getter_translator(getter_t getter) :
        m_getter(getter)
    {
        // nop
    }

    //! Translate parameters and call getter.
    void operator()(
        const void*       base,
        void*             out_value,
        const ib_field_t* field
    ) const
    {
        assert(base != NULL);
        assert(field != NULL);
        assert(field->type == IB_FTYPE_BYTESTR);
        assert(out_value != NULL);

        const ib_bytestr_t** b =
            reinterpret_cast<const ib_bytestr_t**>(out_value);
        *b = m_getter(
            *reinterpret_cast<const ConfigurationData*>(base),
            std::string(field->name, field->nlen)
        ).ib();
    }

private:
    getter_t m_getter;
};

/**
 * Setter translator.
 *
 * See cfgmap_number_getter_translator for discussion.
 *
 * @tparam ConfigurationData Type of configuration data.
 **/
template <typename ConfigurationData>
class cfgmap_byte_string_setter_translator
{
public:
    //! Getter type.
    typedef typename ConfigurationMapInit<
        ConfigurationData
    >::byte_string_setter_t setter_t;

    /**
     * Constructor.
     *
     * @param[in] setter Setter functional.
     **/
    explicit
    cfgmap_byte_string_setter_translator(setter_t setter) :
        m_setter(setter)
    {
        // nop
    }

    //! Translate parameters and call setter.
    void operator()(
        void*       base,
        ib_field_t* field,
        const void* value
    ) const
    {
        assert(base != NULL);
        assert(field != NULL);
        assert(value != NULL);
        assert(field->type == IB_FTYPE_BYTESTR);

        m_setter(
            *reinterpret_cast<ConfigurationData*>(base),
            std::string(field->name, field->nlen),
            ConstByteString(
                *reinterpret_cast<const ib_bytestr_t* const*>(value)
            )
        );
    }

private:
    setter_t m_setter;
};

/**
 * Getter translator.
 *
 * See cfgmap_number_getter_translator for discussion.
 *
 * @tparam ConfigurationData Type of configuration data.
 **/
template <typename ConfigurationData>
class cfgmap_byte_string_getter_s_translator
{
public:
    //! Getter type.
    typedef typename ConfigurationMapInit<
        ConfigurationData
    >::byte_string_getter_s_t getter_t;

    /**
     * Constructor.
     *
     * @param[in] getter Getter functional.
     * @param[in] mm  Memory manager.
     **/
    cfgmap_byte_string_getter_s_translator(
        getter_t      getter,
        MemoryManager mm
    ) :
        m_getter(getter),
        m_mm(mm)
    {
        // nop
    }

    //! Translate parameters and call getter.
    void operator()(
        const void*       base,
        void*             out_value,
        const ib_field_t* field
    ) const
    {
        assert(base != NULL);
        assert(field != NULL);
        assert(field->type == IB_FTYPE_BYTESTR);
        assert(out_value != NULL);

        const ib_bytestr_t** b =
            reinterpret_cast<const ib_bytestr_t**>(out_value);
        *b = ByteString::create(m_mm,
            m_getter(
                *reinterpret_cast<const ConfigurationData*>(base),
                std::string(field->name, field->nlen)
            )
        ).ib();
    }

private:
    getter_t      m_getter;
    MemoryManager m_mm;
};

/**
 * Setter translator.
 *
 * See cfgmap_number_getter_translator for discussion.
 *
 * @tparam ConfigurationData Type of configuration data.
 **/
template <typename ConfigurationData>
class cfgmap_byte_string_setter_s_translator
{
public:
    //! Getter type.
    typedef typename ConfigurationMapInit<
        ConfigurationData
    >::byte_string_setter_s_t setter_t;

    /**
     * Constructor.
     *
     * @param[in] setter Setter functional.
     **/
    explicit
    cfgmap_byte_string_setter_s_translator(setter_t setter) :
        m_setter(setter)
    {
        // nop
    }

    //! Translate parameters and call setter.
    void operator()(
        void*       base,
        ib_field_t* field,
        const void* value
    ) const
    {
        assert(base != NULL);
        assert(field != NULL);
        assert(value != NULL);
        assert(field->type == IB_FTYPE_BYTESTR);

        m_setter(
            *reinterpret_cast<ConfigurationData*>(base),
            std::string(field->name, field->nlen),
            ConstByteString(
                *reinterpret_cast<const ib_bytestr_t* const*>(value)
            ).to_s()
        );
    }

private:
    setter_t m_setter;
};

/// @endcond
} // Internal

// Definitions

template <typename ConfigurationData>
ConfigurationMapInit<
    ConfigurationData,
    typename boost::enable_if<boost::is_class<ConfigurationData> >::type
>::ConfigurationMapInit(
    const ib_cfgmap_init_t*& ib_init,
    MemoryManager            memory_manager,
    bool                     data_is_handle
) :
    m_ib_init(ib_init),
    m_memory_manager(memory_manager),
    m_data_is_handle(data_is_handle)
{
    // nop
}

template <typename ConfigurationData>
ConfigurationMapInit<
    ConfigurationData,
    typename boost::enable_if<boost::is_class<ConfigurationData> >::type
>::~ConfigurationMapInit<
    ConfigurationData,
    typename boost::enable_if<boost::is_class<ConfigurationData> >::type
>()
{
    finish();
}

template <typename ConfigurationData>
template <typename FieldType>
typename ConfigurationMapInit<
    ConfigurationData,
    typename boost::enable_if<boost::is_class<ConfigurationData> >::type
>::this_t&
ConfigurationMapInit<
    ConfigurationData,
    typename boost::enable_if<boost::is_class<ConfigurationData> >::type
>::number(
    const char*                       name,
    FieldType configuration_data_t::* member
)
{
    BOOST_STATIC_ASSERT((
        boost::is_convertible<FieldType, int64_t>::value
    ));
    return number(
        name,
        Internal::configuration_map_member_get<
            ConfigurationData, FieldType
        >(
            member
        ),
        Internal::configuration_map_member_set<
            ConfigurationData, FieldType
        >(
            member
        )
    );
}

template <typename ConfigurationData>
typename ConfigurationMapInit<
    ConfigurationData,
    typename boost::enable_if<boost::is_class<ConfigurationData> >::type
>::this_t&
ConfigurationMapInit<
    ConfigurationData,
    typename boost::enable_if<boost::is_class<ConfigurationData> >::type
>::number(
    const char*            name,
    number_member_getter_t getter,
    number_member_setter_t setter
)
{
    return number(
        name,
        Internal::configuration_map_internal_get<
            ConfigurationData, int64_t
        >(getter),
        Internal::configuration_map_internal_set<
            ConfigurationData, int64_t
        >(setter)
    );
}

template <typename ConfigurationData>
typename ConfigurationMapInit<
    ConfigurationData,
    typename boost::enable_if<boost::is_class<ConfigurationData> >::type
>::this_t&
ConfigurationMapInit<
    ConfigurationData,
    typename boost::enable_if<boost::is_class<ConfigurationData> >::type
>::number(
    const char*     name,
    number_getter_t getter,
    number_setter_t setter
)
{
    add_init(
        name,
        Internal::cfgmap_number_getter_translator<
            ConfigurationData
        >(getter),
        Internal::cfgmap_number_setter_translator<
            ConfigurationData
        >(setter),
        Field::NUMBER
    );

    return *this;
}

template <typename ConfigurationData>
template <typename FieldType>
typename ConfigurationMapInit<
    ConfigurationData,
    typename boost::enable_if<boost::is_class<ConfigurationData> >::type
>::this_t&
ConfigurationMapInit<
    ConfigurationData,
    typename boost::enable_if<boost::is_class<ConfigurationData> >::type
>::real(
    const char*                       name,
    FieldType configuration_data_t::* member
)
{
    BOOST_STATIC_ASSERT((
        boost::is_convertible<FieldType, long double>::value
    ));
    return real(
        name,
        Internal::configuration_map_member_get<
            ConfigurationData, FieldType
        >(
            member
        ),
        Internal::configuration_map_member_set<
            ConfigurationData, FieldType
        >(
            member
        )
    );
}

template <typename ConfigurationData>
typename ConfigurationMapInit<
    ConfigurationData,
    typename boost::enable_if<boost::is_class<ConfigurationData> >::type
>::this_t&
ConfigurationMapInit<
    ConfigurationData,
    typename boost::enable_if<boost::is_class<ConfigurationData> >::type
>::real(
    const char*            name,
    real_member_getter_t getter,
    real_member_setter_t setter
)
{
    return real(
        name,
        Internal::configuration_map_internal_get<
            ConfigurationData, long double
        >(getter),
        Internal::configuration_map_internal_set<
            ConfigurationData, long double
        >(setter)
    );
}

template <typename ConfigurationData>
typename ConfigurationMapInit<
    ConfigurationData,
    typename boost::enable_if<boost::is_class<ConfigurationData> >::type
>::this_t&
ConfigurationMapInit<
    ConfigurationData,
    typename boost::enable_if<boost::is_class<ConfigurationData> >::type
>::real(
    const char*     name,
    real_getter_t getter,
    real_setter_t setter
)
{
    add_init(
        name,
        Internal::cfgmap_real_getter_translator<
            ConfigurationData
        >(getter),
        Internal::cfgmap_real_setter_translator<
            ConfigurationData
        >(setter),
        Field::FLOAT
    );

    return *this;
}

template <typename ConfigurationData>
template <typename FieldType>
typename ConfigurationMapInit<
    ConfigurationData,
    typename boost::enable_if<boost::is_class<ConfigurationData> >::type
>::this_t&
ConfigurationMapInit<
    ConfigurationData,
    typename boost::enable_if<boost::is_class<ConfigurationData> >::type
>::null_string(
    const char*                       name,
    FieldType configuration_data_t::* member
)
{
    BOOST_STATIC_ASSERT((
        boost::is_convertible<FieldType, const char*>::value
    ));
    return null_string(
        name,
        Internal::configuration_map_member_get<
            ConfigurationData, FieldType
        >(
            member
        ),
        Internal::configuration_map_member_set<
            ConfigurationData, FieldType
        >(
            member
        )
    );
}

template <typename ConfigurationData>
typename ConfigurationMapInit<
    ConfigurationData,
    typename boost::enable_if<boost::is_class<ConfigurationData> >::type
>::this_t&
ConfigurationMapInit<
    ConfigurationData,
    typename boost::enable_if<boost::is_class<ConfigurationData> >::type
>::null_string(
    const char*            name,
    null_string_member_getter_t getter,
    null_string_member_setter_t setter
)
{
    return null_string(
        name,
        Internal::configuration_map_internal_get<
            ConfigurationData, const char*
        >(getter),
        Internal::configuration_map_internal_set<
            ConfigurationData, const char*
        >(setter)
    );
}

template <typename ConfigurationData>
typename ConfigurationMapInit<
    ConfigurationData,
    typename boost::enable_if<boost::is_class<ConfigurationData> >::type
>::this_t&
ConfigurationMapInit<
    ConfigurationData,
    typename boost::enable_if<boost::is_class<ConfigurationData> >::type
>::null_string(
    const char*          name,
    null_string_getter_t getter,
    null_string_setter_t setter
)
{
    add_init(
        name,
        Internal::cfgmap_null_string_getter_translator<
            ConfigurationData
        >(getter),
        Internal::cfgmap_null_string_setter_translator<
            ConfigurationData
        >(setter),
        Field::NULL_STRING
    );

    return *this;
}

template <typename ConfigurationData>
template <typename FieldType>
typename ConfigurationMapInit<
    ConfigurationData,
    typename boost::enable_if<boost::is_class<ConfigurationData> >::type
>::this_t&
ConfigurationMapInit<
    ConfigurationData,
    typename boost::enable_if<boost::is_class<ConfigurationData> >::type
>::byte_string(
    const char*                       name,
    FieldType configuration_data_t::* member
)
{
    BOOST_STATIC_ASSERT((
        boost::is_convertible<FieldType, ByteString>::value
    ));
    return byte_string(
        name,
        Internal::configuration_map_member_get<
            ConfigurationData, FieldType
        >(
            member
        ),
        Internal::configuration_map_member_set<
            ConfigurationData, FieldType
        >(
            member
        )
    );
}

template <typename ConfigurationData>
typename ConfigurationMapInit<
    ConfigurationData,
    typename boost::enable_if<boost::is_class<ConfigurationData> >::type
>::this_t&
ConfigurationMapInit<
    ConfigurationData,
    typename boost::enable_if<boost::is_class<ConfigurationData> >::type
>::byte_string(
    const char*                 name,
    byte_string_member_getter_t getter,
    byte_string_member_setter_t setter
)
{
    return byte_string(
        name,
        Internal::configuration_map_internal_get<
            ConfigurationData, ConstByteString
        >(getter),
        Internal::configuration_map_internal_set<
            ConfigurationData, ConstByteString
        >(setter)
    );
}

template <typename ConfigurationData>
typename ConfigurationMapInit<
    ConfigurationData,
    typename boost::enable_if<boost::is_class<ConfigurationData> >::type
>::this_t&
ConfigurationMapInit<
    ConfigurationData,
    typename boost::enable_if<boost::is_class<ConfigurationData> >::type
>::byte_string(
    const char*          name,
    byte_string_getter_t getter,
    byte_string_setter_t setter
)
{
    add_init(
        name,
        Internal::cfgmap_byte_string_getter_translator<
            ConfigurationData
        >(getter),
        Internal::cfgmap_byte_string_setter_translator<
            ConfigurationData
        >(setter),
        Field::BYTE_STRING
    );

    return *this;
}


template <typename ConfigurationData>
template <typename FieldType>
typename ConfigurationMapInit<
    ConfigurationData,
    typename boost::enable_if<boost::is_class<ConfigurationData> >::type
>::this_t&
ConfigurationMapInit<
    ConfigurationData,
    typename boost::enable_if<boost::is_class<ConfigurationData> >::type
>::byte_string_s(
    const char*                       name,
    FieldType configuration_data_t::* member
)
{
    BOOST_STATIC_ASSERT((
        boost::is_convertible<FieldType, std::string>::value
    ));
    return byte_string_s(
        name,
        Internal::configuration_map_member_get<
            ConfigurationData, FieldType
        >(
            member
        ),
        Internal::configuration_map_member_set<
            ConfigurationData, FieldType
        >(
            member
        )
    );
}

template <typename ConfigurationData>
typename ConfigurationMapInit<
    ConfigurationData,
    typename boost::enable_if<boost::is_class<ConfigurationData> >::type
>::this_t&
ConfigurationMapInit<
    ConfigurationData,
    typename boost::enable_if<boost::is_class<ConfigurationData> >::type
>::byte_string_s(
    const char*                   name,
    byte_string_member_getter_s_t getter,
    byte_string_member_setter_s_t setter
)
{
    return byte_string_s(
        name,
        Internal::configuration_map_internal_get<
            ConfigurationData, std::string
        >(getter),
        Internal::configuration_map_internal_set<
            ConfigurationData, std::string
        >(setter)
    );
}

template <typename ConfigurationData>
typename ConfigurationMapInit<
    ConfigurationData,
    typename boost::enable_if<boost::is_class<ConfigurationData> >::type
>::this_t&
ConfigurationMapInit<
    ConfigurationData,
    typename boost::enable_if<boost::is_class<ConfigurationData> >::type
>::byte_string_s(
    const char*            name,
    byte_string_getter_s_t getter,
    byte_string_setter_s_t setter
)
{
    add_init(
        name,
        Internal::cfgmap_byte_string_getter_s_translator<
            ConfigurationData
        >(getter, m_memory_manager),
        Internal::cfgmap_byte_string_setter_s_translator<
            ConfigurationData
        >(setter),
        Field::BYTE_STRING
    );

    return *this;
}

template <typename ConfigurationData>
template <typename Getter, typename Setter>
void ConfigurationMapInit<
    ConfigurationData,
    typename boost::enable_if<boost::is_class<ConfigurationData> >::type
>::add_init(
    const char*   name,
    Getter        getter,
    Setter        setter,
    Field::type_e field_type
)
{
    ib_mm_t mm = m_memory_manager.ib();
    m_inits.push_back(ib_cfgmap_init_t());
    ib_cfgmap_init_t& init = m_inits.back();

    init.name = name;
    init.type = static_cast<ib_ftype_t>(field_type);

    set_configuration_map_init_translators(
        init,
        mm,
        getter,
        setter,
        m_data_is_handle
    );

    init.offset = 0;
    init.dlen = 0;
}

template <typename ConfigurationData>
void ConfigurationMapInit<
    ConfigurationData,
    typename boost::enable_if<boost::is_class<ConfigurationData> >::type
>::finish()
{
    if (m_inits.empty()) {
        return;
    }
    ib_cfgmap_init_t* ib_cmi = m_memory_manager.allocate<ib_cfgmap_init_t>(
        sizeof(*ib_cmi)*(m_inits.size()+1)
    );
    m_ib_init = ib_cmi;
    std::copy(m_inits.begin(), m_inits.end(), ib_cmi);
    ib_cmi[m_inits.size()].name = NULL;
    m_inits.clear();
}

} // IronBee

#endif
