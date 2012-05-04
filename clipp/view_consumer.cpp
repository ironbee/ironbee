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
 * @brief IronBee &mdash; CLIPP View Consumer Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "view_consumer.hpp"

#include <boost/foreach.hpp>

using namespace std;

namespace IronBee {
namespace CLIPP {

namespace {

bool is_not_printable(char c)
{
    return (c < 32 || c > 126) && (c != 10) && (c != 13);
}

void output_with_escapes(const char* b, const char* e)
{
    const char* i = b;
    while (i < e) {
        const char* j = find_if(i, e, &is_not_printable);
        if (j == e) {
            cout.write(i, e - i);
            i = e;
        }
        else {
            if (j > i) {
                cout.write(i, j - i);
                i = j;
            }
            cout << "[" << static_cast<uint32_t>(*i) << "]";
            ++i;
        }
    }
}

}

bool ViewConsumer::operator()(const input_p& input)
{
    if (input->id.empty()) {
        cout << "---- No ID Provided ----" << endl;
    }
    else {
        cout << "---- " << input->id << " ----" << endl;
    }
    cout << input->local_ip.to_s() << ":" << input->local_port
         << " <---> "
         << input->remote_ip.to_s() << ":" << input->remote_port
         << endl;

    BOOST_FOREACH(const input_t::transaction_t& tx, input->transactions) {
        cout << "==== REQUEST ====" << endl;
        output_with_escapes(
            tx.request.data,
            tx.request.data + tx.request.length
        );
        cout << "==== RESPONSE ====" << endl;
        output_with_escapes(
            tx.response.data,
            tx.response.data + tx.response.length
        );
    }

    return true;
}

} // CLIPP
} // IronBee
