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
 * @brief IronBee Modules --- UTF-8 Processing
 *
 * Provide operations for handling UTF-8.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */
#include "ironbee_config_auto.h"

#include <ironbeepp/module.hpp>
#include <ironbeepp/module_bootstrap.hpp>
#include <ironbeepp/module_delegate.hpp>
#include <ironbeepp/operator.hpp>
#include <ironbeepp/transformation.hpp>

#ifdef __clang__
#pragma clang diagnostic push
#if __has_warning("-Wunused-local-typedef")
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
#endif
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string/erase.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

/* UTF-8 library found in base_srcdir/libs/utf8*. */
#include <utf8.h>

#include <iterator>
#include <map>
#include <sstream>
#include <vector>

namespace {

typedef std::map<std::string, char> utf8ToAscii_t;

const std::string UTF8_REPLACEMENT_CHARACTER("\xef\xbf\xbd");

using namespace IronBee;

class Utf8ModuleDelegate : public ModuleDelegate{
private:

    /**
     * Global mapping of UTF-8 characters to ASCII characters.
     *
     * This is not expected to vary per-context so it does not
     * exist in context configuration object.
     */
    utf8ToAscii_t m_utf8toAscii;

    /**
     * Initialize m_utf8toAscii.
     */
    void initialize_m_utf8toAscii();
public:
    explicit Utf8ModuleDelegate(Module m);
};

/**
 * Replace the UTF-8 character given in the sequence of bytes in @a v.
 *
 * This algorithm is not obvious and needs careful attention.
 *
 * Generally,
 * - The UTF-8 prefix bits (or control bits) are removed from the
 *   the characters leaving only the codepoint bits.
 * - All-zero characters ("empty" bytes) are removed.
 * - The series of bytes is repacked into UTF-8.
 * - Prefixes are added back to the characters.
 * - @a v is resized.
 *
 * More specifically,
 * - @a v is iterated through to strip prefixes. If
 *   If a series of leading bytes result in empty bytes, then
 *   v[1] ... v[x] (where x is the index of the last empty byte) are skipped.
 *   v[0] is not skipped as it contains a long prefix which may not
 *   fit in the following bytes, depending on how many codepoints are
 *   used.
 * - After all the empty bytes from v[1] are removed, v[0] is
 *   checked to see if it could be collapsed into v[x], where v[x] is
 *   the first non-empty byte. This is done by checking if the codepoints
 *   in v[x] are `<=` the largest value v[0] could contain assuming
 *   it used an appropriate number of control bits for the new, shorter
 *   array of bytes in @a v. Note, @a v has not yet be resized.
 *   If a fit is found, then the start of @a v is updated from v[0]
 *   to v[x]. @a v is still not resized yet.
 * - Next, all characters have their prefixes removed if they have not
 *   already and @a v is "collapsed." That is @a v has all its
 *   empty bytes removed and the v[0] is now the first byte containing
 *   data. This means that the trailing bytes in @a v contain invalid
 *   data, but this will be taken care of when we eventually resize @a v.
 * - Next, more collapsing rules are checked. Specifically, in the case
 *   of a two-byte character there are many more opportunities to
 *   collapse the character as instead of 6 codepoints a single
 *   UTF-8 byte character has 7 codepoints.
 * - Finally, v[0] is given a new prefix if the new size is greater than 1.
 * - All bytes for v[x] where x > 0 are given the prefix `0x80`.
 * - `v.resize()` is called.
 *
 * @param[in,out] v The vector is edited and resized to produce
 *                a canonical representation of UTF-8 characters.
 *
 */
template<typename T>
void repack_utf8(std::vector<T> &v)
{
    /* Starting point of a zero-stripped v. */
    size_t new_start = 0;

    /* The size of v. It will be shrunk as a last action. */
    size_t new_size;

    /* No repacking necessary. */
    if (v.size() < 2) {
        return;
    }

    /* Strip off the prefix of character 0. */
    v[0] = v[0] & (0xffU >> v.size());

    /* Remove all leading zeros not in v[0] (if v[0] == 0). */
    if (v[0] == 0) {
        size_t i = 1;
        while (i < v.size() && (v[i] & 0x3f) == 0) {
            ++i;
        }

        /* At this point we know that v[0] is 0 and v[i] is not 0.
         * We also know that all v[x] where x is > 0 and < i are 0.
         * There is a possibility that v[0] could be merged into v[i]
         * if v[i] isn't using too many bits. */
        size_t tmp_sz = v.size() - i + 1;
        if (((v[i] & 0x3f) & ~(0xffU >> tmp_sz)) == 0) {
            /* We have enough bits in v[i]. Make it the first byte. */
            new_start = i;
        }
    }

    new_size = v.size() - new_start;

    /* Continue stripping off the prefixes of all remaining bytes.
     * In cases where new_start was not changed from 0, this may
     * mask bytes already processed. A minor inefficiency but not
     * a problem, algorithmically.
     *
     * This also compacts the list to the front of v.
     */
    for (size_t i = 0; i < new_size; ++i) {
        v[i] = v[new_start + i] & 0x3f;
    }

    /* When j == 2 and v[0] == 1, v[0] can fit into v[1].
     * Replace v[0] and set the size to 1. */
    if (new_size == 2 && v[0] == 1) {
        v[0] = v[1] | 0x40;
        v.resize(1);
    }
    /* Similar to the previous case, but v[0] had no data in it. */
    else if (new_size == 2 && v[0] == 0) {
        v.resize(1);
    }
    /* Otherwise just resize the vector. We're good. */
    else {
        /* Now i = v.size() and j = the new size of v. */
        v.resize(new_size);
    }

    /* Only add the prefixes back on if there are multiple bytes. */
    if (v.size() > 1) {
        /* Now put all the prefixes back. */
        v[0] = v[0] | ~(0xffU >> v.size());
        for (size_t i = 1; i < v.size(); ++i) {
            v[i] = 0x80|v[i];
        }
    }
}

/**
 * Return 1 if @a in is UTF8 and 0 otherwise.
 *
 * Overlong values are not errors.
 *
 * @param[in] tx Transaction.
 * @param[in] in Input field to operate on.
 */
int validateUtf8(Transaction tx, ConstField in)
{
    assert(tx);

    if (! in) {
        return 0;
    }

    if (in.type() != Field::NULL_STRING && in.type() != Field::BYTE_STRING) {
        return 0;
    }

    std::istringstream in_stringstream(in.to_s());

    std::istream_iterator<char> it(in_stringstream);
    std::istream_iterator<char> eos;

    if (utf8::is_valid(it, eos)) {
        return 1;
    }
    else {
        return 0;
    }
}

/**
 * Wrapper around utf8::replace_invalid().
 *
 * @param[in] mm Memory manager.
 * @param[in] f The input field to process.
 *
 * @returns A new converted field.
 * @throws IronBee exceptions on error.
 */
ConstField replaceInvalidUtf8(MemoryManager mm, ConstField f)
{
    if (!f) {
        return f;
    }

    if (f.type() != Field::NULL_STRING && f.type() != Field::BYTE_STRING) {
        return f;
    }

    std::string str = f.to_s();

    std::string new_str;
    utf8::replace_invalid(str.begin(), str.end(), std::back_inserter(new_str));

    return Field::create_byte_string(
        mm,
        f.name(),
        f.name_length(),
        ByteString::create(mm, new_str)
    );
}

ConstField removeUtf8ReplacementCharacter(MemoryManager mm, ConstField f)
{
    if (!f) {
        return f;
    }

    if (f.type() != Field::NULL_STRING && f.type() != Field::BYTE_STRING) {
        return f;
    }

    std::string str = f.to_s();

    std::ostringstream oss;
    std::ostream_iterator<char> osi(oss);
    boost::algorithm::erase_all_copy(osi, str, UTF8_REPLACEMENT_CHARACTER);

    return Field::create_byte_string(
        mm,
        f.name(),
        f.name_length(),
        ByteString::create(mm, oss.str())
    );
}

/**
 * Wrapper around utf8::unchecked::utf8to16().
 *
 * @param[in] mm Memory manager.
 * @param[in] f The input field to process.
 *
 * @returns A new field with no invalid characters in it.
 * @throws IronBee exceptions on error.
 */
ConstField utf8To16(MemoryManager mm, ConstField f)
{
    if (!f) {
        return f;
    }

    if (f.type() != Field::NULL_STRING && f.type() != Field::BYTE_STRING) {
        return f;
    }

    std::string str = f.to_s();

    std::string new_str;

    utf8::unchecked::utf8to16(str.begin(), str.end(), std::back_inserter(new_str));

    return Field::create_byte_string(
        mm,
        f.name(),
        f.name_length(),
        ByteString::create(mm, new_str)
    );
 }

/**
 * Wrapper around utf8::unchecked::utf8To32().
 *
 * @param[in] mm Memory manager.
 * @param[in] f The input field to process.
 *
 * @returns A new converted field.
 * @throws IronBee exceptions on error.
 */
ConstField utf8To32(MemoryManager mm, ConstField f)
{
    if (!f) {
        return f;
    }

    if (f.type() != Field::NULL_STRING && f.type() != Field::BYTE_STRING) {
        return f;
    }

    std::string str = f.to_s();

    std::string new_str;

    utf8::unchecked::utf8to32(str.begin(), str.end(), std::back_inserter(new_str));

    return Field::create_byte_string(
        mm,
        f.name(),
        f.name_length(),
        ByteString::create(mm, new_str)
    );
}

/**
 * Wrapper around utf8::unchecked::utf16to8().
 *
 * @param[in] mm Memory manager.
 * @param[in] f The input field to process.
 *
 * @returns A new converted field.
 * @throws IronBee exceptions on error.
 */
ConstField utf16To8(MemoryManager mm, ConstField f)
{
    if (!f) {
        return f;
    }

    if (f.type() != Field::NULL_STRING && f.type() != Field::BYTE_STRING) {
        return f;
    }

    std::string str = f.to_s();

    std::string new_str;

    utf8::unchecked::utf16to8(str.begin(), str.end(), std::back_inserter(new_str));

    return Field::create_byte_string(
        mm,
        f.name(),
        f.name_length(),
        ByteString::create(mm, new_str)
    );
}

/**
 * Wrapper around utf8::unchecked::utf32to8().
 *
 * @param[in] mm Memory manager.
 * @param[in] f The input field to process.
 *
 * @returns A new converted field.
 * @throws IronBee exceptions on error.
 */
ConstField utf32To8(MemoryManager mm, ConstField f)
{
    if (!f) {
        return f;
    }

    if (f.type() != Field::NULL_STRING && f.type() != Field::BYTE_STRING) {
        return f;
    }

    std::string str = f.to_s();

    std::string new_str;

    utf8::unchecked::utf32to8(str.begin(), str.end(), std::back_inserter(new_str));

    return Field::create_byte_string(
        mm,
        f.name(),
        f.name_length(),
        ByteString::create(mm, new_str)
    );
}

/**
 * Read a valid UTF character for @a itr into @a utfchar.
 *
 * This class exposes its internal buffer to the user. The user
 * may modify this buffer as it will be resized and overwitten
 * when Utf8Reader::read() is called.
 */
class Utf8Reader {
public:
    /**
     * Construct a Utf8Reader that iterates over @a str.
     *
     * @a str must not be changed during this class's use.
     *
     * @param[in] str The sting to iterate over.
     */
    Utf8Reader(std::string& str);

    /**
     * Read a character, returning if it is valid or not.
     *
     * If an invalid character is read, the read bytes
     * up to the errant byte is stored in this class's vector.
     *
     * @return True of the character is valid. False otherwise.
     * In the case of false, the buffer returned by
     * Utf8Reader::utf8char() contains bytes up to the byte that
     * caused the error.
     */
    bool read();

    /**
     * Shift the internal iterator used to read characters by @a n.
     * The next call to Utf8Reader::read() will start at new position.
     *
     * @param[in] n The numbe to shift the std::string::iterator by.
     *
     */
    void shift(int n);

    /**
     * Return a vector that holds the currently read number of bytes.
     * This is a reference to the internal buffer. The user may modify
     * this buffer, but realize it will be changed by Utf8Reader::read().
     *
     * @returns A reference to the internal buffer of this Utf8Reader.
     */
    std::vector<unsigned char>& utf8char();

    /* Is there more to read? */
    bool has_more();

private:
    std::string::iterator      m_itr;
    std::string::iterator      m_end;
    std::vector<unsigned char> m_utfchar;
};

Utf8Reader::Utf8Reader(std::string& str)
:
    m_itr(str.begin()),
    m_end(str.end()),
    m_utfchar(6)
{
    m_utfchar.resize(0);
}

std::vector<unsigned char>& Utf8Reader::utf8char()
{
    return m_utfchar;
}

bool Utf8Reader::has_more(){
    return m_itr != m_end;
}

bool Utf8Reader::read()
{
    /* Retrieve the first byte in a UTF character. */
    unsigned char c = *m_itr;

    /* If single-byte encoded (high-order bit is 0), keep the byte. */
    if (~c & 0x80) {
        m_utfchar.resize(1);
        m_utfchar[0] = c;
        ++m_itr;
        return true;
    }

    /* Is this byte the first byte in a multi-byte UTF-8 encoding? */
    if ((c & 0xc0) == 0xc0) {
        /* The number of bytes used to encode this character.
         * We know we will use at least 2 from the true if-check. */
        int bytes = 2;

        /* Count more bits, representing more bytes. */
        for (char mask = 0x20; (c & mask) && (mask > 0); mask = mask >> 1) {
            ++bytes;
        }

        /* If there are too many bytes, emit the first byte and skip on. */
        if (bytes > 6) {
            m_utfchar.resize(1);
            m_utfchar[0] = c;
            ++m_itr;
            return false;
        }

        m_utfchar.resize(bytes);
        m_utfchar[0] = c;

        /* Advance to the second byte we are considering in the stream. */
        ++m_itr;

        /* We just considered the first byte. Start i = 1. */
        for (int i = 1; i < bytes; ++i, ++m_itr) {

            /* Premature end of character. */
            if (m_itr == m_end) {
                m_utfchar.resize(i);
                return false;
            }

            c = *m_itr;

            if ((c & 0x80) && (~c & 0x40)) {
                m_utfchar[i] = c;
            }
            else {
                m_utfchar[i] = c;
                m_utfchar.resize(i);
                return false;
            }
        }

        return true;
    }

    /* If we end up here, just echo the char. */
    m_utfchar.resize(1);
    m_utfchar[0] = c;
    ++m_itr;
    return false;
}

void Utf8Reader::shift(int n) {
    m_itr = m_itr + n;
}

/**
 * Handle invalid reads from a Utf8Reader in a common way.
 *
 * The Utf8Reader will read characters until an error is detected.
 * The caller of the Utf8Reader::read() method can handle this error
 * in many ways. Specifically, inserting the UTF-8 replacement
 * character U+FFFE (UTF-8 hex: 0xef 0xbf 0xbd) or dropping the character,
 * and repositioning the Utf8Reader's iterator at the appropriate
 * new start point.
 *
 * This function chooses emit the replacement character on an error
 * and to resume parsing at position i+1 where i is the position of the
 * first byte in the sequence that caused the error.
 */
void handleInvalidCharacter(
    std::string& outstr,
    Utf8Reader& utf8Reader,
    const std::string replacement_character = UTF8_REPLACEMENT_CHARACTER
) {
    std::vector<unsigned char>& v = utf8Reader.utf8char();

    if (v.size() > 1) {
        /* If the byte that caused the error is not the  first byte,
         * then push back all but the first byte read. */
        utf8Reader.shift(1 - v.size());
    }

    /* Write replacement character. */
    BOOST_FOREACH(char c, replacement_character) {
        outstr.push_back(c);
    }
}

/**
 * Replace overlong UTF-8 characters with their shortest form.
 *
 * Invalid characters are discarded.
 *
 * @param[in] mm MemoryManager to create the out field from.
 * @param[in] f The input field to consider.
 *
 * @returns A newly created field containing the normalized UTF-8 string.
 *
 * @throws IronBee::einval on invalid UTF-8 sequences.
 */
ConstField normalizeUtf8(MemoryManager mm, ConstField f)
{
    if (!f) {
        return f;
    }

    if (f.type() != Field::NULL_STRING && f.type() != Field::BYTE_STRING) {
        return f;
    }

    std::string str = f.to_s();

    std::string new_str;

    Utf8Reader reader(str);

    while (reader.has_more()) {

        /* Valid char. */
        if (reader.read()) {
            repack_utf8(reader.utf8char());

            /* Always emit data, repacked or not. */
            BOOST_FOREACH(char c, reader.utf8char()) {
                new_str.push_back(c);
            }
        }
        /* If we've read an invalid character. */
        else {
            handleInvalidCharacter(new_str, reader);
        }
    }

    return Field::create_byte_string(
        mm,
        f.name(),
        f.name_length(),
        ByteString::create(mm, new_str)
    );
}

/**
 * Look up any multi character UTF-8 and see if there are ASCII replacements.
 *
 * If there is no replacement the 0 character is injected.
 *
 * @param[in] utf8ToAscii Table of replacements.
 * @param[in] mm Memory manager.
 * @param[in] f The field to convert.
 *
 * @return Newly created field holding the converted string.
 * @throws IronBee exceptions on errors.
 */
ConstField utf8ToAscii(
    utf8ToAscii_t& utf8ToAscii,
    MemoryManager  mm,
    ConstField     f
)
{
    if (!f) {
        return f;
    }

    if (f.type() != Field::NULL_STRING && f.type() != Field::BYTE_STRING) {
        return f;
    }

    std::string str = f.to_s();

    std::string new_str;

    Utf8Reader reader(str);

    while (reader.has_more()) {

        utf8ToAscii_t::const_iterator map_itr = utf8ToAscii.end();

        if (reader.read()) {
            /* Get the start of the string. */
            char * first_char = reinterpret_cast<char *>(
                &(reader.utf8char()[0]));

            /* Make a C++ string out of it. */
            std::string utf8char(
                first_char,
                reader.utf8char().size()
            );

            /* Use this to find a mapping, if any. */
            map_itr = utf8ToAscii.find(utf8char);

            /* If we have a mapping (map_itr != end). */
            if (map_itr != utf8ToAscii.end()) {
                new_str.push_back(map_itr->second);
            }
            /* Handle unmapped characters. */
            else {
                BOOST_FOREACH(char c, utf8char) {
                    new_str.push_back(c);
                }
            }
        }
        else {
            handleInvalidCharacter(new_str, reader, "\x0");
        }

    }

    return Field::create_byte_string(
        mm,
        f.name(),
        f.name_length(),
        ByteString::create(mm, new_str)
    );
}

/**
 * Utility function to wrap creating new operator instances.
 *
 * @param[in] f The function to use as the operator.
 *
 * @return The new operator instance.
 */
Operator::operator_instance_t operator_generator(
    int f(Transaction, ConstField)
)
{
    return boost::bind(f, _1, _2);
}

/**
 * Utility function to wrap creating new transaction instances.
 *
 * @param[in] f The function to use as the transaction.
 *
 * @return The new transaction instance.
 */
Transformation::transformation_instance_t transformation_generator(
    ConstField f(MemoryManager, ConstField)
)
{
    return boost::bind(f, _1, _2);
}

Utf8ModuleDelegate::Utf8ModuleDelegate(Module m) : ModuleDelegate(m)
{
    initialize_m_utf8toAscii();

    MemoryManager mm = m.engine().main_memory_mm();

    Operator::create(
        mm,
        "validateUtf8",
        IB_OP_CAPABILITY_NONE,
        boost::bind(operator_generator, validateUtf8)
    ).register_with(m.engine());

    Transformation::create(
        mm,
        "replaceInvalidUtf8",
        false,
        boost::bind(transformation_generator, replaceInvalidUtf8)
    ).register_with(m.engine());

    Transformation::create(
        mm,
        "utf8To16",
        false,
        boost::bind(transformation_generator, utf8To16)
    ).register_with(m.engine());

    Transformation::create(
        mm,
        "utf8To32",
        false,
        boost::bind(transformation_generator, utf8To32)
    ).register_with(m.engine());

    Transformation::create(
        mm,
        "utf16To8",
        false,
        boost::bind(transformation_generator, utf16To8)
    ).register_with(m.engine());

    Transformation::create(
        mm,
        "utf32To8",
        false,
        boost::bind(transformation_generator, utf32To8)
    ).register_with(m.engine());

    Transformation::create(
        mm,
        "normalizeUtf8",
        false,
        boost::bind(transformation_generator, normalizeUtf8)
    ).register_with(m.engine());

    Transformation::create(
        mm,
        "removeUtf8ReplacementCharacter",
        false,
        boost::bind(transformation_generator, removeUtf8ReplacementCharacter)
    ).register_with(m.engine());

    Transformation::create<void>(
        mm,
        "utf8ToAscii",
        false,
        boost::function<void *(MemoryManager, const char *)>(),
        boost::function< void(void *)>(),
        boost::bind(utf8ToAscii, boost::ref(m_utf8toAscii), _1, _2)
    ).register_with(m.engine());

}

void Utf8ModuleDelegate::initialize_m_utf8toAscii()
{
    // U+00A1  ¡   c2 a1   INVERTED EXCLAMATION MARK
    m_utf8toAscii["\xc2\xa1"] = '!';
    // U+00A2  ¢   c2 a2   CENT SIGN
    m_utf8toAscii["\xc2\xa2"] = 'c';
    // U+00A3  £   c2 a3   POUND SIGN
    m_utf8toAscii["\xc2\xa3"] = 'l';
    // U+00A4  ¤   c2 a4   CURRENCY SIGN
    m_utf8toAscii["\xc2\xa4"] = 'x';
    // U+00A5  ¥   c2 a5   YEN SIGN
    m_utf8toAscii["\xc2\xa5"] = 'Y';
    // U+00A6  ¦   c2 a6   BROKEN BAR
    m_utf8toAscii["\xc2\xa6"] = '|';
    // U+00A7  §   c2 a7   SECTION SIGN
    m_utf8toAscii["\xc2\xa7"] = 'S';
    // U+00A8  ¨   c2 a8   DIAERESIS
    m_utf8toAscii["\xc2\xa8"] = ' ';
    // U+00A9  ©   c2 a9   COPYRIGHT SIGN
    m_utf8toAscii["\xc2\xa9"] = 'c';
    // U+00AA  ª   c2 aa   FEMININE ORDINAL INDICATOR
    m_utf8toAscii["\xc2\xaa"] = 'a';
    // U+00AB  «   c2 ab   LEFT-POINTING DOUBLE ANGLE QUOTATION MARK
    m_utf8toAscii["\xc2\xab"] = '<';
    // U+00AC  ¬   c2 ac   NOT SIGN
    m_utf8toAscii["\xc2\xac"] = '-';
    // U+00AD  ­   c2 ad   SOFT HYPHEN
    m_utf8toAscii["\xc2\xad"] = '-';
    // U+00AE  ®   c2 ae   REGISTERED SIGN
    m_utf8toAscii["\xc2\xae"] = 'r';
    // U+00AF  ¯   c2 af   MACRON
    m_utf8toAscii["\xc2\xaf"] = '-';
    // U+00B0  °   c2 b0   DEGREE SIGN
    m_utf8toAscii["\xc2\xb0"] = 'o';
    // U+00B2  ²   c2 b2   SUPERSCRIPT TWO
    m_utf8toAscii["\xc2\xb2"] = '2';
    // U+00B3  ³   c2 b3   SUPERSCRIPT THREE
    m_utf8toAscii["\xc2\xb3"] = '3';
    // U+00B4  ´   c2 b4   ACUTE ACCENT
    m_utf8toAscii["\xc2\xb4"] = '\'';
    // U+00B5  µ   c2 b5   MICRO SIGN
    m_utf8toAscii["\xc2\xb5"] = 'u';
    // U+00B6  ¶   c2 b6   PILCROW SIGN
    m_utf8toAscii["\xc2\xb6"] = 'P';
    // U+00B7  ·   c2 b7   MIDDLE DOT
    m_utf8toAscii["\xc2\xb7"] = '.';
    // U+00B8  ¸   c2 b8   CEDILLA
    m_utf8toAscii["\xc2\xb8"] = '.';
    // U+00B9  ¹   c2 b9   SUPERSCRIPT ONE
    m_utf8toAscii["\xc2\xb9"] = '1';
    // U+00BA  º   c2 ba   MASCULINE ORDINAL INDICATOR
    m_utf8toAscii["\xc2\xba"] = 'o';
    // U+00BB  »   c2 bb   RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK
    m_utf8toAscii["\xc2\xbb"] = '>';
    // U+00BF  ¿   c2 bf   INVERTED QUESTION MARK
    m_utf8toAscii["\xc2\xbf"] = '?';
    // U+00C0  À   c3 80   LATIN CAPITAL LETTER A WITH GRAVE
    m_utf8toAscii["\xc3\x80"] = 'A';
    // U+00C1  Á   c3 81   LATIN CAPITAL LETTER A WITH ACUTE
    m_utf8toAscii["\xc3\x81"] = 'A';
    // U+00C2  Â   c3 82   LATIN CAPITAL LETTER A WITH CIRCUMFLEX
    m_utf8toAscii["\xc3\x82"] = 'A';
    // U+00C3  Ã   c3 83   LATIN CAPITAL LETTER A WITH TILDE
    m_utf8toAscii["\xc3\x83"] = 'A';
    // U+00C4  Ä   c3 84   LATIN CAPITAL LETTER A WITH DIAERESIS
    m_utf8toAscii["\xc3\x84"] = 'A';
    // U+00C5  Å   c3 85   LATIN CAPITAL LETTER A WITH RING ABOVE
    m_utf8toAscii["\xc3\x85"] = 'A';
    // U+00C6  Æ   c3 86   LATIN CAPITAL LETTER AE
    m_utf8toAscii["\xc3\x86"] = 'A';
    // U+00C7  Ç   c3 87   LATIN CAPITAL LETTER C WITH CEDILLA
    m_utf8toAscii["\xc3\x87"] = 'C';
    // U+00C8  È   c3 88   LATIN CAPITAL LETTER E WITH GRAVE
    m_utf8toAscii["\xc3\x88"] = 'E';
    // U+00C9  É   c3 89   LATIN CAPITAL LETTER E WITH ACUTE
    m_utf8toAscii["\xc3\x89"] = 'E';
    // U+00CA  Ê   c3 8a   LATIN CAPITAL LETTER E WITH CIRCUMFLEX
    m_utf8toAscii["\xc3\x8a"] = 'E';
    // U+00CB  Ë   c3 8b   LATIN CAPITAL LETTER E WITH DIAERESIS
    m_utf8toAscii["\xc3\x8b"] = 'E';
    // U+00CC  Ì   c3 8c   LATIN CAPITAL LETTER I WITH GRAVE
    m_utf8toAscii["\xc3\x8c"] = 'I';
    // U+00CD  Í   c3 8d   LATIN CAPITAL LETTER I WITH ACUTE
    m_utf8toAscii["\xc3\x8d"] = 'I';
    // U+00CE  Î   c3 8e   LATIN CAPITAL LETTER I WITH CIRCUMFLEX
    m_utf8toAscii["\xc3\x8e"] = 'I';
    // U+00CF  Ï   c3 8f   LATIN CAPITAL LETTER I WITH DIAERESIS
    m_utf8toAscii["\xc3\x8f"] = 'I';
    // U+00D0  Ð   c3 90   LATIN CAPITAL LETTER ETH
    m_utf8toAscii["\xc3\x90"] = 'D';
    // U+00D1  Ñ   c3 91   LATIN CAPITAL LETTER N WITH TILDE
    m_utf8toAscii["\xc3\x91"] = 'N';
    // U+00D2  Ò   c3 92   LATIN CAPITAL LETTER O WITH GRAVE
    m_utf8toAscii["\xc3\x92"] = 'O';
    // U+00D3  Ó   c3 93   LATIN CAPITAL LETTER O WITH ACUTE
    m_utf8toAscii["\xc3\x93"] = 'O';
    // U+00D4  Ô   c3 94   LATIN CAPITAL LETTER O WITH CIRCUMFLEX
    m_utf8toAscii["\xc3\x94"] = 'O';
    // U+00D5  Õ   c3 95   LATIN CAPITAL LETTER O WITH TILDE
    m_utf8toAscii["\xc3\x95"] = 'O';
    // U+00D6  Ö   c3 96   LATIN CAPITAL LETTER O WITH DIAERESIS
    m_utf8toAscii["\xc3\x96"] = 'O';
    // U+00D7  ×   c3 97   MULTIPLICATION SIGN
    m_utf8toAscii["\xc3\x97"] = 'x';
    // U+00D8  Ø   c3 98   LATIN CAPITAL LETTER O WITH STROKE
    m_utf8toAscii["\xc3\x98"] = '0';
    // U+00D9  Ù   c3 99   LATIN CAPITAL LETTER U WITH GRAVE
    m_utf8toAscii["\xc3\x99"] = 'U';
    // U+00DA  Ú   c3 9a   LATIN CAPITAL LETTER U WITH ACUTE
    m_utf8toAscii["\xc3\x9a"] = 'U';
    // U+00DB  Û   c3 9b   LATIN CAPITAL LETTER U WITH CIRCUMFLEX
    m_utf8toAscii["\xc3\x9b"] = 'U';
    // U+00DC  Ü   c3 9c   LATIN CAPITAL LETTER U WITH DIAERESIS
    m_utf8toAscii["\xc3\x9c"] = 'U';
    // U+00DD  Ý   c3 9d   LATIN CAPITAL LETTER Y WITH ACUTE
    m_utf8toAscii["\xc3\x9d"] = 'Y';
    // U+00DE  Þ   c3 9e   LATIN CAPITAL LETTER THORN
    m_utf8toAscii["\xc3\x9e"] = 'P';
    // U+00DF  ß   c3 9f   LATIN SMALL LETTER SHARP S
    m_utf8toAscii["\xc3\x9f"] = 'B';
    // U+00E0  à   c3 a0   LATIN SMALL LETTER A WITH GRAVE
    m_utf8toAscii["\xc3\xa0"] = 'a';
    // U+00E1  á   c3 a1   LATIN SMALL LETTER A WITH ACUTE
    m_utf8toAscii["\xc3\xa1"] = 'a';
    // U+00E2  â   c3 a2   LATIN SMALL LETTER A WITH CIRCUMFLEX
    m_utf8toAscii["\xc3\xa2"] = 'a';
    // U+00E3  ã   c3 a3   LATIN SMALL LETTER A WITH TILDE
    m_utf8toAscii["\xc3\xa3"] = 'a';
    // U+00E4  ä   c3 a4   LATIN SMALL LETTER A WITH DIAERESIS
    m_utf8toAscii["\xc3\xa4"] = 'a';
    // U+00E5  å   c3 a5   LATIN SMALL LETTER A WITH RING ABOVE
    m_utf8toAscii["\xc3\xa5"] = 'a';
    // U+00E6  æ   c3 a6   LATIN SMALL LETTER AE
    m_utf8toAscii["\xc3\xa6"] = 'a';
    // U+00E7  ç   c3 a7   LATIN SMALL LETTER C WITH CEDILLA
    m_utf8toAscii["\xc3\xa7"] = 'c';
    // U+00E8  è   c3 a8   LATIN SMALL LETTER E WITH GRAVE
    m_utf8toAscii["\xc3\xa8"] = 'e';
    // U+00E9  é   c3 a9   LATIN SMALL LETTER E WITH ACUTE
    m_utf8toAscii["\xc3\xa9"] = 'e';
    // U+00EA  ê   c3 aa   LATIN SMALL LETTER E WITH CIRCUMFLEX
    m_utf8toAscii["\xc3\xaa"] = 'e';
    // U+00EB  ë   c3 ab   LATIN SMALL LETTER E WITH DIAERESIS
    m_utf8toAscii["\xc3\xab"] = 'e';
    // U+00EC  ì   c3 ac   LATIN SMALL LETTER I WITH GRAVE
    m_utf8toAscii["\xc3\xac"] = 'i';
    // U+00ED  í   c3 ad   LATIN SMALL LETTER I WITH ACUTE
    m_utf8toAscii["\xc3\xad"] = 'i';
    // U+00EE  î   c3 ae   LATIN SMALL LETTER I WITH CIRCUMFLEX
    m_utf8toAscii["\xc3\xae"] = 'i';
    // U+00EF  ï   c3 af   LATIN SMALL LETTER I WITH DIAERESIS
    m_utf8toAscii["\xc3\xaf"] = 'i';
    // U+00F0  ð   c3 b0   LATIN SMALL LETTER ETH
    m_utf8toAscii["\xc3\xb0"] = 'o';
    // U+00F1  ñ   c3 b1   LATIN SMALL LETTER N WITH TILDE
    m_utf8toAscii["\xc3\xb1"] = 'n';
    // U+00F2  ò   c3 b2   LATIN SMALL LETTER O WITH GRAVE
    m_utf8toAscii["\xc3\xb2"] = 'o';
    // U+00F3  ó   c3 b3   LATIN SMALL LETTER O WITH ACUTE
    m_utf8toAscii["\xc3\xb3"] = 'o';
    // U+00F4  ô   c3 b4   LATIN SMALL LETTER O WITH CIRCUMFLEX
    m_utf8toAscii["\xc3\xb4"] = 'o';
    // U+00F5  õ   c3 b5   LATIN SMALL LETTER O WITH TILDE
    m_utf8toAscii["\xc3\xb5"] = 'o';
    // U+00F6  ö   c3 b6   LATIN SMALL LETTER O WITH DIAERESIS
    m_utf8toAscii["\xc3\xb6"] = 'o';
    // U+00F7  ÷   c3 b7   DIVISION SIGN
    m_utf8toAscii["\xc3\xb7"] = '/';
    // U+00F8  ø   c3 b8   LATIN SMALL LETTER O WITH STROKE
    m_utf8toAscii["\xc3\xb8"] = 'o';
    // U+00F9  ù   c3 b9   LATIN SMALL LETTER U WITH GRAVE
    m_utf8toAscii["\xc3\xb9"] = 'u';
    // U+00FA  ú   c3 ba   LATIN SMALL LETTER U WITH ACUTE
    m_utf8toAscii["\xc3\xba"] = 'u';
    // U+00FB  û   c3 bb   LATIN SMALL LETTER U WITH CIRCUMFLEX
    m_utf8toAscii["\xc3\xbb"] = 'u';
    // U+00FC  ü   c3 bc   LATIN SMALL LETTER U WITH DIAERESIS
    m_utf8toAscii["\xc3\xbc"] = 'u';
    // U+00FD  ý   c3 bd   LATIN SMALL LETTER Y WITH ACUTE
    m_utf8toAscii["\xc3\xbd"] = 'y';
    // U+00FE  þ   c3 be   LATIN SMALL LETTER THORN
    m_utf8toAscii["\xc3\xbe"] = 'p';
    // U+00FF  ÿ   c3 bf   LATIN SMALL LETTER Y WITH DIAERESIS
    m_utf8toAscii["\xc3\xbf"] ='y' ;

    // The following mappings are not typical or valid, but are used in
    // some attacks.

    // U+FF0E
    m_utf8toAscii["\xff\x0e"] = '.';
    // U+EFC8
    m_utf8toAscii["\xef\xc8"] = '/';
    // U+F025
    m_utf8toAscii["\xf0\x25"] = '/';
    // U+2216
    m_utf8toAscii["\x22\x16"] = '\\';
    // U+2215
    m_utf8toAscii["\x22\x15"] = '/';
}

} /* Anonymous Namespace. */

IBPP_BOOTSTRAP_MODULE_DELEGATE("utf8", Utf8ModuleDelegate);
