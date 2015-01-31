/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
/* vi: set expandtab shiftwidth=4 tabstop=4: */

/**
 * \file
 * <PRE>
 * High Performance c-string to javascript-string encoder
 *
 * Copyright &copy; 2006, 2007  Nick Galbreath -- nickg [at] modp [dot] com
 * All rights reserved.
 *
 * http://code.google.com/p/stringencoders/
 *
 * Released under bsd license.  See modp_bjavascript.c for details.
 * </PRE>
 */

#ifndef COM_MODP_STRINGENCODERS_BJAVASCRIPT
#define COM_MODP_STRINGENCODERS_BJAVASCRIPT

#ifdef __cplusplus
#define BEGIN_C extern "C" {
#define END_C }
#else
#define BEGIN_C
#define END_C
#endif

BEGIN_C


/**
 * "javascript" encode a stirng
 * This takes a c-string and does character escaping
 * so it can be put into a var js_string = '...';
 *
 * \param[out] dest output string.  Must
 * \param[in] str The input string
 * \param[in] len  The length of the input string, excluding any
 *   final null byte.
 */
int modp_bjavascript_encode(char* dest, const char* str, int len);

#define modp_bjavascript_encode_len(A) (4*A + 1)

/**
 * Given the exact size of output string.
 *
 * Can be used to allocate the right amount of memory for
 * modp_burl_encode.  Be sure to add 1 byte for final null.
 *
 * This is somewhat expensive since it examines every character
 *  in the input string
 *
 * \param[in] str  The input string
 * \param[in] len  THe length of the input string, excluding any
 *   final null byte (i.e. strlen(str))
 * \return the size of the output string, excluding the final
 *   null byte.
 */
int modp_bjavascript_encode_strlen(const char* str, int len);

END_C

#ifdef __cplusplus
#include <cstring>
#include <string>
namespace modp {

    inline std::string javascript_encode(const char* s, size_t len)
    {
        std::string x(modp_bjavascript_encode_len(len), '\0');
        int d = modp_bjavascript_encode(const_cast<char*>(x.data()), s, len);
        x.erase(d, std::string::npos);
        return x;
    }

    inline std::string javascript_encode(const char* s)
    {
        return javascript_encode(s, strlen(s));
    }

    inline std::string& javascript_encode(std::string& s)
    {
        std::string x(javascript_encode(s.data(), s.size()));
        s.swap(x);
        return s;
    }

    inline std::string javascript_encode(const std::string& s)
    {
        return javascript_encode(s.data(), s.size());
    }

}       /* namespace modp */
#endif  /* __cplusplus */

#endif /* modp_bjavascript */
