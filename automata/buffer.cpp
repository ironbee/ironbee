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
 * @brief IronAutomata --- Buffer Support Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironautomata/buffer.hpp>

using namespace std;

namespace IronAutomata {

BufferAssembler::BufferAssembler(buffer_t& buffer_) :
    m_buffer(buffer_)
{
    // nop
}

size_t BufferAssembler::extend(size_t n)
{
    size_t i = size();
    if (buffer().capacity() < i + n) {
        buffer().reserve(max(2 * buffer().capacity(), i + n));
    }
    return i;
}

char* BufferAssembler::append_string(const string& s)
{
    size_t i = extend(s.length());

    m_buffer.insert(m_buffer.begin() + i, s.begin(), s.end());

    return ptr<char>(i);
}

uint8_t* BufferAssembler::append_bytes(const uint8_t* v, size_t n)
{
    size_t i = extend(n);

    m_buffer.insert(m_buffer.begin() + i, v, v + n);

    return ptr<uint8_t>(i);
}

} // IronAutomata
