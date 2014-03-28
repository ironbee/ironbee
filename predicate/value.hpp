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
 * @brief Predicate --- Value Utilities
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __PREDICATE__VALUE__
#define __PREDICATE__VALUE__

#include <predicate/ironbee.hpp>

#include <iostream>
#include <string>

namespace IronBee {
namespace Predicate {

/**
 * A Value in Predicate.
 *
 * This class is based on and similar to Field and ConstField.  In contrast
 * to Field, it provides the subset of functionality useful to Predicate and
 * some additional, Predicate  specific functionality: namely truthiness and 
 * sexpr support.
 *
 * @sa ConstField
 * @sa Field
 **/
class Value
{
public:
    //! Underlying IronBee type.
    typedef const ib_field_t* ib_type;
    
    //! Types of values.
    enum type_e {
        NUMBER = ConstField::NUMBER,
        FLOAT  = ConstField::FLOAT,
        STRING = ConstField::BYTE_STRING,
        LIST   = ConstField::LIST
    };
    
    /**
     * Construct singular value.
     *
     * Only assignment, to_s(), to_field(), ib(), and conversion to bool are 
     * supported.  All other operations are undefined.
     **/
    Value();
    
    //! Construct from ConstField.
    explicit
    Value(ConstField field);
    
    //! Construct from C type.
    explicit
    Value(ib_type ib);

    // Intentionally inline.
    //! Convert to ConstField.
    ConstField to_field() const
    {
        return m_field;
    }
    
    //! Access underlying C type.
    ib_type ib() const
    {
        return m_field.ib();
    }
    
    /**
     * @name Creation
     * Routines for creating new values.
     *
     * Nameless versions use the empty name.  Named versions require that
     * the name parameter last at least as long as `mm`.  Usually, this
     * means either a C string literal or a call to MemoryManager::strdup().
     **/
    ///@{
    
    /**
     * Create number value.
     *
     * @param[in] mm Memory manager defining value lifetime.
     * @param[in] num Number value.
     * @return New value.
     **/
    static
    Value create_number(
        MemoryManager mm,
        int64_t       num
    );
    
   /**
    * Create number value with name.
    *
    * @param[in] mm Memory manager defining value lifetime.
    * @param[in] name Name of value.  Must live at least as long as @a mm.
    * @param[in] name_length Length of @a name.
    * @param[in] num Number value.
    * @return New value.
    **/
    static
    Value create_number(
        MemoryManager mm,
        const char*   name,
        size_t        name_length,
        int64_t       num
    );

    /**
     * Create float value.
     *
     * @param[in] mm Memory manager defining value lifetime.
     * @param[in] f Float value.
     * @return New value.
     **/
    static
    Value create_float(
        MemoryManager mm,
        long double   f
    );
    
   /**
    * Create float value with name.
    *
    * @param[in] mm Memory manager defining value lifetime.
    * @param[in] name Name of value.  Must live at least as long as @a mm.
    * @param[in] name_length Length of @a name.
    * @param[in] f Float value.
    * @return New value.
    **/
    static
    Value create_float(
        MemoryManager mm,
        const char*   name,
        size_t        name_length,
        long double   f
    );

    /**
     * Create string value.
     *
     * @param[in] mm Memory manager defining value lifetime.
     * @param[in] s String value.
     * @return New value.
     **/
    static
    Value create_string(
        MemoryManager   mm,
        ConstByteString s
    );
    
   /**
    * Create string value with name.
    *
    * @param[in] mm Memory manager defining value lifetime.
    * @param[in] name Name of value.  Must live at least as long as @a mm.
    * @param[in] name_length Length of @a name.
    * @param[in] s String value.
    * @return New value.
    **/
    static
    Value create_string(
        MemoryManager   mm,
        const char*     name,
        size_t          name_length,
        ConstByteString s
    );

    /**
     * Alias list value.
     *
     * @param[in] mm Memory manager defining value lifetime.
     * @param[in] l  List to alias.
     * @return New value.
     **/
    static
    Value alias_list(
        MemoryManager    mm,
        ConstList<Value> l
    );
    
   /**
    * Alias list value with name.
    *
    * @param[in] mm Memory manager defining value lifetime.
    * @param[in] name Name of value.  Must live at least as long as @a mm.
    * @param[in] name_length Length of @a name.
    * @param[in] l List to alias.
    * @return New value.
    **/
    static
    Value alias_list(
        MemoryManager    mm,
        const char*      name,
        size_t           name_length,
        ConstList<Value> l
    );

    ///@}

    /**
     * Duplicate value.
     *
     * Unlike Field::dup(), this method does deep copying of lists taking
     * advantage of knowing that all lists are lists of values.
     *
     * This method is useful when a a Value needs its lifetime adjusted.
     *
     * @param[in] mm Memory manager defining new lifetime.
     * @return New value identical except for lifetime.
     **/
    Value dup(
        MemoryManager mm
    ) const;

    /**
     * Duplicate value, adjusting name.
     *
     * Unlike Field::dup(), this method does deep copying of lists taking
     * advantage of knowing that all lists are lists of values.
     *
     * This method is useful when a a Value needs its lifetime adjusted.
     *
     * @param[in] name New name.
     * @param[in] name_length Length of @a name.
     *
     * @param[in] mm Memory manager defining new lifetime.
     * @return New value identical except for name and  lifetime.
     **/
    Value dup(
        MemoryManager mm,
        const char*   name,
        size_t        name_length
    ) const;
    
    ///@cond Internal
    typedef void (*unspecified_bool_type)(Value***);
    ///@endcond
    
    /**
     * Convert to bool.
     *
     * Singular values and empty lists are false.  All others are true.
     *
     * @return Iff value should be treated as truthy.
     **/
    operator unspecified_bool_type() const;
    
    /**
     * Convert to sexpr.
     *
     * @return Sexpr representing value.
     **/
    const std::string to_s() const;
    
    /**
     * @name Value getters.
     *
     * All getters throw @ref einval if the wrong getter is used for the type
     * or if singular.
     **/
    ///@{
    
    //! Value as number.
    int64_t as_number() const;
    //! Value as float.
    long double as_float() const;
    //! Value as string.
    ConstByteString as_string() const;
    //! Value as list.
    ConstList<Value> as_list() const;
    
    //@}
    
    //! Name.
    const char* name() const;
    //! Length of name.
    size_t name_length() const;
    
    //! Type of value.
    type_e type() const;

private:
    // Used for unspecified_bool_type.
    static void unspecified_bool(Value***) {};
    
    //! Underlying field.
    ConstField m_field;
};

/**
 * Ostream Outputter.
 *
 * @param[in] o Where to output.
 * @param[in] v What to output.
 * @return o
 **/
std::ostream& operator<<(std::ostream& o, const Value& v);

} // Predicate
} // IronBee

#endif
