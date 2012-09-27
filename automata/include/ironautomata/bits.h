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

#ifndef _IA_EUDOXUS_BITS_H_
#define _IA_EUDOXUS_BITS_H_

/**
 * @file
 * @brief IronAutomata &mdash; Eudoxus Bit Manipulation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <stdbool.h>
#include <stdint.h>

/**
 * @defgroup IronAutomataBits Bits
 * @ingroup IronAutomata
 *
 * Bit manipulation routines.
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Return @a i th bit of a uint8_t.
 *
 * @param[in] byte Byte to read bit of.
 * @param[in] i    Index of bit to read.
 * @return true iff @a i th bit of @a byte is 1.
 */
static
inline
bool ia_bit8(uint8_t byte, int i)
{
    return (byte & (1 << i)) != 0;
}

/**
 * Return @a i th bit of a uint16_t.
 *
 * @param[in] word Word to read bit of.
 * @param[in] i    Index of bit to read.
 * @return true iff @a i th bit of @a word is 1.
 */
static
inline
bool ia_bit16(uint16_t word, int i)
{
    return (word & (1 << i)) != 0;
}

/**
 * Return @a i th bit of a uint32_t.
 *
 * @param[in] word Word to read bit of.
 * @param[in] i    Index of bit to read.
 * @return true iff @a i th bit of @a word is 1.
 */
static
inline
bool ia_bit32(uint32_t word, int i)
{
    return (word & (1 << i)) != 0;
}

/**
 * Return @a i th bit of a uint64_t.
 *
 * @param[in] word Word to read bit of.
 * @param[in] i    Index of bit to read.
 * @return true iff @a i th bit of @a word is 1.
 */
static
inline
bool ia_bit64(uint64_t word, int i)
{
    return (word & (1 << i)) != 0;
}

/**
 * Returns @a i th bit of a byte sequence.
 *
 * This function is for variable length byte sequences.  For fixed multiple
 * of two bytes, e.g., uint32_t, prefer other methods.
 *
 * @param[in] bytes Bytes to read from.
 * @param[in] i     Index of bit to read.
 * @return true iff @a i th bit of @a bytes is 1.
 */
static
inline
bool ia_bitv(const uint8_t *bytes, int i)
{
    return ia_bit8(bytes[i / 8], i % 8);
}

/**
 * Return @a byte with @a i th bit set to 1.
 *
 * @param[in] byte Byte to set bit of.
 * @param[in] i    Bit to set.
 * @return @a byte with @a i th bit set to 1.
 */
static
inline
uint8_t ia_setbit8(uint8_t byte, int i)
{
    return byte | (1 << i);
}

/**
 * Return @a word with @a i th bit set to 1.
 *
 * @param[in] word Word to set bit of.
 * @param[in] i    Bit to set.
 * @return @a word with @a i th bit set to 1.
 */
static
inline
uint16_t ia_setbit16(uint16_t word, int i)
{
    return word | (1 << i);
}

/**
 * Return @a word with @a i th bit set to 1.
 *
 * @param[in] word Word to set bit of.
 * @param[in] i    Bit to set.
 * @return @a word with @a i th bit set to 1.
 */
static
inline
uint32_t ia_setbit32(uint32_t word, int i)
{
    return word | (1 << i);
}

/**
 * Return @a word with @a i th bit set to 1.
 *
 * @param[in] word Word to set bit of.
 * @param[in] i    Bit to set.
 * @return @a word with @a i th bit set to 1.
 */
static
inline
uint64_t ia_setbit64(uint64_t word, int i)
{
    return word | (1 << i);
}

/**
 * Change the @a i th bit of the the bytes at @a bytes to 1.
 *
 * @param[in] bytes Byte sequence to change.
 * @param[in] i     Bit to set.
 */
static
inline
void ia_setbitv(uint8_t *bytes, int i)
{
    bytes[i / 8] = ia_setbit8(bytes[i / 8], i % 8);
}

/**
 * @} IronAutomataBits
 */

#ifdef __cplusplus
}
#endif

#endif /* _IA_EUDOXUS_BITS_H_ */
