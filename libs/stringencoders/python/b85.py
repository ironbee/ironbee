#!/usr/bin/env python

#
# This is the MIT License
# http://www.opensource.org/licenses/mit-license.php
#
# Copyright (c) 2008 Nick Galbreath
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#

#import psyco
#psyco.full()

gsCharToInt = [
    256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256,
    256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256,
    256, 256, 256, 256, 256, 256, 256, 256, 256,   0, 256,   1,
    2,   3, 256,   4,   5,   6,   7,   8, 256,   9,  10,  11,
    12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22, 256,
    23,  24,  25,  26,  27,  28,  29,  30,  31,  32,  33,  34,
    35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,
    47,  48,  49,  50,  51,  52,  53,  54, 256,  55,  56,  57,
    58,  59,  60,  61,  62,  63,  64,  65,  66,  67,  68,  69,
    70,  71,  72,  73,  74,  75,  76,  77,  78,  79,  80,  81,
    82,  83,  84, 256, 256, 256, 256, 256, 256, 256, 256, 256,
    256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256,
    256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256,
    256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256,
    256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256,
    256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256,
    256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256,
    256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256,
    256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256,
    256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256,
    256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256,
    256, 256, 256, 256]

gsIntToChar = [
    '!',  '#',  '$',  '%', '\'',  '(',  ')',  '*',  '+',  '-',
    '.',  '/',  '0',  '1',  '2',  '3',  '4',  '5',  '6',  '7',
    '8',  '9',  ':',  '<',  '=',  '>',  '?',  '@',  'A',  'B',
    'C',  'D',  'E',  'F',  'G',  'H',  'I',  'J',  'K',  'L',
    'M',  'N',  'O',  'P',  'Q',  'R',  'S',  'T',  'U',  'V',
    'W',  'X',  'Y',  'Z',  '[',  ']',  '^',  '_',  '`',  'a',
    'b',  'c',  'd',  'e',  'f',  'g',  'h',  'i',  'j',  'k',
    'l',  'm',  'n',  'o',  'p',  'q',  'r',  's',  't',  'u',
    'v',  'w',  'x',  'y',  'z'
    ]

from struct import unpack, pack
# covert 4 characters into 5
def b85_encode(s):
    parts = []
    numchunks = len(s) // 4
    format = '!' + str(numchunks) + 'I'
    for x in  unpack(format, s):
        # network order (big endian), 32-bit unsigned integer
        # note: x86 is little endian
        parts.append(gsIntToChar[x // 52200625])
        parts.append(gsIntToChar[(x // 614125) % 85])
        parts.append(gsIntToChar[(x // 7225) % 85])
        parts.append(gsIntToChar[(x // 85) % 85])
        parts.append(gsIntToChar[x % 85])
    return ''.join(parts)

#
# MAY be 10-20% faster, when running on pysco
#   certainly 2x SLOWER when running normally.
#
# also does not use the 'struct' module which may be desirable
# to some
#
def b85_encode2(s):
    parts = []
    for i in xrange(0,len(s), 4):
        chunk = s[i:i+4]
        x = ord(chunk[3]) + 256*(ord(chunk[2]) + 256*(ord(chunk[1]) + 256*ord(chunk[0])))

        # network order (big endian), 32-bit unsigned integer
        # note: x86 is little endian
        parts.append(gsIntToChar[x // 52200625])
        parts.append(gsIntToChar[(x // 614125) % 85])
        parts.append(gsIntToChar[(x // 7225) % 85])
        parts.append(gsIntToChar[(x // 85) % 85])
        parts.append(gsIntToChar[x % 85])
    return ''.join(parts)

# convert 5 characters to 4
def b85_decode(s):
    parts = []
    for i in xrange(0, len(s), 5):
        bsum = 0;
        for j in xrange(0,5):
            val = gsCharToInt[ord(s[i+j])]
            bsum = 85*bsum + val
        tmp = pack('!I', bsum)
        parts.append(tmp)
        #parts += tmp 
        #parts += unpack('cccc', tmp)
    return ''.join(parts)

# convert 5 characters to 4
def b85_decode2(s):
    parts = []
    for i in xrange(0, len(s), 5):
        bsum = 0;
        for j in xrange(0,5):
            val = gsCharToInt[ord(s[i+j])]
            bsum = 85*bsum + val
        parts.append(chr((bsum >> 24) & 0xff))
        parts.append(chr((bsum >> 16) & 0xff))
        parts.append(chr((bsum >> 8) & 0xff))
        parts.append(chr(bsum & 0xff))

    return ''.join(parts)

import unittest
class B85Test(unittest.TestCase):

    def testDecode1(self):
        s = b85_decode('!!!!#')
        self.assertEquals(4, len(s))
        self.assertEquals(0, ord(s[0]))
        self.assertEquals(0, ord(s[1]))
        self.assertEquals(0, ord(s[2]))
        self.assertEquals(1, ord(s[3]))

        e = b85_encode(s)
        self.assertEquals('!!!!#', e)


if __name__ == '__main__':
    from time import clock
    #unittest.main()

    s = '!!!!#' * 10

    t0 = clock()
    for i in xrange(1000000):
        b85_decode(s)
    t1 = clock()
    print "decode v1",  t1-t0

    t0 = clock()
    for i in xrange(1000000):
       b85_decode2(s)
    t1 = clock()
    print "decode v2",  t1-t0


    s = b85_decode('!!!!#' * 10)

    t0 = clock()
    for i in xrange(1000000):
       b85_encode(s)
    t1 = clock()
    print "encode v1",  t1-t0

    t0 = clock()
    for i in xrange(1000000):
        b85_encode2(s)
    t1 = clock()
    print "encode v2",  t1-t0


