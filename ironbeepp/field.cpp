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
 * @brief IronBee++ Field Implementation
 *
 * @sa field.hpp
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbeepp/field.hpp>
#include <ironbeepp/catch.hpp>
#include <ironbeepp/throw.hpp>

#include <ironbee/field.h>
#ifdef __clang__
#pragma clang diagnostic push
#if __has_warning("-Wunused-local-typedef")
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
#endif
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <cstring>
#include <cassert>

namespace IronBee {

namespace Internal {

std::string type_as_s(Field::type_e type)
{
    static const std::string generic("GENERIC");
    static const std::string number("NUMBER");
    static const std::string time("TIME");
    static const std::string float_s("FLOAT");
    static const std::string null_string("NULL STRING");
    static const std::string byte_string("BYTE STRING");
    static const std::string list("LIST");
    static const std::string stream_buffer("STREAM BUFFER");
    switch (type) {
        case Field::GENERIC: return generic;
        case Field::TIME: return time;
        case Field::NUMBER: return number;
        case Field::FLOAT: return float_s;
        case Field::NULL_STRING: return null_string;
        case Field::BYTE_STRING: return byte_string;
        case Field::LIST: return list;
        case Field::STREAM_BUFFER: return stream_buffer;
        default:
            return std::string("Unknown field type: ") +
                boost::lexical_cast<std::string>(type);
    }
}

std::string type_as_s(ib_ftype_t type)
{
    return type_as_s(static_cast<Field::type_e>(type));
}

void check_type(Field::type_e expected, Field::type_e actual)
{
    if (expected != actual) {
        BOOST_THROW_EXCEPTION(
           einval() << errinfo_what(
              "Expected field type " + type_as_s(expected) +
              " but is field type " + type_as_s(actual)
           )
        );
    }
}

void set_value(ib_field_t* f, void* in_value)
{
    ib_status_t rc = ib_field_setv(f, in_value);
    throw_if_error(rc);
}

void set_value(
    ib_field_t* f,
    void* in_value,
    const char* arg,
    size_t arg_length
)
{
    ib_status_t rc = ib_field_setv_ex(f, in_value, arg, arg_length);
    throw_if_error(rc);
}

void set_value_no_copy(ib_field_t* f, void* mutable_in_value)
{
    ib_status_t rc = ib_field_setv_no_copy(f, mutable_in_value);
    throw_if_error(rc);
}

namespace Hooks {
extern "C" {

ib_status_t ibpp_field_dynamic_get(
    const ib_field_t* field,
    void*             out_val,
    const void*       arg,
    size_t            arg_length,
    void*             cbdata
)
{
    // We will only pass this as as a const reference.
    ConstField fieldpp(field);

    const char* carg = reinterpret_cast<const char*>(arg);

    // No engine available.
    ib_status_t rc = IB_OK;
    try {
        switch (field->type) {
            case IB_FTYPE_TIME: {
                ib_time_t* n = reinterpret_cast<ib_time_t*>(out_val);
                *n = data_to_value<Field::time_get_t>(cbdata)(
                    fieldpp, carg, arg_length
                );
                return IB_OK;
            }
            case IB_FTYPE_NUM: {
                ib_num_t* n = reinterpret_cast<ib_num_t*>(out_val);
                *n = data_to_value<Field::number_get_t>(cbdata)(
                    fieldpp, carg, arg_length
                );
                return IB_OK;
            }
            case IB_FTYPE_FLOAT: {
                ib_float_t* u = reinterpret_cast<ib_float_t*>(out_val);
                *u = data_to_value<
                    Field::float_get_t
                >(cbdata)(
                    fieldpp, carg, arg_length
                );
                return IB_OK;
            }
            case IB_FTYPE_NULSTR:
            {
                const char** ns = reinterpret_cast<const char**>(out_val);
                *ns = data_to_value<
                    Field::null_string_get_t
                >(cbdata)(
                    fieldpp, carg, arg_length
                );
                return IB_OK;
            }
            case IB_FTYPE_BYTESTR:
            {
                const ib_bytestr_t** bs
                    = reinterpret_cast<const ib_bytestr_t**>(out_val);
                *bs = data_to_value<
                    Field::byte_string_get_t
                >(cbdata)(
                    fieldpp, carg, arg_length
                ).ib();
                return IB_OK;
            }
            case IB_FTYPE_LIST:
            {
                const ib_list_t** l
                    = reinterpret_cast<const ib_list_t**>(out_val);
                *l = data_to_value<
                    Internal::dynamic_list_getter_translator_t
                >(cbdata)(
                    fieldpp, carg, arg_length
                );
                return IB_OK;
            }
            default:
                BOOST_THROW_EXCEPTION(
                    einval() << errinfo_what(
                        "Unsupported field type: " + type_as_s(field->type)
                    )
                );
        }
    }
    catch (...) {
        return convert_exception();
    }

    // If we got here, it is in error.
    assert(rc != IB_OK);
    return rc;
}

ib_status_t ibpp_field_dynamic_set(
    ib_field_t* field,
    const void* arg,
    size_t      arg_length,
    void*       in_value,
    void*       cbdata
)
{
    const char* carg = reinterpret_cast<const char*>(arg);

    // No engine available.
    try {
        switch (field->type) {
            case IB_FTYPE_TIME:
                data_to_value<Field::time_set_t>(cbdata)(
                    Field(field),
                    carg, arg_length,
                    *reinterpret_cast<const uint64_t*>(in_value)
                );
                break;
            case IB_FTYPE_NUM:
                data_to_value<Field::number_set_t>(cbdata)(
                    Field(field),
                    carg, arg_length,
                    *reinterpret_cast<const int64_t*>(in_value)
                );
                break;
            case IB_FTYPE_FLOAT:
                data_to_value<Field::float_set_t>(cbdata)(
                    Field(field),
                    carg, arg_length,
                    *reinterpret_cast<const long double*>(in_value)
                );
                break;
            case IB_FTYPE_NULSTR:
                data_to_value<Field::null_string_set_t>(cbdata)(
                    Field(field),
                    carg, arg_length,
                    reinterpret_cast<const char*>(in_value)
                );
                break;
            case IB_FTYPE_BYTESTR: {
                // Const cast, but then immeidately store as const.
                const ConstByteString value(
                    reinterpret_cast<const ib_bytestr_t*>(in_value)
                );
                data_to_value<Field::byte_string_set_t>(cbdata)(
                    Field(field),
                    carg, arg_length,
                    value
                );
                break;
            }
            case IB_FTYPE_LIST: {
                const ib_list_t* value =
                    reinterpret_cast<const ib_list_t*>(in_value);
                data_to_value<
                    Internal::dynamic_list_setter_translator_t
                >(cbdata)(
                    Field(field),
                    carg, arg_length,
                    value
                );
                break;
            }
            default:
                BOOST_THROW_EXCEPTION(
                    einval() << errinfo_what(
                        "Unsupported field type: " + type_as_s(field->type)
                    )
                );
        }
    }
    catch (...) {
        return convert_exception();
    }
    return IB_OK;
}

} // extern "C"
} // Hooks

Field create_field(
    MemoryManager mm,
    const char*   name,
    size_t        name_length,
    Field::type_e type,
    void*         in_value
)
{
    ib_field_t* f = NULL;

    ib_status_t rc = ib_field_create(
        &f,
        mm.ib(),
        name, name_length,
        static_cast<ib_ftype_t>(type),
        in_value
    );
    throw_if_error(rc);

    return Field(f);
}

Field create_no_copy(
    MemoryManager mm,
    const char*   name,
    size_t        name_length,
    Field::type_e type,
    void*         mutable_in_value
)
{
    ib_field_t* f = NULL;

    ib_status_t rc = ib_field_create_no_copy(
        &f,
        mm.ib(),
        name, name_length,
        static_cast<ib_ftype_t>(type),
        mutable_in_value
    );
    throw_if_error(rc);

    return Field(f);
}

Field create_alias(
    MemoryManager mm,
    const char*   name,
    size_t        name_length,
    Field::type_e type,
    void*         mutable_out_value
)
{
    ib_field_t* f = NULL;

    ib_status_t rc = ib_field_create_alias(
        &f,
        mm.ib(),
        name, name_length,
        static_cast<ib_ftype_t>(type),
        mutable_out_value
    );
    throw_if_error(rc);

    return Field(f);
}

Field create_dynamic_field(
    MemoryManager mm,
    const char*   name,
    size_t        name_length,
    Field::type_e type,
    void*         cbdata_get,
    void*         cbdata_set
)
{
    ib_field_t* f = NULL;

    ib_status_t rc = ib_field_create_dynamic(
        &f,
        mm.ib(),
        name, name_length,
        static_cast<ib_ftype_t>(type),
        Hooks::ibpp_field_dynamic_get,
        cbdata_get,
        Hooks::ibpp_field_dynamic_set,
        cbdata_set
    );
    throw_if_error(rc);

    return Field(f);
}

} // Internal

/* ConstField */

ConstField::ConstField() :
    m_ib(NULL)
{
    // nop
}

ConstField::ConstField(const ib_field_t* ib_field) :
    m_ib(ib_field)
{
    // nop
}

Field ConstField::dup(MemoryManager mm) const
{
    return dup(mm, name(), name_length());
}

Field ConstField::dup() const
{
    return dup(memory_manager());
}

Field ConstField::dup(
    MemoryManager mm,
    const char*   new_name,
    size_t        new_name_length
) const
{
    ib_field_t* f = NULL;

    ib_status_t rc = ib_field_copy(
        &f,
        mm.ib(),
        new_name, new_name_length,
        ib()
    );
    throw_if_error(rc);

    return Field(f);
}

Field ConstField::dup(const char* new_name, size_t new_name_length) const
{
    return dup(memory_manager(), new_name, new_name_length);
}

const char* ConstField::name() const
{
    return ib()->name;
}

size_t ConstField::name_length() const
{
    return ib()->nlen;
}

std::string ConstField::name_as_s() const
{
    return std::string(name(), name_length());
}

ConstField::type_e ConstField::type() const
{
    return static_cast<ConstField::type_e>(ib()->type);
}

MemoryManager ConstField::memory_manager() const
{
    return MemoryManager(ib()->mm);
}

std::string ConstField::to_s() const
{
    switch (type()) {
        case TIME:
            return boost::lexical_cast<std::string>(value_as_time());
        case NUMBER:
            return boost::lexical_cast<std::string>(value_as_number());
        case FLOAT:
            // lexical_cast may not include decimal point.
            return (boost::format("%f") %  value_as_float()).str();
        case NULL_STRING:
            return std::string(value_as_null_string());
        case BYTE_STRING:
            return value_as_byte_string().to_s();
        default:
            BOOST_THROW_EXCEPTION(
                einval() << errinfo_what(
                    "ConstField::to_s() does not support field type: " +
                    Internal::type_as_s(type())
                )
            );
    }
}

bool ConstField::is_dynamic() const
{
    return ib_field_is_dynamic(ib()) == 1;
}

uint64_t ConstField::value_as_time() const
{
    Internal::check_type(TIME, type());
    uint64_t v;
    throw_if_error(ib_field_value(ib(), ib_ftype_time_out(&v)));
    return v;
}

uint64_t ConstField::value_as_time(const std::string& arg) const
{
    return value_as_time(arg.data(),arg.length());
}

uint64_t ConstField::value_as_time(
    const char* arg,
    size_t      arg_length
) const
{
    Internal::check_type(TIME, type());
    uint64_t v;
    throw_if_error(ib_field_value_ex(
        ib(), ib_ftype_time_out(&v),
        arg, arg_length
    ));
    return v;
}

int64_t ConstField::value_as_number() const
{
    Internal::check_type(NUMBER, type());
    int64_t v;
    throw_if_error(ib_field_value(ib(), ib_ftype_num_out(&v)));
    return v;
}

int64_t ConstField::value_as_number(const std::string& arg) const
{
    return value_as_number(arg.data(),arg.length());
}

int64_t ConstField::value_as_number(
    const char* arg,
    size_t      arg_length
) const
{
    Internal::check_type(NUMBER, type());
    int64_t v;
    throw_if_error(ib_field_value_ex(
        ib(), ib_ftype_num_out(&v),
        arg, arg_length
    ));
    return v;
}

long double ConstField::value_as_float() const
{
    Internal::check_type(FLOAT, type());
    long double v;
    throw_if_error(ib_field_value(ib(), ib_ftype_float_out(&v)));
    return v;
}

long double ConstField::value_as_float(const std::string& arg) const
{
    return value_as_float(arg.data(), arg.length());
}

long double ConstField::value_as_float(
    const char* arg,
    size_t      arg_length
) const
{
    Internal::check_type(FLOAT, type());
    long double v;
    throw_if_error(ib_field_value_ex(
        ib(), ib_ftype_float_out(&v),
        arg, arg_length
    ));
    return v;
}

const char* ConstField::value_as_null_string() const
{
    Internal::check_type(NULL_STRING, type());
    const char* v;
    throw_if_error(ib_field_value(
        ib(), ib_ftype_nulstr_out(&v)
    ));
    return v;
}

const char* ConstField::value_as_null_string(const std::string& arg) const
{
    return value_as_null_string(arg.data(), arg.length());
}

const char* ConstField::value_as_null_string(
    const char* arg,
    size_t      arg_length
) const
{
    Internal::check_type(NULL_STRING, type());
    const char* v;
    throw_if_error(ib_field_value_ex(
        ib(), ib_ftype_nulstr_out(&v),
        arg, arg_length
    ));
    return v;
}

ConstByteString ConstField::value_as_byte_string() const
{
    Internal::check_type(BYTE_STRING, type());
    const ib_bytestr_t* v;
    throw_if_error(ib_field_value(
        ib(), ib_ftype_bytestr_out(&v)
    ));
    return ConstByteString(v);
}

ConstByteString ConstField::value_as_byte_string(const std::string& arg) const
{
    return value_as_byte_string(arg.data(), arg.length());
}

ConstByteString ConstField::value_as_byte_string(
    const char* arg,
    size_t      arg_length
) const
{
    Internal::check_type(BYTE_STRING, type());
    const ib_bytestr_t* v;
    throw_if_error(ib_field_value_ex(
        ib(), ib_ftype_bytestr_out(&v),
        arg, arg_length
    ));
    return ConstByteString(v);
}

/* Field */

// See api documentation for discussion of const_cast.
Field Field::remove_const(const ConstField& field)
{
    return Field(const_cast<ib_field_t*>(field.ib()));
}

Field::Field() :
    m_ib(NULL)
{
    // nop
}

Field Field::create_time(
    MemoryManager mm,
    const char*   name,
    size_t        name_length,
    uint64_t      value
)
{
    return Internal::create_field(
        mm,
        name, name_length,
        Field::TIME,
        ib_ftype_time_in(&value)
    );
}

Field Field::create_number(
    MemoryManager mm,
    const char*   name,
    size_t        name_length,
    int64_t       value
)
{
    return Internal::create_field(
        mm,
        name, name_length,
        Field::NUMBER,
        ib_ftype_num_in(&value)
    );
}

Field Field::create_float(
    MemoryManager mm,
    const char*   name,
    size_t        name_length,
    long double   value
)
{
    return Internal::create_field(
        mm,
        name, name_length,
        Field::FLOAT,
        ib_ftype_float_in(&value)
    );
}

Field Field::create_null_string(
    MemoryManager mm,
    const char*   name,
    size_t        name_length,
    const char*   value
)
{
    return Internal::create_field(
        mm,
        name, name_length,
        Field::NULL_STRING,
        ib_ftype_nulstr_in(value)
    );
}

Field Field::create_byte_string(
    MemoryManager   mm,
    const char*     name,
    size_t          name_length,
    ConstByteString value
)
{
    return Internal::create_field(
        mm,
        name, name_length,
        Field::BYTE_STRING,
        ib_ftype_bytestr_in(value.ib())
    );
}

Field Field::create_no_copy_null_string(
    MemoryManager mm,
    const char*   name,
    size_t        name_length,
    char*         value
)
{
    return Internal::create_no_copy(
        mm,
        name, name_length,
        Field::NULL_STRING,
        ib_ftype_nulstr_mutable_in(value)
    );
}

Field Field::create_no_copy_byte_string(
    MemoryManager mm,
    const char*   name,
    size_t        name_length,
    ByteString    value
)
{
    return Internal::create_no_copy(
        mm,
        name, name_length,
        Field::BYTE_STRING,
        ib_ftype_bytestr_mutable_in(value.ib())
    );
}

Field Field::create_alias_time(
    MemoryManager mm,
    const char*   name,
    size_t        name_length,
    uint64_t&     value
)
{
    return Internal::create_alias(
        mm,
        name, name_length,
        Field::TIME,
        ib_ftype_time_storage(&value)
    );
}

Field Field::create_alias_number(
    MemoryManager mm,
    const char*   name,
    size_t        name_length,
    int64_t&      value
)
{
    return Internal::create_alias(
        mm,
        name, name_length,
        Field::NUMBER,
        ib_ftype_num_storage(&value)
    );
}

Field Field::create_alias_float(
    MemoryManager mm,
    const char*   name,
    size_t        name_length,
    long double&  value
)
{
    return Internal::create_alias(
        mm,
        name, name_length,
        Field::FLOAT,
        ib_ftype_float_storage(&value)
    );
}

Field Field::create_alias_null_string(
    MemoryManager mm,
    const char*   name,
    size_t        name_length,
    char*&        value
)
{
    return Internal::create_alias(
        mm,
        name, name_length,
        Field::NULL_STRING,
        ib_ftype_nulstr_storage(&value)
    );
}

Field Field::create_alias_byte_string(
    MemoryManager  mm,
    const char*    name,
    size_t         name_length,
    ib_bytestr_t*& value
)
{
    // ByteString is a friend.
    return Internal::create_alias(
        mm,
        name, name_length,
        Field::BYTE_STRING,
        ib_ftype_bytestr_storage(&value)
    );
}

Field Field::create_alias_list(
    MemoryManager mm,
    const char*   name,
    size_t        name_length,
    ib_list_t*&   value
)
{
    // ByteString is a friend.
    return Internal::create_alias(
        mm,
        name, name_length,
        Field::LIST,
        ib_ftype_list_storage(&value)
    );
}

Field Field::create_dynamic_time(
    MemoryManager mm,
    const char*   name,
    size_t        name_length,
    time_get_t    get,
    time_set_t    set
)
{
    return Internal::create_dynamic_field(
        mm,
        name, name_length,
        Field::TIME,
        value_to_data(get, mm.ib()),
        value_to_data(set, mm.ib())
    );
}

Field Field::create_dynamic_number(
    MemoryManager mm,
    const char*   name,
    size_t        name_length,
    number_get_t  get,
    number_set_t  set
)
{
    return Internal::create_dynamic_field(
        mm,
        name, name_length,
        Field::NUMBER,
        value_to_data(get, mm.ib()),
        value_to_data(set, mm.ib())
    );
}

Field Field::create_dynamic_float(
    MemoryManager mm,
    const char*   name,
    size_t        name_length,
    float_get_t   get,
    float_set_t   set
)
{
    return Internal::create_dynamic_field(
        mm,
        name, name_length,
        Field::FLOAT,
        value_to_data(get, mm.ib()),
        value_to_data(set, mm.ib())
    );
}

Field Field::create_dynamic_null_string(
    MemoryManager     mm,
    const char*       name,
    size_t            name_length,
    null_string_get_t get,
    null_string_set_t set
)
{
    return Internal::create_dynamic_field(
        mm,
        name, name_length,
        Field::NULL_STRING,
        value_to_data(get, mm.ib()),
        value_to_data(set, mm.ib())
    );
}

Field Field::create_dynamic_byte_string(
    MemoryManager     mm,
    const char*       name,
    size_t            name_length,
    byte_string_get_t get,
    byte_string_set_t set
)
{
    return Internal::create_dynamic_field(
        mm,
        name, name_length,
        Field::BYTE_STRING,
        value_to_data(get, mm.ib()),
        value_to_data(set, mm.ib())
    );
}

void Field::set_time(uint64_t value) const
{
    Internal::check_type(TIME, type());
    Internal::set_value(ib(), ib_ftype_time_in(&value));
}

void Field::set_time(uint64_t value, const std::string& arg) const
{
    return set_time(value, arg.data(), arg.length());
}

void Field::set_time(
    uint64_t value,
    const char* arg, size_t arg_length
) const
{
    Internal::check_type(TIME, type());
    Internal::set_value(ib(), ib_ftype_time_in(&value), arg, arg_length);
}

void Field::set_number(int64_t value) const
{
    Internal::check_type(NUMBER, type());
    Internal::set_value(ib(), ib_ftype_num_in(&value));
}

void Field::set_number(int64_t value, const std::string& arg) const
{
    return set_number(value, arg.data(), arg.length());
}

void Field::set_number(
    int64_t value,
    const char* arg, size_t arg_length
) const
{
    Internal::check_type(NUMBER, type());
    Internal::set_value(ib(), ib_ftype_num_in(&value), arg, arg_length);
}

void Field::set_float(long double value) const
{
    Internal::check_type(FLOAT, type());
    Internal::set_value(ib(), ib_ftype_float_in(&value));
}
void Field::set_float(long double value, const std::string& arg) const
{
    return set_float(value, arg.data(), arg.length());
}

void Field::set_float(
    long double value,
    const char* arg, size_t arg_length
) const
{
    Internal::check_type(FLOAT, type());
    Internal::set_value(ib(), ib_ftype_float_in(&value), arg, arg_length);
}

void Field::set_null_string(const char* value) const
{
    Internal::check_type(NULL_STRING, type());
    Internal::set_value(ib(), ib_ftype_nulstr_in(value));
}
void Field::set_null_string(const char* value, const std::string& arg) const
{
    return set_null_string(value, arg.data(), arg.length());
}

void Field::set_null_string(
    const char* value,
    const char* arg, size_t arg_length
) const
{
    Internal::check_type(NULL_STRING, type());
    Internal::set_value(
        ib(),
        ib_ftype_nulstr_in(value),
        arg, arg_length
    );
}

void Field::set_byte_string(ConstByteString value) const
{
    Internal::check_type(BYTE_STRING, type());
    Internal::set_value(ib(), ib_ftype_bytestr_in(value.ib()));
}
void Field::set_byte_string(ConstByteString value, const std::string& arg) const
{
    return set_byte_string(value, arg.data(), arg.length());
}

void Field::set_byte_string(
    ConstByteString value,
    const char* arg,
    size_t arg_length
) const
{
    Internal::check_type(BYTE_STRING, type());
    Internal::set_value(
        ib(), ib_ftype_bytestr_in(value.ib()),
        arg, arg_length
    );
}

void Field::set_no_copy_null_string(char* value) const
{
    Internal::check_type(NULL_STRING, type());
    Internal::set_value_no_copy(
        ib(), ib_ftype_nulstr_mutable_in(value)
    );
}

void Field::set_no_copy_byte_string(ByteString value) const
{
    Internal::check_type(BYTE_STRING, type());
    Internal::set_value_no_copy(
        ib(), ib_ftype_bytestr_mutable_in(value.ib())
    );
}

uint64_t& Field::mutable_value_as_time() const
{
    Internal::check_type(TIME, type());
    ib_time_t* n;
    throw_if_error(ib_field_mutable_value(ib(),
        ib_ftype_time_mutable_out(&n)
    ));
    return *n;
}

int64_t& Field::mutable_value_as_number() const
{
    Internal::check_type(NUMBER, type());
    ib_num_t* n;
    throw_if_error(ib_field_mutable_value(ib(),
        ib_ftype_num_mutable_out(&n)
    ));
    return *n;
}

long double& Field::mutable_value_as_float() const
{
    Internal::check_type(FLOAT, type());
    ib_float_t* n;
    throw_if_error(ib_field_mutable_value(ib(),
        ib_ftype_float_mutable_out(&n)
    ));
    return *n;
}

char* Field::mutable_value_as_null_string() const
{
    Internal::check_type(NULL_STRING, type());
    char* cs;
    throw_if_error(ib_field_mutable_value(ib(),
        ib_ftype_nulstr_mutable_out(&cs)
    ));
    return cs;
}

ByteString Field::mutable_value_as_byte_string() const
{
    Internal::check_type(BYTE_STRING, type());
    ib_bytestr_t* bs;
    throw_if_error(ib_field_mutable_value(ib(),
        ib_ftype_bytestr_mutable_out(&bs)
    ));
    return ByteString(bs);
}

void Field::make_static() const
{
    throw_if_error(ib_field_make_static(ib()));
}

Field::Field(ib_field_t* ib_field) :
    ConstField(ib_field),
    m_ib(ib_field)
{
    // nop
}

/* Global */

std::ostream& operator<<(std::ostream& o, const ConstField& field)
{
    if (! field) {
        o << "IronBee::Field[!singular!]";
    }
    else {
        o << "IronBee::Field["
          << std::string(field.name(), field.name_length())
          << " = " << field.to_s() << "]";
    }

    return o;
}

} // IronBee
