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
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include <ironbeepp/exception.hpp>
#include <ironbeepp/memory_manager.hpp>
#include <ironbeepp/memory_pool_lite.hpp>
#include <ironbeepp/module.hpp>
#include <ironbeepp/module_bootstrap.hpp>
#include <ironbeepp/module_delegate.hpp>
#include <ironbeepp/throw.hpp>
#include <ironbeepp/transformation.hpp>

#include <modp_b16.h>
#include <modp_b64.h>
#include <modp_b64w.h>

#include <string>

using namespace std;
using namespace IronBee;

namespace {
const char* c_smrt_strenc_name = "smart_decode";

//! The actuall transformation class.
class SmartStringEncoderTransformation;

//! The delegate of this module for IronBee.
class SmartStringEncoder;

/**
 * A decoder class that defines the interface for decoding text.
 */
class AbstractDecoder {
public:
    /**
     * Decode the input string, writing the results to the output string.
     *
     * The length of characters consumed is returned.
     *
     * @param[in] in The input string.
     * @param[in] in_sz The length of the string that should be decoded.
     * @param[in] out The output buffer to write to.
     * @param[in] out_sz The legnth of bytes written to @a out.
     *
     * @returns The total bytes consumed of the input.
     * @throws IronBee exceptions on errors.
     */
    virtual size_t decode(
        const char* in,
        size_t      in_sz,
        char*       out,
        size_t*     out_sz
    ) const = 0;

    virtual ~AbstractDecoder(){};
};

/**
 * Skips 1 character and decodes the next two characters.
 */
class HexDecoder : public AbstractDecoder {
private:
    const string m_prefix;
public:

    /**
     * Constructor.
     *
     * @param[in] prefix The prefix that preceeds a two-character hex encoded
     *            byte.
     */
    explicit HexDecoder(string prefix);

    /**
     * Decode the input string, writing the results to the output string.
     *
     * The length of characters consumed is returned.
     *
     * @param[in] in The input string.
     * @param[in] in_sz The length of the string that should be decoded.
     * @param[in] out The output buffer to write to.
     * @param[in] out_sz The legnth of bytes written to @a out.
     *
     * @returns The total bytes consumed of the input.
     * @throws IronBee exceptions on errors.
     */
    size_t decode(
        const char* in,
        size_t      in_sz,
        char*       out,
        size_t*     out_sz
    ) const;
};

HexDecoder::HexDecoder(string prefix) : m_prefix(prefix)
{
    /* nop */
}

size_t HexDecoder::decode(
    const char* in,
    size_t      in_sz,
    char*       out,
    size_t*     out_sz
) const
{
    int sz = modp_b16_decode(out, in+m_prefix.size(), in_sz - m_prefix.size());

    /* Failed to decode. Consume 1 byte and advance. */
    if (sz < 0) {
        out[0] = in[0];
        *out_sz = 1;
        return 1;
    }

    *out_sz = static_cast<size_t>(sz);

    /* We always consume everything. */
    return in_sz;
}

/**
 * The actual transformation implememtation class.
 */
class SmartStringEncoderTransformation {
private:

    //! A copy of the user's submitted argument.
    std::string m_arg;

    //! Decode %HH where HH is a hex pair.
    HexDecoder m_pctprefix;

    //! Decode \xHH where HH is a hex pair.
    HexDecoder m_xprefix;
public:

    /**
     * Constructor.
     *
     * @param[in] mm Memory manager for all allocations.
     * @param[in] arg The user's arg provided in the IronBee configuration file.
     */
    SmartStringEncoderTransformation(MemoryManager mm, const char* arg);

    /**
     * The transformation implementation.
     *
     * @param[in] mm Memory manager.
     * @param[in] infield The input field.
     *
     * @returns The transformed field.
     * @throws IronBee exceptions on errors.
     */
    ConstField operator()(MemoryManager mm, ConstField infield) const;

};

/**
 * The Smart String Encoder module delegate.
 */
class SmartStringEncoder : public ModuleDelegate {
public:

    /**
     * Constructor.
     *
     * @param[in] module The module.
     */
    explicit SmartStringEncoder(Module module);
};

SmartStringEncoderTransformation::SmartStringEncoderTransformation(
    MemoryManager mm,
    const char*   arg
) :
    m_arg(arg),
    m_pctprefix("%"),
    m_xprefix("\\x")
{
}

ConstField SmartStringEncoderTransformation::operator()(
    MemoryManager mm,
    ConstField infield
) const
{
    const char           *instr;
    size_t                instr_sz;
    char                 *outstr;
    size_t                outstr_sz;
    ConstByteString       bs;

    /* Set instr and instr_sz for decoding. */
    switch (infield.type()){
    case ConstField::BYTE_STRING:
        bs       = infield.value_as_byte_string();
        instr    = bs.const_data();
        instr_sz = bs.size();
        break;
    case ConstField::NULL_STRING:
        instr    = infield.value_as_null_string();
        instr_sz = strlen(instr);
        break;
    default:
        BOOST_THROW_EXCEPTION(
            einval()
                << errinfo_what("Invalid input field type.")
        );
    }

    /* Decoded strings are always shorter than encoded strings.
     * Make a string of at-least enough space to hold a un decodable string. */
    outstr = mm.allocate<char>(instr_sz);
    outstr_sz = 0;

    /* Walk through instr and try to decode it into outstr. */
    for (size_t i = 0; i < instr_sz; ) {
        size_t bytes_written;
        size_t bytes_consumed;

        /* Decode 1 char prefix, 2 char hex encoding. */
        if (i + 2 < instr_sz) {
            if (instr[i] == '%') {
                bytes_consumed = m_pctprefix.decode(instr+i, 3, outstr+outstr_sz, &bytes_written);
                i += bytes_consumed;
                outstr_sz += bytes_written;
                continue;
            }
        }

        /* Decode 2 char prefix, 2 char hex encoding. */
        if (i + 3 < instr_sz) {
            if (instr[i] == '\\' && instr[i+1] == 'x') {
                bytes_consumed = m_xprefix.decode(instr+i, 4, outstr+outstr_sz, &bytes_written);
                i += bytes_consumed;
                outstr_sz += bytes_written;
                continue;
            }
        }

        /* If nothing handles the input, consume a single byte. */
        outstr[outstr_sz] = instr[i];
        ++i;
        ++outstr_sz;
    }

    /* On success, build an return a field. */
    return Field::create_no_copy_byte_string(
        mm,
        infield.name(),
        infield.name_length(),
        ByteString::create(
            mm,
            outstr,
            outstr_sz
            )
        );
}

/**
 * Transformation generator function.
 */
Transformation::transformation_instance_t smrt_strenc_gen(
    MemoryManager mm,
    const char*   arg
)
{
    return SmartStringEncoderTransformation(mm, arg);
}

SmartStringEncoder::SmartStringEncoder(Module module)
:
    ModuleDelegate(module)
{
    MemoryManager mm = module.engine().main_memory_mm();

    Transformation::create(
        mm,
        c_smrt_strenc_name,
        false,
        boost::bind(smrt_strenc_gen, mm, _2)
    ).register_with(module.engine());
}

} // anonymous namespace

IBPP_BOOTSTRAP_MODULE_DELEGATE("smart_stringencoders", SmartStringEncoder);
