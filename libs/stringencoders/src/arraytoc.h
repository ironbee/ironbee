/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
/* vi: set expandtab shiftwidth=4 tabstop=4: */

#ifndef COM_MODP_STRINGENCODERS_ARRAYTOC
#define COM_MODP_STRINGENCODERS_ARRAYTOC

#include <stdint.h>

#ifdef __cplusplus
#define BEGIN_C extern "C" {
#define END_C }
#else
#define BEGIN_C
#define END_C
#endif

BEGIN_C

/** \brief output a uint32_t array into source code
 *
 *
 * \param[in] ary the input array
 * \param[in] size number of elements in array
 * \param[in] name the name of the struct for the source code
 *
 */
void uint32_array_to_c(const uint32_t* ary, int size, const char* name);

/** \brief output an uint32_t array into source code as hex values
 *
 * \param[in] ary the input array
 * \param[in] size number of elements in array
 * \param[in] name the name of the struct for source code
 *
 */
void uint32_array_to_c_hex(const uint32_t* ary, int size, const char* name);

/** \brief output a char array into source code
 *
 * \param[in] ary the input array
 * \param[in] size number of elements in array
 * \param[in] name the name of the struct for source code
 */
void char_array_to_c(const char* ary, int size, const char* name);

#endif
