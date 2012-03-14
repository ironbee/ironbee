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
 * @internal
 *
 * @sa field.hpp
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbeepp/field.hpp>
#include <ironbeepp/internal/catch.hpp>
#include <ironbeepp/internal/throw.hpp>
#include <ironbeepp/internal/data.hpp>

#include <ironbee/field.h>
#include <ironbee/debug.h>

#include <boost/lexical_cast.hpp>

#include <cstring>
#include <cassert>

namespace IronBee {

namespace Internal {
namespace {

Field create_field(
    MemoryPool    pool,
    const char*   name,
    size_t        name_length,
    Field::type_e type,
    const void*   value
)
{
    ib_field_t* f = NULL;

    ib_status_t rc = ib_field_create_ex(
        &f,
        pool.ib(),
        name, name_length,
        type,
        value
    );
    Internal::throw_if_error(rc);

    return Field(f);
}

Field create_alias(
    MemoryPool    pool,
    const char*   name,
    size_t        name_length,
    Field::type_e type,
    void*         value
)
{
    ib_field_t* f = NULL;

    ib_status_t rc = ib_field_createn_ex(
        &f,
        pool.ib(),
        name, name_length,
        type,
        value
    );
    Internal::throw_if_error(rc);

    return Field(f);
}

std::string type_as_s(Field::type_e type)
{
    static const std::string generic("GENERIC");
    static const std::string number("NUMBER");
    static const std::string unsigned_number("UNSIGNED NUMBER");
    static const std::string null_string("NULL STRING");
    static const std::string byte_string("BYTE STRING");
    static const std::string list("LIST");
    static const std::string stream_buffer("STREAM BUFFER");
    switch (type) {
        case Field::GENERIC: return generic;
        case Field::NUMBER: return number;
        case Field::UNSIGNED_NUMBER: return unsigned_number;
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

void set_value(ib_field_t* f, const void* value)
{
    ib_status_t rc = ib_field_setv(f, value);
    Internal::throw_if_error(rc);
}

void set_value(
    ib_field_t* f,
    const void* value,
    const char* arg,
    size_t arg_length
)
{
    ib_status_t rc = ib_field_setv_ex(f, value, arg, arg_length);
    Internal::throw_if_error(rc);
}

void set_value_static(ib_field_t* f, const void* value)
{
    ib_status_t rc = ib_field_setv_static(f, value);
    Internal::throw_if_error(rc);
}

namespace Hooks {
extern "C" {

const void* field_dynamic_get(
    const ib_field_t* field,
    const void*       arg,
    size_t            arg_length,
    void*             cbdata
)
{
    IB_FTRACE_INIT();

    // We will only pass this as as a const reference.
    ConstField fieldpp(field);

    const char* carg = reinterpret_cast<const char*>(arg);

    // No engine available.
    ib_status_t rc = IBPP_TRY_CATCH(NULL, {
        switch (field->type) {
            case IB_FTYPE_NUM:
                IB_FTRACE_RET_PTR(const void,
                    ib_field_dyn_return_num( field,
                        Internal::data_to_value<Field::number_get_t>(cbdata)(
                            fieldpp, carg, arg_length
                        )
                    )
                );
            case IB_FTYPE_UNUM:
                IB_FTRACE_RET_PTR(const void,
                    ib_field_dyn_return_unum( field,
                        Internal::data_to_value<
                            Field::unsigned_number_get_t
                        >(cbdata)(
                            fieldpp, carg, arg_length
                        )
                    )
                );
            case IB_FTYPE_NULSTR:
                IB_FTRACE_RET_PTR(const void,
                    reinterpret_cast<const void*>(
                        Internal::data_to_value<
                            Field::null_string_get_t
                        >(cbdata)(
                            fieldpp, carg, arg_length
                        )
                    )
                );
            case IB_FTYPE_BYTESTR:
                IB_FTRACE_RET_PTR(const void,
                    Internal::data_to_value<Field::byte_string_get_t>(cbdata)(
                        fieldpp, carg, arg_length
                    ).ib()
                );
            default:
                BOOST_THROW_EXCEPTION(
                    einval() << errinfo_what(
                        "Unsupported field type: " + type_as_s(field->type)
                    )
                );
        }
    });

    // If we got here, it is in error.
    assert(rc != IB_OK);
    return NULL;
}

ib_status_t field_dynamic_set(
    ib_field_t* field,
    const void* arg,
    size_t      arg_length,
    const void* value,
    void*       cbdata
)
{
    IB_FTRACE_INIT();

    const char* carg = reinterpret_cast<const char*>(arg);

    // No engine available.
    IB_FTRACE_RET_STATUS(IBPP_TRY_CATCH(NULL, {
        switch (field->type) {
            case IB_FTYPE_NUM:
                Internal::data_to_value<Field::number_set_t>(cbdata)(
                    Field(field),
                    carg, arg_length,
                    reinterpret_cast<int64_t>(value)
                );
            case IB_FTYPE_UNUM:
            Internal::data_to_value<Field::unsigned_number_set_t>(cbdata)(
                Field(field),
                carg, arg_length,
                reinterpret_cast<uint64_t>(value)
            );
            case IB_FTYPE_NULSTR:
                Internal::data_to_value<Field::null_string_set_t>(cbdata)(
                    Field(field),
                    carg, arg_length,
                    reinterpret_cast<const char*>(value)
                );
            case IB_FTYPE_BYTESTR: {
                // Const cast, but then immeidately store as const.
                const ByteString valuepp(
                    const_cast<ib_bytestr_t*>(
                        reinterpret_cast<const ib_bytestr_t*>(value)
                    )
                );
                Internal::data_to_value<Field::byte_string_set_t>(cbdata)(
                    Field(field),
                    carg, arg_length,
                    valuepp
                );
            }
            default:
                BOOST_THROW_EXCEPTION(
                    einval() << errinfo_what(
                        "Unsupported field type: " + type_as_s(field->type)
                    )
                );
        }
    }));
}

} // extern "C"
} // Hooks

template <typename F>
void register_dynamic_get(ib_field_t* field, F f)
{
    if (f.empty()) {
        ib_field_dyn_register_get(field, NULL, NULL);
    } else {
        ib_field_dyn_register_get(
            field,
            Hooks::field_dynamic_get,
            Internal::value_to_data(f, field->mp)
        );
    }
}

template <typename F>
void register_dynamic_set(ib_field_t* field, F f)
{
    if (f.empty()) {
        ib_field_dyn_register_set(field, NULL, NULL);
    } else {
        ib_field_dyn_register_set(
            field,
            Hooks::field_dynamic_set,
            Internal::value_to_data(f, field->mp)
        );
    }
}

} // Anonymous
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

Field ConstField::dup(MemoryPool pool) const
{
    return dup(pool, name(), name_length());
}

Field ConstField::dup() const
{
    return dup(memory_pool());
}

Field ConstField::dup(
    MemoryPool pool,
    const char* new_name,
    size_t new_name_length
) const
{
    ib_field_t* f = NULL;

    ib_status_t rc = ib_field_copy_ex(
        &f,
        pool.ib(),
        new_name, new_name_length,
        ib()
    );
    Internal::throw_if_error(rc);

    return Field(f);
}

Field ConstField::dup(const char* new_name, size_t new_name_length) const
{
    return dup(memory_pool(), new_name, new_name_length);
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

MemoryPool ConstField::memory_pool() const
{
    return MemoryPool(ib()->mp);
}

std::string ConstField::to_s() const
{
    switch (type()) {
        case NUMBER:
            return boost::lexical_cast<std::string>(value_as_number());
        case UNSIGNED_NUMBER:
            return
                boost::lexical_cast<std::string>(value_as_unsigned_number());
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

int64_t ConstField::value_as_number() const
{
    Internal::check_type(NUMBER, type());
    return *ib_field_value_num(ib());
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
    return *ib_field_value_num_ex(ib(), arg, arg_length);
}

uint64_t ConstField::value_as_unsigned_number() const
{
    Internal::check_type(UNSIGNED_NUMBER, type());
    return *ib_field_value_unum(ib());
}

uint64_t ConstField::value_as_unsigned_number(const std::string& arg) const
{
    return value_as_unsigned_number(arg.data(), arg.length());
}

uint64_t ConstField::value_as_unsigned_number(
    const char* arg,
    size_t      arg_length
) const
{
    Internal::check_type(UNSIGNED_NUMBER, type());
    return *ib_field_value_unum_ex(ib(), arg, arg_length);
}

const char* ConstField::value_as_null_string() const
{
    Internal::check_type(NULL_STRING, type());
    return ib_field_value_nulstr(ib());
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
    return ib_field_value_nulstr_ex(ib(), arg, arg_length);
}

ByteString ConstField::value_as_byte_string() const
{
    Internal::check_type(BYTE_STRING, type());
    return ByteString(ib_field_value_bytestr(ib()));
}

ByteString ConstField::value_as_byte_string(const std::string& arg) const
{
    return value_as_byte_string(arg.data(), arg.length());
}

ByteString ConstField::value_as_byte_string(
    const char* arg,
    size_t      arg_length
) const
{
    Internal::check_type(BYTE_STRING, type());
    return ByteString(ib_field_value_bytestr_ex(ib(), arg, arg_length));
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

Field Field::create_number(
    MemoryPool  pool,
    const char* name,
    size_t      name_length,
    int64_t     value
)
{
    return Internal::create_field(
        pool,
        name, name_length,
        Field::NUMBER,
        &value
    );
}


Field Field::create_unsigned_number(
    MemoryPool  pool,
    const char* name,
    size_t      name_length,
    uint64_t    value
)
{
    return Internal::create_field(
        pool,
        name, name_length,
        Field::UNSIGNED_NUMBER,
        &value
    );
}

Field Field::create_null_string(
    MemoryPool  pool,
    const char* name,
    size_t      name_length,
    const char* value
)
{
    return Internal::create_field(
        pool,
        name, name_length,
        Field::NULL_STRING,
        reinterpret_cast<const void*>(&value)
    );
}

Field Field::create_byte_string(
    MemoryPool       pool,
    const char*      name,
    size_t           name_length,
    ConstByteString  value
)
{
    const ib_bytestr_t* ib_bs = value.ib();
    return Internal::create_field(
        pool,
        name, name_length,
        Field::BYTE_STRING,
        &ib_bs
    );
}

Field Field::create_alias_number(
    MemoryPool  pool,
    const char* name,
    size_t      name_length,
    int64_t&    value
)
{
    return Internal::create_alias(
        pool,
        name, name_length,
        Field::NUMBER,
        &value
    );
}

Field Field::create_alias_unsigned_number(
    MemoryPool  pool,
    const char* name,
    size_t      name_length,
    uint64_t&   value
)
{
    return Internal::create_alias(
        pool,
        name, name_length,
        Field::UNSIGNED_NUMBER,
        &value
    );
}

Field Field::create_alias_null_string(
    MemoryPool  pool,
    const char* name,
    size_t      name_length,
    const char* value
)
{
    return Internal::create_alias(
        pool,
        name, name_length,
        Field::NULL_STRING,
        &value
    );
}

Field Field::create_alias_byte_string(
    MemoryPool      pool,
    const char*     name,
    size_t          name_length,
    ConstByteString value
)
{
    return Internal::create_alias(
        pool,
        name, name_length,
        Field::BYTE_STRING,
        &value
    );
}

void Field::set_number(int64_t value) const
{
    Internal::check_type(NUMBER, type());
    Internal::set_value(ib(), &value);
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
    Internal::set_value(ib(), &value, arg, arg_length);
}

void Field::set_unsigned_number(uint64_t value) const
{
    Internal::check_type(UNSIGNED_NUMBER, type());
    Internal::set_value(ib(), &value);
}
void Field::set_unsigned_number(uint64_t value, const std::string& arg) const
{
    return set_unsigned_number(value, arg.data(), arg.length());
}

void Field::set_unsigned_number(
    uint64_t value,
    const char* arg, size_t arg_length
) const
{
    Internal::check_type(UNSIGNED_NUMBER, type());
    Internal::set_value(ib(), &value, arg, arg_length);
}

void Field::set_null_string(const char* value) const
{
    Internal::check_type(NULL_STRING, type());
    Internal::set_value(ib(), reinterpret_cast<const void*>(&value));
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
    Internal::set_value(ib(), value, arg, arg_length);
}

void Field::set_byte_string(ConstByteString value) const
{
    Internal::check_type(BYTE_STRING, type());
    const ib_bytestr_t* ib_bs = value.ib();
    Internal::set_value(ib(), &ib_bs);
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
    Internal::set_value(ib(), value.ib(), arg, arg_length);
}

void Field::set_static_number(int64_t value) const
{
    Internal::check_type(NUMBER, type());
    Internal::set_value_static(ib(), &value);
}

void Field::set_static_unsigned_number(uint64_t value) const
{
    Internal::check_type(UNSIGNED_NUMBER, type());
    Internal::set_value_static(ib(), &value);
}

void Field::set_static_null_string(const char* value) const
{
    Internal::check_type(NULL_STRING, type());
    Internal::set_value_static(ib(), &value);
}

void Field::set_static_byte_string(ConstByteString value) const
{
    Internal::check_type(BYTE_STRING, type());
    Internal::set_value_static(ib(), &value);
}

void Field::register_dynamic_get_number(number_get_t f) const
{
    Internal::register_dynamic_get(ib(), f);
}

void Field::register_dynamic_get_unsigned_number(
    unsigned_number_get_t f
) const
{
    Internal::register_dynamic_get(ib(), f);
}

void Field::register_dynamic_get_null_string(null_string_get_t f) const
{
    Internal::register_dynamic_get(ib(), f);
}

void Field::register_dynamic_get_byte_string(byte_string_get_t f) const
{
    Internal::register_dynamic_get(ib(), f);
}

void Field::register_dynamic_set_number(number_set_t f) const
{
    Internal::register_dynamic_set(ib(), f);
}

void Field::register_dynamic_set_unsigned_number(
    unsigned_number_set_t f
) const
{
    Internal::register_dynamic_set(ib(), f);
}

void Field::register_dynamic_set_null_string(null_string_set_t f) const
{
    Internal::register_dynamic_set(ib(), f);
}

void Field::register_dynamic_set_byte_string(byte_string_set_t f) const
{
    Internal::register_dynamic_set(ib(), f);
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
        o << "IronBee::Field[" << field.name() << " = "
          << field.to_s() << "]";
    }

    return o;
}

} // IronBee
