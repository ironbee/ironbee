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

#include <ironbee/field.h>
#include <ironbee/debug.h>

#include <boost/lexical_cast.hpp>

#include <cstring>
#include <cassert>

namespace IronBee {

namespace Internal {

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

void set_value(ib_field_t* f, void* in_value)
{
    ib_status_t rc = ib_field_setv(f, in_value);
    Internal::throw_if_error(rc);
}

void set_value(
    ib_field_t* f,
    void* in_value,
    const char* arg,
    size_t arg_length
)
{
    ib_status_t rc = ib_field_setv_ex(f, in_value, arg, arg_length);
    Internal::throw_if_error(rc);
}

void set_value_no_copy(ib_field_t* f, void* mutable_in_value)
{
    ib_status_t rc = ib_field_setv_no_copy(f, mutable_in_value);
    Internal::throw_if_error(rc);
}

namespace Hooks {
extern "C" {

ib_status_t field_dynamic_get(
    const ib_field_t* field,
    void*             out_val,
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
            case IB_FTYPE_NUM: {
                ib_num_t* n = reinterpret_cast<ib_num_t*>(out_val);
                *n = Internal::data_to_value<Field::number_get_t>(cbdata)(
                    fieldpp, carg, arg_length
                );
                IB_FTRACE_RET_STATUS(IB_OK);
            }
            case IB_FTYPE_UNUM: {
                ib_unum_t* u = reinterpret_cast<ib_unum_t*>(out_val);
                *u = Internal::data_to_value<
                    Field::unsigned_number_get_t
                >(cbdata)(
                    fieldpp, carg, arg_length
                );
                IB_FTRACE_RET_STATUS(IB_OK);
            }
            case IB_FTYPE_NULSTR:
            {
                const char** ns = reinterpret_cast<const char**>(out_val);
                *ns = Internal::data_to_value<
                    Field::null_string_get_t
                >(cbdata)(
                    fieldpp, carg, arg_length
                );
                IB_FTRACE_RET_STATUS(IB_OK);
            }
            case IB_FTYPE_BYTESTR:
            {
                const ib_bytestr_t** bs
                    = reinterpret_cast<const ib_bytestr_t**>(out_val);
                *bs = Internal::data_to_value<
                    Field::byte_string_get_t
                >(cbdata)(
                    fieldpp, carg, arg_length
                ).ib();
                IB_FTRACE_RET_STATUS(IB_OK);
            }
            case IB_FTYPE_LIST:
            {
                const ib_list_t** l
                    = reinterpret_cast<const ib_list_t**>(out_val);
                *l = Internal::data_to_value<
                    Internal::dynamic_list_getter_translator_t
                >(cbdata)(
                    fieldpp, carg, arg_length
                );
                IB_FTRACE_RET_STATUS(IB_OK);
            }
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
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t field_dynamic_set(
    ib_field_t* field,
    const void* arg,
    size_t      arg_length,
    void*       in_value,
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
                    *reinterpret_cast<const int64_t*>(in_value)
                );
                break;
            case IB_FTYPE_UNUM:
                Internal::data_to_value<Field::unsigned_number_set_t>(cbdata)(
                    Field(field),
                    carg, arg_length,
                    *reinterpret_cast<const uint64_t*>(in_value)
                );
                break;
            case IB_FTYPE_NULSTR:
                Internal::data_to_value<Field::null_string_set_t>(cbdata)(
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
                Internal::data_to_value<Field::byte_string_set_t>(cbdata)(
                    Field(field),
                    carg, arg_length,
                    value
                );
                break;
            }
            case IB_FTYPE_LIST: {
                const ib_list_t* value =
                    reinterpret_cast<const ib_list_t*>(in_value);
                Internal::data_to_value<
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
    }));
}

} // extern "C"
} // Hooks

Field create_field(
    MemoryPool    pool,
    const char*   name,
    size_t        name_length,
    Field::type_e type,
    void*         in_value
)
{
    ib_field_t* f = NULL;

    ib_status_t rc = ib_field_create(
        &f,
        pool.ib(),
        name, name_length,
        static_cast<ib_ftype_t>(type),
        in_value
    );
    Internal::throw_if_error(rc);

    return Field(f);
}

Field create_no_copy(
    MemoryPool    pool,
    const char*   name,
    size_t        name_length,
    Field::type_e type,
    void*         mutable_in_value
)
{
    ib_field_t* f = NULL;

    ib_status_t rc = ib_field_create_no_copy(
        &f,
        pool.ib(),
        name, name_length,
        static_cast<ib_ftype_t>(type),
        mutable_in_value
    );
    Internal::throw_if_error(rc);

    return Field(f);
}

Field create_alias(
    MemoryPool    pool,
    const char*   name,
    size_t        name_length,
    Field::type_e type,
    void*         mutable_out_value
)
{
    ib_field_t* f = NULL;

    ib_status_t rc = ib_field_create_alias(
        &f,
        pool.ib(),
        name, name_length,
        static_cast<ib_ftype_t>(type),
        mutable_out_value
    );
    Internal::throw_if_error(rc);

    return Field(f);
}

Field create_dynamic_field(
    MemoryPool    pool,
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
        pool.ib(),
        name, name_length,
        static_cast<ib_ftype_t>(type),
        Hooks::field_dynamic_get,
        cbdata_get,
        Hooks::field_dynamic_set,
        cbdata_set
    );
    Internal::throw_if_error(rc);

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

    ib_status_t rc = ib_field_copy(
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
    int64_t v;
    Internal::throw_if_error(ib_field_value(ib(), ib_ftype_num_out(&v)));
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
    Internal::throw_if_error(ib_field_value_ex(
        ib(), ib_ftype_num_out(&v),
        arg, arg_length
    ));
    return v;
}

uint64_t ConstField::value_as_unsigned_number() const
{
    Internal::check_type(UNSIGNED_NUMBER, type());
    uint64_t v;
    Internal::throw_if_error(ib_field_value(ib(), ib_ftype_unum_out(&v)));
    return v;
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
    uint64_t v;
    Internal::throw_if_error(ib_field_value_ex(
        ib(), ib_ftype_unum_out(&v),
        arg, arg_length
    ));
    return v;
}

const char* ConstField::value_as_null_string() const
{
    Internal::check_type(NULL_STRING, type());
    const char* v;
    Internal::throw_if_error(ib_field_value(
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
    Internal::throw_if_error(ib_field_value_ex(
        ib(), ib_ftype_nulstr_out(&v),
        arg, arg_length
    ));
    return v;
}

ConstByteString ConstField::value_as_byte_string() const
{
    Internal::check_type(BYTE_STRING, type());
    const ib_bytestr_t* v;
    Internal::throw_if_error(ib_field_value(
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
    Internal::throw_if_error(ib_field_value_ex(
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
        ib_ftype_num_in(&value)
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
        ib_ftype_unum_in(&value)
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
        ib_ftype_nulstr_in(value)
    );
}

Field Field::create_byte_string(
    MemoryPool       pool,
    const char*      name,
    size_t           name_length,
    ConstByteString  value
)
{
    return Internal::create_field(
        pool,
        name, name_length,
        Field::BYTE_STRING,
        ib_ftype_bytestr_in(value.ib())
    );
}

Field Field::create_no_copy_null_string(
    MemoryPool  pool,
    const char* name,
    size_t      name_length,
    char*       value
)
{
    return Internal::create_no_copy(
        pool,
        name, name_length,
        Field::NULL_STRING,
        ib_ftype_nulstr_mutable_in(value)
    );
}

Field Field::create_no_copy_byte_string(
    MemoryPool      pool,
    const char*     name,
    size_t          name_length,
    ByteString      value
)
{
    return Internal::create_no_copy(
        pool,
        name, name_length,
        Field::BYTE_STRING,
        ib_ftype_bytestr_mutable_in(value.ib())
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
        ib_ftype_num_storage(&value)
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
        ib_ftype_unum_storage(&value)
    );
}


Field Field::create_alias_null_string(
     MemoryPool  pool,
     const char* name,
     size_t      name_length,
     char*&      value
)
{
    return Internal::create_alias(
        pool,
        name, name_length,
        Field::NULL_STRING,
        ib_ftype_nulstr_storage(&value)
    );
}

Field Field::create_alias_byte_string(
    MemoryPool     pool,
    const char*    name,
    size_t         name_length,
    ib_bytestr_t*& value
)
{
    // ByteString is a friend.
    return Internal::create_alias(
        pool,
        name, name_length,
        Field::BYTE_STRING,
        ib_ftype_bytestr_storage(&value)
    );
}

Field Field::create_alias_list(
    MemoryPool     pool,
    const char*    name,
    size_t         name_length,
    ib_list_t*&    value
)
{
    // ByteString is a friend.
    return Internal::create_alias(
        pool,
        name, name_length,
        Field::LIST,
        ib_ftype_list_storage(&value)
    );
}

Field Field::create_dynamic_number(
    MemoryPool   pool,
    const char*  name,
    size_t       name_length,
    number_get_t get,
    number_set_t set
)
{
    return Internal::create_dynamic_field(
        pool,
        name, name_length,
        Field::NUMBER,
        Internal::value_to_data(get, pool.ib()),
        Internal::value_to_data(set, pool.ib())
    );
}

Field Field::create_dynamic_unsigned_number(
    MemoryPool            pool,
    const char*           name,
    size_t                name_length,
    unsigned_number_get_t get,
    unsigned_number_set_t set
)
{
    return Internal::create_dynamic_field(
        pool,
        name, name_length,
        Field::UNSIGNED_NUMBER,
        Internal::value_to_data(get, pool.ib()),
        Internal::value_to_data(set, pool.ib())
    );
}

Field Field::create_dynamic_null_string(
    MemoryPool        pool,
    const char*       name,
    size_t            name_length,
    null_string_get_t get,
    null_string_set_t set
)
{
    return Internal::create_dynamic_field(
        pool,
        name, name_length,
        Field::NULL_STRING,
        Internal::value_to_data(get, pool.ib()),
        Internal::value_to_data(set, pool.ib())
    );
}

Field Field::create_dynamic_byte_string(
    MemoryPool        pool,
    const char*       name,
    size_t            name_length,
    byte_string_get_t get,
    byte_string_set_t set
)
{
    return Internal::create_dynamic_field(
        pool,
        name, name_length,
        Field::BYTE_STRING,
        Internal::value_to_data(get, pool.ib()),
        Internal::value_to_data(set, pool.ib())
    );
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

void Field::set_unsigned_number(uint64_t value) const
{
    Internal::check_type(UNSIGNED_NUMBER, type());
    Internal::set_value(ib(), ib_ftype_unum_in(&value));
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
    Internal::set_value(ib(), ib_ftype_unum_in(&value), arg, arg_length);
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

int64_t& Field::mutable_value_as_number() const
{
    Internal::check_type(NUMBER, type());
    ib_num_t* n;
    Internal::throw_if_error(ib_field_mutable_value(ib(),
        ib_ftype_num_mutable_out(&n)
    ));
    return *n;
}

uint64_t& Field::mutable_value_as_unsigned_number() const
{
    Internal::check_type(UNSIGNED_NUMBER, type());
    ib_unum_t* n;
    Internal::throw_if_error(ib_field_mutable_value(ib(),
        ib_ftype_unum_mutable_out(&n)
    ));
    return *n;
}

char* Field::mutable_value_as_null_string() const
{
    Internal::check_type(NULL_STRING, type());
    char* cs;
    Internal::throw_if_error(ib_field_mutable_value(ib(),
        ib_ftype_nulstr_mutable_out(&cs)
    ));
    return cs;
}

ByteString Field::mutable_value_as_byte_string() const
{
    Internal::check_type(BYTE_STRING, type());
    ib_bytestr_t* bs;
    Internal::throw_if_error(ib_field_mutable_value(ib(),
        ib_ftype_bytestr_mutable_out(&bs)
    ));
    return ByteString(bs);
}

void Field::make_static() const
{
    Internal::throw_if_error(ib_field_make_static(ib()));
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
