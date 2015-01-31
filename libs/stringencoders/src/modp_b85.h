/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
/* vi: set expandtab shiftwidth=4 tabstop=4: */

/**
 * \file
 * <pre>
 * High Performance Base85 Encoder / Decoder
 *
 * Copyright &copy; 2006,2007 Nick Galbreath -- nickg [at] modp [dot] com
 * All rights reserved.
 *
 * http://code.google.com/p/stringencoders/
 *
 * Released under bsd license.  See modp_b85.c for details.
 * </PRE>
 *
 * This provides a endian-safe base85 encode/decode operations.  This
 * means, the result will be the same on x86 or ibm/sparc chips.
 *
 * (Note: making it endian-specifc only results in a 5% savings in
 *  the decode operation, so why bother)
 */

#ifndef COM_MODP_STRINGENCODERS_B85
#define COM_MODP_STRINGENCODERS_B85

#ifdef __cplusplus
#define BEGIN_C extern "C" {
#define END_C }
#else
#define BEGIN_C
#define END_C
#endif

BEGIN_C

/**
 * base 85 encode
 *
 * \param[out] dest  should have at least b85fast_encode_len memory allocated
 * \param[in] src   input string
 * \param[in] len   input string length, must be a multiple of 4
 * \return the strlen of the destination, or -1 if error
 */
int modp_b85_encode(char* dest, const char* src, int len);

/**
 * Base 85 decode
 *
 * \param[out] dest -- destination locations.  May equal input.
 * \param[in] src -- source b85data
 * \param len -- length of source
 * \return -1 on decoding error, length of output otherwise
 *    No ending null is added
 */
int modp_b85_decode(char* dest, const char* src, int len);

/**
 * Returns the amount of memory to allocate for encoding the input
 * string.
 *
 */
#define modp_b85_encode_len(A) ((A + 3) / 4 * 5 + 1)

/**
 * Return output strlen, without a NULL
 */
#define modp_b85_encode_strlen(A) ((A + 3) / 4 * 5)

/**
 * Return the amount of memory to allocate for decoding a base 85
 * encoded string.
 *
 */
#define modp_b85_decode_len(A) ((A + 4) / 5 * 4)

END_C

#ifdef __cplusplus
#include <cstring>
#include <string>

namespace modp {

    /**
     *
     * \param[in] s the input data
     * \param[in] len the length of input
     * \return b85 encoded string
     */
    inline std::string b85_encode(const char* s, size_t len)
    {
        std::string x(modp_b85_encode_len(len), '\0');
        int d = modp_b85_encode(const_cast<char*>(x.data()), s,
                                static_cast<int>(len));
        if (d < 0) {
            x.clear();
        } else {
            x.erase(d, std::string::npos);
        }
        return x;
    }

    /**
     * \param[in] null-terminated c-string input
     * \return b85 encoded string
     */
    inline std::string b85_encode(const char* s)
    {
        return b85_encode(s, strlen(s));
    }

    /**
     * /param[in,out] s the string to encode
     * /return a reference to the input string, empty if error
     */
    inline std::string& b85_encode(std::string& s)
    {
        std::string x(b85_encode(s.data(), s.size()));
        s.swap(x);
        return s;
    }

    /**
     * \param[in] s the input string
     * \return base85 encoded string
     */
    inline std::string b85_encode(const std::string& s)
    {
        return b85_encode(s.data(), s.size());
    }

    /**
     * Base85 decode a string.
     * This function does not allocate memory.
     *
     * \param s the string to decode
     * \return a reference to the input string. The string is empty
     *   if an error occurred.
     */
    inline std::string& b85_decode(std::string& s)
    {
        int d = modp_b85_decode(const_cast<char*>(s.data()), s.data(),
                                static_cast<int>(s.size()));
        if (d < 0) {
            s.clear();
        } else {
            s.erase(d, std::string::npos);
        }
        return s;
    }

    inline std::string b85_decode(const std::string& s)
    {
        std::string x(s);
        b85_decode(x);
        return x;
    }

    inline std::string b85_decode(const char* s, size_t len)
    {
        std::string x(s,len);
        return b85_decode(x);
    }

    inline std::string b85_decode(const char* s)
    {
        std::string x(s);
        return b85_decode(x);
    }

}  /* namespace modp */

#endif  /* ifdef __cplusplus */

#endif  /* ifndef MODP_B85 */
