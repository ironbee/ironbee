/**
 * \mainpage
 *
 * The modp_b family of encoders/decoders.
 *
 * Most have a similar interface:
 *  - modp_bXXX_encode(char* dest, char* src, int len) -- encodes src, and puts the result in dest.  The caller allocates dest.  It returns the strlen of the output.
 *  - modp_bXXX_encode_len(int len) -- returns the amount of memory needed to be allocated BEFORE calling _encode.
 *  - modp_bXXX_encode_strlen(int len) -- returns the strlen of the encoded output (but without doing the encode operation)
 *  - modp_bXXX_decode(char* dest, char* src, int len) -- decodes src and puts result in dest.  Returns the number of bytes written.
 *  - modp_bXXX_decode_len(int len) -- the amount of memory needed to decode.
 *
 * The header files all include a sample C++ std::string wrapper.
 *
 * In addition:
 * - modp_numtoa.h defines fast integer and float  types to char buffer converts.
 * - modp_ascii.h defines fast toupper, tolower transformations.
 *
 * \section modp_b64
 *
 * Converts 3 bytes into 4 characters, for 1.33 expansion ratio. This
 * version is ridiculously fast -- on some platforms the decode
 * operation is 4x faster than a standard implementation.
 *
 * \section modp_b64w
 *
 * Does the same type of transformation as modp_b64 but uses a
 * slightly different alphabet to make it "safe" to use inside a URL.
 * The mapping is: "/" to "_", * "+" to "-", * "=" to "."  If you are
 * intergating with another base64 encoder, you may need to change
 * this. See the mod_b64w.h header file for details.
 *
 * \section modp_b16
 *
 * This is the standard "binary to ascii" encoding that convert 1 byte
 * into two chars (0-9, a-f).  It's actually slower than base64 so
 * there is not reason to use this except for legacy applications.
 * (how can this be?  Because on decode we have to read 4 bytes to get
 * 2.  and in base64 we read 4 bytes to get 3, so base64's loop is
 * shorter).
 *
 * \section modp_b85
 *
 * Base 85 is the "most dense ascii encoding" possible, converting 4
 * bytes into 5 chars (1.2).  The ouput is 11% smaller than base 64
 * (1.33/1.2), but unfortunately, it's about twice as slow as base-64
 * encoding since true division has to be used (instead of raw bit
 * operations).
 *
 * \section mod_b2
 * Converts the given string to a base 2 or binary encoding (all
 * 1s and 0s).  For useful for debugging.
 *
 * \section modp_burl
 *
 * This performs url-encoding and url-decoding.  While not a true base
 * converted like the others, it does use an optimized base-16
 * converter for the encoded "%XY" data.  This has an alternate
 * encoder that provides a minimal encoding, modp_burl_min_encode.
 * See modp_burl.h for details
 *
 * \section modp_bjavascript
 *
 * Converts a raw c-string into something that can be enbedded into
 * javascript.  This might be useful for server-generated dynamic
 * javascript.  There is no decode function provided.  This is only
 * use when generating raw "text/javascript" files.  It is <b>NOT</b>
 * safe to make javascript that will ultimately be embedded inside
 * HTML via script tags.
 *
 * \section modp_numtoa
 *
 * The functions modp_itoa, modp_uitoa, modp_dtoa converts signed integers,
 * unsigned integers, and 'double' type conversion to char buffer (string).
 * The advantages over sprintf/snprintf are
 *  - core dump proof
 *  - have a fixed maximum size (e.g. try printf("%f", 2.0e100) for example)
 *  - 5-22x faster!
 *
 * See modp_numtoa.h for details
 *
 * \section modp_ascii
 *
 * modp_toupper and modp_tolower upper or lower case a string using the standard C
 * locale (i.e. 7-bit ascii).  These are 2-22x faster than using standard ctype
 * functions.  Also include is "toprint" which replaces "unprintable" characters
 * with a "?".
 *
 *
 */
