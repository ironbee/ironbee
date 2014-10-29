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

#include <ironbee/decode.h>

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
#include <vector>
#include <boost/foreach.hpp>

using namespace std;
using namespace IronBee;

namespace {

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
     * The length of characters consumed is returned. Zero is returned
     * if no decoding was possible.
     *
     * @param[in] in The input string.
     * @param[in] in_len The length of the input string.
     * @param[in] out The output buffer to write to.
     * @param[in] out_len The legnth of bytes written to @a out.
     *
     * @returns The total bytes consumed of the input.
     * @throws IronBee exceptions on errors.
     */
    virtual size_t attempt_decode(
        const char* in,
        size_t      in_len,
        char*       out,
        size_t*     out_len
    ) const = 0;

    virtual ~AbstractDecoder(){};
};

/**
 * Skips the prefix and decodes the next two characters.
 */
class HexDecoder : public AbstractDecoder {
private:
    const string m_prefix;

    bool can_decode(const char* str, size_t str_len) const;
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
     * @param[in] in_len The length of the input string.
     * @param[in] out The output buffer to write to.
     * @param[in] out_len The legnth of bytes written to @a out.
     *
     * @returns The total bytes consumed of the input.
     * @throws IronBee exceptions on errors.
     */
    size_t attempt_decode(
        const char* in,
        size_t      in_len,
        char*       out,
        size_t*     out_len
    ) const;

};

HexDecoder::HexDecoder(string prefix) : m_prefix(prefix)
{
    /* nop */
}

size_t HexDecoder::attempt_decode(
    const char* in,
    size_t      in_len,
    char*       out,
    size_t*     out_len
) const
{
    if (can_decode(in, in_len)) {
        int sz = modp_b16_decode(out, in+m_prefix.size(), 2);

        if (sz > 0) {
            *out_len = static_cast<size_t>(sz);

            /* We always consume everything. */
            return m_prefix.size() + 2;
        }
    }

    /* On failure, return 0. Consume nothing. */
    return 0;
}

bool HexDecoder::can_decode(const char *in, size_t in_sz) const
{
    if (m_prefix.size() + 2 > in_sz) {
        return false;
    }

    return 0 == memcmp(m_prefix.data(), in, m_prefix.size());
}

class HtmlEntityDecoder : public AbstractDecoder {
public:
    HtmlEntityDecoder();
    size_t attempt_decode(
        const char* in,
        size_t      in_len,
        char*       out,
        size_t*     out_len
    ) const;
};


HtmlEntityDecoder::HtmlEntityDecoder()
{
}

size_t HtmlEntityDecoder::attempt_decode(
    const char* in,
    size_t      in_len,
    char*       out,
    size_t*     out_len
) const
{
    const char* in_end;
    ib_status_t rc;

    /* If the string does not start with '&', consume nothing. */
    if (in != memchr(in, '&', in_len)) {
        return 0;
    }

    /* If the string does not end in ';', consume nothing. */
    in_end = reinterpret_cast<const char *>(memchr(in, ';', in_len));
    if (in_end == NULL) {
        return 0;
    }

    /* Shrink the in_len value. */
    in_len = in_end - in + 1;

    rc = ib_util_decode_html_entity(
        reinterpret_cast<const uint8_t *>(in),
        in_len,
        reinterpret_cast<uint8_t *>(out),
        out_len
    );

    if (rc != IB_OK) {
        return 0;
    }

    return in_len;
}


/**
 * The actual transformation implememtation class.
 */
class SmartStringEncoderTransformation {
private:

    //! A copy of the user's submitted argument.
    std::string m_arg;

    std::vector< boost::shared_ptr< AbstractDecoder > > m_decoders;
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

    /**
     * Add a decodeer to this transformation.
     *
     * @param[in] decoder The decoder to add.
     *
     * @returns `*this` to allow chanining of calls to
     * SmartStringEncoderTransformation::add().
     */
    SmartStringEncoderTransformation& add(
        boost::shared_ptr<AbstractDecoder> decoder
    );
};

SmartStringEncoderTransformation& SmartStringEncoderTransformation::add(
    boost::shared_ptr<AbstractDecoder> decoder
)
{
    m_decoders.push_back(decoder);
    return *this;
}

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
    m_arg(arg)
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
        size_t bytes_written  = 0;
        size_t bytes_consumed = 0;

        BOOST_FOREACH(boost::shared_ptr<AbstractDecoder> decoder, m_decoders)
        {
            bytes_consumed = decoder->attempt_decode(
                instr + i,
                instr_sz - i,
                outstr + outstr_sz,
                &bytes_written
            );

            if (bytes_consumed > 0) {
                i += bytes_consumed;
                outstr_sz += bytes_written;
                break;
            }
        }

        if (bytes_consumed == 0) {
            /* If nothing handles the input, consume a single byte. */
            outstr[outstr_sz] = instr[i];
            ++i;
            ++outstr_sz;
        }
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

SmartStringEncoderTransformation smart_url_hex_decode(
    MemoryManager mm,
    const char*   arg
)
{
    SmartStringEncoderTransformation decoder(mm, arg);

    decoder.add(boost::shared_ptr<AbstractDecoder>(new HexDecoder("%25")));
    decoder.add(boost::shared_ptr<AbstractDecoder>(new HexDecoder("%u00")));
    decoder.add(boost::shared_ptr<AbstractDecoder>(new HexDecoder("%")));

    return decoder;
}

SmartStringEncoderTransformation smart_hex_decode(
    MemoryManager mm,
    const char*   arg
)
{
    SmartStringEncoderTransformation decoder(mm, arg);

    decoder.add(boost::shared_ptr<AbstractDecoder>(new HexDecoder("0x")));
    decoder.add(boost::shared_ptr<AbstractDecoder>(new HexDecoder("\\x")));
    decoder.add(boost::shared_ptr<AbstractDecoder>(new HexDecoder("U+00")));

    return decoder;
}

SmartStringEncoderTransformation smart_html_decode(
    MemoryManager mm,
    const char*   arg
)
{
    SmartStringEncoderTransformation decoder(mm, arg);

    decoder.add(boost::shared_ptr<AbstractDecoder>(new HtmlEntityDecoder()));

    return decoder;
}

SmartStringEncoder::SmartStringEncoder(Module module)
:
    ModuleDelegate(module)
{
    MemoryManager mm = module.engine().main_memory_mm();

    Transformation::create(
        mm,
        "smart_url_hex_decode",
        false,
        boost::bind(smart_url_hex_decode, mm, _2)
    ).register_with(module.engine());
    Transformation::create(
        mm,
        "smart_hex_decode",
        false,
        boost::bind(smart_hex_decode, mm, _2)
    ).register_with(module.engine());
    Transformation::create(
        mm,
        "smart_html_decode",
        false,
        boost::bind(smart_html_decode, mm, _2)
    ).register_with(module.engine());

}

} // anonymous namespace

IBPP_BOOTSTRAP_MODULE_DELEGATE("smart_stringencoders", SmartStringEncoder);


