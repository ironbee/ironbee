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
 * @brief IronBee --- String encoders Module
 *
 * This module exposes the string encoders library via transformations.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbeepp/all.hpp>

#include <modp_b16.h>
#include <modp_b64.h>
#include <modp_b64w.h>

using namespace std;
using namespace IronBee;

namespace {

//! Base64 transformation name.
const char* c_b64_decode = "b64_decode";
//! Base64 web safe transformation name.
const char* c_b64w_decode = "b64w_decode";
//! Hex transformation name.
const char* c_b16_decode = "b16_decode";

//! Called on module load.
void module_load(IronBee::Module module);

} // Anonymous

IBPP_BOOTSTRAP_MODULE("stringencoders", module_load)

// Implementation

// Reopen for doxygen; not needed by C++.
namespace {

//! Trivial generator.
Transformation::transformation_instance_t generate(
    Transformation::transformation_instance_t which
)
{
    return which;
}

ConstField fwd_list_elements_to(
    ConstField(*fn)(MemoryManager, ConstField),
    MemoryManager mm,
    ConstField    input
)
{
    List<const ib_field_t *> decoded_input =
        List<const ib_field_t *>::create(mm);

    BOOST_FOREACH(
        ib_field_t *f,
        input.value_as_list<ib_field_t*>()
    )
    {
        decoded_input.push_back(fn(mm, ConstField(f)).ib());
    }

    return Field::create_no_copy_list(
        mm,
        input.name(), input.name_length(),
        decoded_input
    );
}

//! Decode base64.
ConstField b64_decode(
    MemoryManager mm,
    ConstField    input
)
{
    /* Handle and return list types. */
    if (input.type() == Field::LIST) {
        return fwd_list_elements_to(b64_decode, mm, input);
    }

    if (input.type() != Field::BYTE_STRING) {
        BOOST_THROW_EXCEPTION(
            einval() << errinfo_what(
                "Unsupported field type."
            )
        );
    }

    ConstByteString bs = input.value_as_byte_string();
    size_t output_length = modp_b64_decode_len(bs.size());
    char* output = mm.allocate<char>(output_length);

    int actual_output_length =
        modp_b64_decode(output, bs.const_data(), bs.size());

    return Field::create_no_copy_byte_string(
        mm,
        input.name(), input.name_length(),
        ByteString::create(mm, output, actual_output_length)
    );
}

//! Decode base64 web-safe.
ConstField b64w_decode(
    MemoryManager mm,
    ConstField    input
)
{
    /* Handle and return list types. */
    if (input.type() == Field::LIST) {
        return fwd_list_elements_to(b64w_decode, mm, input);
    }

    if (input.type() != Field::BYTE_STRING) {
        BOOST_THROW_EXCEPTION(
            einval() << errinfo_what(
                "Unsupported field type."
            )
        );
    }

    ConstByteString bs = input.value_as_byte_string();
    size_t output_length = modp_b64w_decode_len(bs.size());
    char* output = mm.allocate<char>(output_length);

    int actual_output_length =
        modp_b64w_decode(output, bs.const_data(), bs.size());

    return Field::create_no_copy_byte_string(
        mm,
        input.name(), input.name_length(),
        ByteString::create(mm, output, actual_output_length)
    );
}

//! Decode base16.
ConstField b16_decode(
    const char*   prefix,
    MemoryManager mm,
    ConstField    input
)
{
    /* Handle and return list types. */
    if (input.type() == Field::LIST) {
        List<const ib_field_t *> decoded_input =
            List<const ib_field_t *>::create(mm);

        BOOST_FOREACH(
            ib_field_t *f,
            input.value_as_list<ib_field_t*>()
        )
        {
            decoded_input.push_back(b16_decode(prefix, mm, ConstField(f)).ib());
        }

        return Field::create_no_copy_list(
            mm,
            input.name(), input.name_length(),
            decoded_input
        );
    }

    if (input.type() != Field::BYTE_STRING) {
        BOOST_THROW_EXCEPTION(
            einval() << errinfo_what(
                "Unsupported field type."
            )
        );
    }

    ConstByteString bs = input.value_as_byte_string();
    const char* data = NULL;
    size_t data_length = 0;
    const char* bs_data = bs.const_data();
    size_t bs_size = bs.size();

    if (! prefix || ! prefix[0]) {
        // Directly decode.
        data = bs_data;
        data_length = bs_size;
    }
    else {
        // Extract out XX from any prefixXX in string.
        size_t prefix_length = strlen(prefix);
        char* mutable_data = mm.allocate<char>(bs_size);
        data = mutable_data;
        size_t i = 0;
        data_length = 0;
        while (i < bs_size - prefix_length - 1) {
            if (memcmp(bs_data + i, prefix, prefix_length) == 0) {
                // skip prefix
                i += prefix_length;
                // copy two chars
                *(mutable_data + data_length) = *(bs_data + i);
                ++data_length; ++i;
                *(mutable_data + data_length) = *(bs_data + i);
                ++data_length; ++i;
            }
        }
    }

    size_t output_length = modp_b16_decode_len(data_length);
    char* output = mm.allocate<char>(output_length);

    int actual_output_length = modp_b16_decode(output, data, data_length);

    return Field::create_no_copy_byte_string(
        mm,
        input.name(), input.name_length(),
        ByteString::create(mm, output, actual_output_length)
    );
}

//! Generate b16 decoder.
Transformation::transformation_instance_t b16_generate(
    MemoryManager mm,
    const char* param
)
{
    return boost::bind(b16_decode, mm.strdup(param), _1, _2);
}

void module_load(IronBee::Module module)
{
    MemoryManager mm = module.engine().main_memory_mm();

    Transformation::create(
        mm,
        c_b64_decode,
        true,
        bind(generate, b64_decode)
    ).register_with(module.engine());

    Transformation::create(
        mm,
        c_b64w_decode,
        true,
        bind(generate, b64w_decode)
    ).register_with(module.engine());

    Transformation::create(
        mm,
        c_b16_decode,
        true,
        b16_generate
    ).register_with(module.engine());
}

} // Anonymous
