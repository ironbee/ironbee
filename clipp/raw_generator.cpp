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
 * @brief IronBee &mdash; CLIPP Raw Generator Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "raw_generator.hpp"

#include <iostream>
#include <fstream>
#include <stdexcept>

using namespace std;

namespace IronBee {
namespace CLIPP {

namespace {

//! Remaining bytes in @a f.
size_t remaining(ifstream& f)
{
    size_t length;
    streampos current = f.tellg();
    f.seekg(0, ios::end);
    length = f.tellg();
    f.seekg(current, ios::beg);

    return length - current;
}

//! Load @a file into @a buffer.
void load(vector<char>& buffer, const string& file)
{
    ifstream in(file.c_str());
    if (! in) {
        throw runtime_error("Could not read " + file);
    }
    size_t length = remaining(in);
    buffer = vector<char>(length);
    in.read(&*buffer.begin(), length);
}

}

RawGenerator::RawGenerator()
{
    // nop
}

RawGenerator::RawGenerator(
    const std::string& request_path,
    const std::string& response_path
) :
    m_produced_input(false)
{
    load(m_request_buffer,  request_path);
    load(m_response_buffer, response_path);
}

bool RawGenerator::operator()(input_t& out_input)
{
    if (m_produced_input) {
        return false;
    }

    out_input.local_ip          = buffer_t(local_ip);
    out_input.remote_ip         = buffer_t(remote_ip);
    out_input.local_port        = local_port;
    out_input.remote_port       = remote_port;
    out_input.transactions.clear();
    out_input.transactions.push_back(
        input_t::transaction_t(
            buffer_t(
                &*m_request_buffer.begin(),
                m_request_buffer.size()
            ),
           buffer_t(
               &*m_response_buffer.begin(),
               m_response_buffer.size()
           )
        )
    );

    m_produced_input = true;

    return true;
}

const std::string RawGenerator::local_ip    = "1.2.3.4";
const std::string RawGenerator::remote_ip   = "5.6.7.8";
const uint16_t    RawGenerator::local_port  = 1234;
const uint16_t    RawGenerator::remote_port = 5678;

} // CLIPP
} // IronBee
