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
 * @brief IronBee --- CLIPP Raw Consumer Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "ironbee_config_auto.h"

#include "raw_consumer.hpp"

#include <boost/make_shared.hpp>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <fstream>
#include <map>
#include <stdexcept>

using namespace std;

namespace IronBee {
namespace CLIPP {

namespace  {

class RawConsumerDelegate :
    public Input::Delegate
{
public:
    explicit
    RawConsumerDelegate(const string& output_path) :
        m_output_path(output_path),
        m_info(NULL)
    {
        // nop
    }

    void request_started(const Input::RequestEvent& event)
    {
        ofstream& out = output(REQUEST);

        if (event.raw.length > 0) {
            out << event.raw << endl;
        }
        else {
            out << event.method << " "
                << event.uri << " "
                << event.protocol << endl;
        }
    }

    void request_header(const Input::HeaderEvent& event)
    {
        ofstream& out = output(REQUEST);

        BOOST_FOREACH(const Input::header_t& header, event.headers)
        {
            out << header.first << ": " << header.second << endl;
        }
    }

    void request_header_finished(const Input::NullEvent& event)
    {
        output(REQUEST) << endl;
    }

    void request_body(const Input::DataEvent& event)
    {
        output(REQUEST) << event.data;
    }

    void response_started(const Input::ResponseEvent& event)
    {
        ofstream& out = output(RESPONSE);

        if (event.raw.length > 0) {
            out << event.raw << endl;
        }
        else {
            out
                << event.protocol << " "
                << event.status << " "
                << event.message << endl;
        }
    }

    void response_header(const Input::HeaderEvent& event)
    {
        ofstream& out = output(RESPONSE);

        BOOST_FOREACH(const Input::header_t& header, event.headers)
        {
            out << header.first << ": " << header.second << endl;
        }
    }

    void response_header_finished(const Input::NullEvent& event)
    {
        output(RESPONSE) << endl;
    }

    void response_body(const Input::DataEvent& event)
    {
        output(RESPONSE) << event.data;
    }

    void begin_input(const string& id)
    {
        m_id = id;

        event_map_t::iterator i = m_event_infos.find(m_id);
        if (i == m_event_infos.end()) {
            i = m_event_infos.insert(
                i,
                make_pair(m_id, event_info_t())
            );
        }
        m_info = &i->second;
    }

private:
    enum event_info_e {
        NONE,
        REQUEST,
        RESPONSE
    };
    string output_path(const string& event_id, size_t id, event_info_e which)
    {
        static const string event_info_e_label[] = {
            "none",
            "request",
            "response"
        };

        return m_output_path + "/" +
			event_info_e_label[which] + "." +
            boost::lexical_cast<string>(id) + ".raw";
    }

    struct event_info_t
    {
        event_info_t() :
            which(NONE), next_id(1)
        {
            // nop
        }

        event_info_e which;
        size_t next_id;
        boost::shared_ptr<ofstream> file;
    };

    ofstream& output(event_info_e which)
    {
        if (m_info->which != which) {
			string path = output_path(m_id, m_info->next_id, which);
            m_info->file.reset(new ofstream(path.c_str()));
            ++m_info->next_id;
            m_info->which = which;
			
			if (! *(m_info->file)) {
				throw runtime_error("Error opening file: " + path);
			}
        }

        return *(m_info->file);
    }

    string m_id;
    string m_output_path;
    event_info_t* m_info;
    typedef map<string, event_info_t> event_map_t;
    event_map_t m_event_infos;
};

}

struct RawConsumer::State
{
    State(const string& path) :
        output_path(path)
    {
        boost::filesystem::create_directories(path);
    }

    string output_path;
};

RawConsumer::RawConsumer()
{
    // nop
}

RawConsumer::RawConsumer(const string& output_path) :
    m_state(boost::make_shared<State>(output_path))
{
  // nop
}

bool RawConsumer::operator()(const Input::input_p& input)
{
    assert(m_state);

    RawConsumerDelegate delegate(m_state->output_path);
    delegate.begin_input(input->id);
    input->connection.dispatch(delegate);

    return true;
}

} // CLIPP
} // IronBee
