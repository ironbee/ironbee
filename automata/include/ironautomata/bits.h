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
    return (word & ((uint64_t)1 << i)) != 0;
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
 * Returns @a i th bit of a 64 bit word sequence.
 *
 * @param[in] words Bytes to read from.
 * @param[in] i     Index of bit to read.
 * @return true iff @a i th bit of @a bytes is 1.
 */
static
inline
bool ia_bitv64(const uint64_t *words, int i)
{
    return ia_bit64(words[i / 64], i % 64);
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
 * Return @a byte with @a i th bit set to 0.
 *
 * @param[in] byte Byte to unset bit of.
 * @param[in] i    Bit to unset.
 * @return @a byte with @a i th bit set to 0.
 */
static
inline
uint8_t ia_unsetbit8(uint8_t byte, int i)
{
    return byte & ~(1 << i);
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
 * Return @a word with @a i th bit set to 0.
 *
 * @param[in] word Word to unset bit of.
 * @param[in] i    Bit to unset.
 * @return @a word with @a i th bit set to 0.
 */
static
inline
uint16_t ia_unsetbit16(uint16_t word, int i)
{
    return word & ~(1 << i);
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
 * Return @a word with @a i th bit set to 0.
 *
 * @param[in] word Word to unset bit of.
 * @param[in] i    Bit to unset.
 * @return @a word with @a i th bit set to 0.
 */
static
inline
uint32_t ia_unsetbit32(uint32_t word, int i)
{
    return word & ~(1 << i);
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
    return word | ((uint64_t)1 << i);
}

/**
 * Return @a word with @a i th bit set to 0.
 *
 * @param[in] word Word to unset bit of.
 * @param[in] i    Bit to unset.
 * @return @a word with @a i th bit set to 0.
 */
static
inline
uint64_t ia_unsetbit64(uint64_t word, int i)
{
    return word & ~((uint64_t)1 << i);
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
 * Change the @a i th bit of the the bytes at @a bytes to 0.
 *
 * @param[in] bytes Byte sequence to change.
 * @param[in] i     Bit to unset.
 */
static
inline
void ia_unsetbitv(uint8_t *bytes, int i)
{
    bytes[i / 8] = ia_unsetbit8(bytes[i / 8], i % 8);
}

/**
 * Change the @a i th bit of the 64 bit words at @a words to 1.
 *
 * @param[in] words Pointer to uint64_ts.
 * @param[in] i     Bit to set.
 */
static
inline
void ia_setbitv64(uint64_t *words, int i)
{
    words[i / 64] = ia_setbit64(words[i / 64], i % 64);
}

/**
 * Change the @a i th bit of the 64 bit words at @a words to 0.
 *
 * @param[in] words Pointer to uint64_ts.
 * @param[in] i     Bit to set.
 */
static
inline
void ia_unsetbitv64(uint64_t *words, int i)
{
    words[i / 64] = ia_unsetbit64(words[i / 64], i % 64);
}

/**
 * Population count of a 64 bit word.
 *
 * @param[in] word Word to count 1s of.
 * @return Number of 1s.
 */
static
inline
int ia_popcount64(uint64_t word)
{
#if __GNUC__ >= 4
    return __builtin_popcountll(word);
#else
#error "__builtin_popcountll support required.  Please report this to developers."
#endif
}

/**
 * Population count of 64 bit words.
 *
 * @param[in] words Words to counts 1s of.
 * @param[in] i     Index of last bit to look at.
 * @return Number of 1s.
 */
static
inline
int ia_popcountv64(const uint64_t *words, int i)
{
    int acc = 0;
    for (int j = 0; j < i / 64; ++j) {
        acc += ia_popcount64(words[j]);
    }
    if ((i % 64) > 0) {
        acc += ia_popcount64(
            words[i / 64] & (~(uint64_t)0 >> (63 - (i % 64)))
        );
    }
    return acc;
}

/**
 * @} IronAutomataBits
 */

#ifdef __cplusplus
}
#endif

#endif /* _IA_EUDOXUS_BITS_H_ */
