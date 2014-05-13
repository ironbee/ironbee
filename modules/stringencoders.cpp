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

#include <modp_b64.h>
#include <modp_b64w.h>

using namespace std;
using namespace IronBee;

namespace {

//! Base64 transformation name.
const char* c_b64_decode = "b64_decode";
//! Base64 web safe transformation name.
const char* c_b64w_decode = "b64w_decode";

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

//! Decode base64.
ConstField b64_decode(
    MemoryManager mm,
    ConstField    input
)
{
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


void module_load(IronBee::Module module)
{
    MemoryManager mm = module.engine().main_memory_mm();

    Transformation::create(
        mm,
        c_b64_decode,
        false,
        bind(generate, b64_decode)
    ).register_with(module.engine());

    Transformation::create(
        mm,
        c_b64w_decode,
        false,
        bind(generate, b64w_decode)
    ).register_with(module.engine());
}

} // Anonymous
