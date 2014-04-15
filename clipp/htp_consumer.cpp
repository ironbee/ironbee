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
 * @brief IronBee --- CLIPP HTP Test Consumer Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <clipp/htp_consumer.hpp>

#include <boost/make_shared.hpp>

#include <fstream>

using namespace std;

namespace IronBee {
namespace CLIPP {

namespace  {

class HTPConsumerDelegate :
    public Input::Delegate
{
public:
    explicit
    HTPConsumerDelegate(ostream& output) :
        m_output(output)
    {
        // nop
    }

    void connection_data_in(const Input::DataEvent& event)
    {
        m_output << ">>>" << endl;
        m_output.write(event.data.data, event.data.length);
    }

    void connection_data_out(const Input::DataEvent& event)
    {
        m_output << "<<<" << endl;
        m_output.write(event.data.data, event.data.length);
    }

private:
    ostream& m_output;
};

}

struct HTPConsumer::State
{
    State(const std::string& path) :
        output(path.c_str(), ios::binary)
    {
        if (! output) {
            throw runtime_error("Could not open " + path + " for writing.");
        }
    }

    ofstream output;
};

HTPConsumer::HTPConsumer()
{
    // nop
}

HTPConsumer::HTPConsumer(const std::string& output_path) :
    m_state(boost::make_shared<State>(output_path))
{
    // nop
}

bool HTPConsumer::operator()(const Input::input_p& input)
{
    if (! m_state || ! m_state->output) {
        return false;
    }

    HTPConsumerDelegate delegate(m_state->output);
    input->connection.dispatch(delegate);

    return true;
}

} // CLIPP
} // IronBee
