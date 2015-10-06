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
 * @brief IronBee --- Eudoxus Benchmarker
 *
 * A command line executor for Eudoxus.  Runs automata against inputs and
 * records output and timing information.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironautomata/eudoxus.h>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#if __has_warning("-Wunused-local-typedef")
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
#endif
#include <boost/bind.hpp>
#include <boost/chrono.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>
#include <boost/scoped_ptr.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <fstream>

using namespace std;

/**
 * Handle timing across an event stream.
 *
 * This class knows about three different types of events and records the
 * amount of time spent in each.
 */
class TimingInfo
{
public:
    //! Clock we are using.
    typedef boost::chrono::high_resolution_clock clock_t;
    //! Milliseconds as double.
    typedef boost::chrono::duration<double, boost::milli> ms_t;

    //! Event types.
    enum event_e {
        //! Default event.  I.e., anything not below.
        DEFAULT,
        //! Eudoxus.  Active while in Eudoxus code.
        EUDOXUS,
        //! Output.  Active while in ee output code.
        OUTPUT,
        //! Number of event types.
        NUM
    };

    /**
     * Constructor.
     *
     * Starts with all durations at 0 and in DEFAULT event.
     */
    TimingInfo() :
        m_durations(NUM)
    {
        m_last_event = clock_t::now();
        m_event_type = DEFAULT;
    }


    /**
     * Change which event is being timed.
     *
     * @param[in] et New event type.
     */
    void switch_event(event_e et)
    {
        update();
        m_event_type = et;
    }

    /**
     * Force an update.
     *
     * Ensures that durations are updated to current time.
     */
    void update()
    {
        clock_t::time_point now = clock_t::now();
        clock_t::duration since = now - m_last_event;
        m_durations[m_event_type] += since;
        m_last_event = clock_t::now();
    }

    /**
     * Amount of time elapsed for event @a et.
     *
     * @param[in] et Event type to report for.
     * @return Elapsed time for @a et in native duration.
     */
    clock_t::duration elapsed(event_e et = DEFAULT)
    {
        update();
        return m_durations[et];
    }

    //! As above, but reports ms_t.
    ms_t elapsed_ms(event_e et = DEFAULT)
    {
        return ms_t(elapsed(et));
    }

private:
    clock_t::time_point m_last_event;
    event_e m_event_type;
    vector<clock_t::duration> m_durations;
};

//! Type of an output transformation.
typedef boost::function<
    string(
        const char*,
        size_t,
        const uint8_t*
    )
> output_transform_t;

//! Type of an output callback.
typedef boost::function<
    void(const string& s, const uint8_t* input)
> output_callback_t;

/**
 * Handle outputs.
 *
 * When combined with c_output_callback, handles Eudoxus callbacks.  It uses
 * the specified output transformation to transform the output and then passes
 * it to the callback.  Along the way, it ensures that @a timing_info is
 * recording properly.
 */
class OutputHandler
{
public:
    /**
     * Constructor.
     *
     * @param[in] timing_info TimingInfo to keep informed.
     * @param[in] transform   Functional to transform output.
     * @param[in] callback    Functional to do something with the output.
     */
    OutputHandler(
        TimingInfo& timing_info,
        output_transform_t transform,
        output_callback_t callback
    ) :
        m_timing_info(timing_info),
        m_transform(transform),
        m_callback(callback)
    {
        // nop
    }

    //! Call operator.  See ia_eudoxus_callback_t.
    void operator()(
        const char* output,
        size_t length,
        const uint8_t* input
    ) const
    {
        m_timing_info.switch_event(TimingInfo::OUTPUT);
        m_callback(m_transform(output, length, input), input);
        m_timing_info.switch_event(TimingInfo::EUDOXUS);
    }

private:
    TimingInfo& m_timing_info;
    output_transform_t m_transform;
    output_callback_t m_callback;
};

//! Count of outputs.  Used by output_record_count.
typedef map<string, size_t> output_record_map_t;

extern "C" {

//! Eudoxus callback.  Just forwards to OutputHandler.
ia_eudoxus_command_t c_output_callback(
    ia_eudoxus_t*, // unused
    const char* output,
    size_t output_length,
    const uint8_t* input,
    void* data
)
{
    OutputHandler* handler = reinterpret_cast<OutputHandler*>(data);
    (*handler)(output, output_length, input);

    return IA_EUDOXUS_CMD_CONTINUE;
}

}

//! Transform output into a string directly.
string output_transform_string(
    const char* output,
    size_t output_length,
    const uint8_t*
)
{
    return string(output, output_length);
}

/**
 * Interpret output as uint32_t length and pull from input.
 *
 * Treats the length as the number of previous bytes in input to use as
 * output.
 *
 * @param[in] output        Interpreted as pointer to uint32_t.
 * @param[in] output_length Hopefully 4.
 * @param[in] input         Current input location.
 * @return Previous @a output bytes of @a input.
 */
string output_transform_length(
    const char* output,
    size_t output_length,
    const uint8_t* input
)
{
    assert(output_length == 4);
    if (output_length != 4) {
        BOOST_THROW_EXCEPTION(
            std::invalid_argument("output_length must be 4")
        );
    }

    uint32_t x = *reinterpret_cast<const uint32_t*>(output);
    return string(input - x, input);
}

//! Interpret output as uint32_t and transform directly to string.
string output_transform_integer(
    const char* output,
    size_t output_length,
    const uint8_t*
)
{
    assert(output_length == 4);
    if (output_length != 4) {
        BOOST_THROW_EXCEPTION(
            std::invalid_argument("output_length must be 4")
        );
    }

    uint32_t x = *reinterpret_cast<const uint32_t*>(output);
    return boost::lexical_cast<string>(x);
}

//! Return empty string.
string output_transform_nop(const char*, size_t, const uint8_t*)
{
    return string();
}

//! Write output to @a out without location.
void output_record_raw(
    const string& s,
    const uint8_t*,
    const uint8_t*,
    size_t,
    ostream& out
)
{
    out << s << endl;
}

//! Write output to @a out.
void output_record_list(
    const string& s,
    const uint8_t* input,
    const uint8_t* block_start,
    size_t pre_block,
    ostream& out
)
{
    out << boost::format("%8d: %s\n") % (pre_block + input - block_start) % s;
}

//! Increment a map of outputs to counts.
void output_record_count(
    const string& s,
    output_record_map_t& counts
)
{
    output_record_map_t::iterator i = counts.insert(make_pair(s, 0)).first;
    ++i->second;
}

//! Do nothing.
void output_record_nop(const string&, const uint8_t*)
{
    // nop
}

/**
 * Output a eudoxus result code with error message to cout.
 *
 * @param[in] eudoxus Eudoxus.
 * @param[in] rc      Result code to output.
 */
void output_eudoxus_result(const ia_eudoxus_t* eudoxus, ia_eudoxus_result_t rc)
{
    const char* rc_message = NULL;

    switch (rc) {
        case IA_EUDOXUS_OK: rc_message = "OK"; break;
        case IA_EUDOXUS_STOP: rc_message = "STOP"; break;
        case IA_EUDOXUS_ERROR: rc_message = "ERROR"; break;
        case IA_EUDOXUS_END: rc_message = "END"; break;
        case IA_EUDOXUS_EINVAL: rc_message = "EINVAL"; break;
        case IA_EUDOXUS_EALLOC: rc_message = "EALLOC"; break;
        case IA_EUDOXUS_EINCOMPAT: rc_message = "EINCOMPAT"; break;
        case IA_EUDOXUS_EINSANE: rc_message = "EINSANE"; break;
        default:
            rc_message = "Unknown!";
    }

    const char* message = NULL;
    if (eudoxus) {
        message = ia_eudoxus_error(eudoxus);
    }
    if (! message) {
        message = "No message.";
    }

    cout << "Eudoxus Reported " << rc_message << ": " << message << endl;
}

//! Main.
int main(int argc, char **argv)
{
    namespace po = boost::program_options;

    const static string c_output_type_key("Output-Type");

    string output_s;
    string input_s;
    string automata_s;
    string output_type_s("auto");
    string record_s("list");
    size_t block_size = 1024;
    size_t overlap_size = 128;
    bool no_output = false;
    bool final = false;
    bool list_output = false;
    size_t n = 1;

    po::options_description desc("Options:");
    desc.add_options()
        ("help", "display help and exit")
        ("output,o", po::value<string>(&output_s),
            "where to write output, defaults to STDOUT"
        )
        ("input,i", po::value<string>(&input_s),
            "where to read input from, defaults to STDIN"
        )
        ("automata,a", po::value<string>(&automata_s),
            "where to read automata from; required, but -a is optional"
        )
        ("type,t", po::value<string>(&output_type_s),
            "output type: auto, string, length, integer, nop; default is auto"
        )
        ("record,r", po::value<string>(&record_s),
            "output record: list, count, nop; default is list"
        )
        ("size,s", po::value<size_t>(&block_size),
            "input block size; default = 1024"
        )
        ("overlap,l", po::value<size_t>(&overlap_size),
            "how much to overlap blocks; default = 128"
        )
        ("final,f", po::bool_switch(&final),
            "only output for final node"
        )
        ("num-runs,n", po::value<size_t>(&n),
            "number of times to run input through; 0 = infinite, default = 1"
        )
        ("list-output,L", po::bool_switch(&list_output),
            "list all outputs of automata and exit"
        )
        ;

    po::positional_options_description pd;
    pd.add("automata", 1);

    po::variables_map vm;
    po::store(
        po::command_line_parser(argc, argv)
            .options(desc)
            .positional(pd)
            .run(),
        vm
    );
    po::notify(vm);

    if (vm.count("help")) {
        cout << desc << endl;
        return 1;
    }

    if (! vm.count("automata")) {
        cout << "automata is required." << endl;
        cout << desc << endl;
        return 1;
    }

    if (overlap_size > block_size / 2) {
        cout << "block_size must be at least twice overlap size." << endl;
        return 1;
    }

    try {
        // for memory management only
        boost::scoped_ptr<istream> input_mem;
        boost::scoped_ptr<ostream> output_mem;

        ostream* output   = &cout;

        if (! output_s.empty()) {
            output_mem.reset(new ofstream(output_s.c_str()));
            output = output_mem.get();
            if (! *output) {
                cout << "Error: Could not open " << output_s << " for writing."
                     << endl;
                return 1;
            }
        }

        // Load automata
        ia_eudoxus_result_t rc;
        ia_eudoxus_t* eudoxus;

        TimingInfo ti;
        rc = ia_eudoxus_create_from_path(&eudoxus, automata_s.c_str());
        if (rc != IA_EUDOXUS_OK) {
            output_eudoxus_result(NULL, rc);
            return 1;
        }
        cout << "Loaded automata in " << ti.elapsed_ms() << endl;

        // Figure out output.
        output_transform_t output_transform;
        if (output_type_s == "auto") {
            const uint8_t* value = NULL;
            size_t length = 0;
            rc = ia_eudoxus_metadata_with_key(
                eudoxus,
                reinterpret_cast<const uint8_t*>(c_output_type_key.data()),
                c_output_type_key.length(),
                &value, &length
            );
            if (rc == IA_EUDOXUS_END) {
                cerr << "Error: Automata does not contain "
                     << c_output_type_key << ".  "
                     << "Must specify explicitly with --type."
                     << endl;
                return 1;
            }
            if (rc != IA_EUDOXUS_OK) {
                cerr << "Error: Could not read metadata." << endl;
                return 1;
            }
            output_type_s = string(reinterpret_cast<const char*>(value), length);
            cout << "Read Output-Type of " << output_type_s << endl;
        }

        if (output_type_s == "string") {
            output_transform = output_transform_string;
        }
        else if (output_type_s == "length") {
            output_transform = output_transform_length;
        }
        else if (output_type_s == "integer") {
            output_transform = output_transform_integer;
        }
        else if (output_type_s == "nop") {
            no_output = true;
            output_transform = output_transform_nop;
        }
        else {
            cout << "Error: Unknown output type: " << output_type_s << endl;
            return 1;
        }

        // Construct callback.
        vector<uint8_t> input_buffer(block_size);
        size_t pre_block = 0;
        output_record_map_t counts;
        output_callback_t output_callback;
        if (list_output) {
            output_callback = boost::bind(
                output_record_raw,
                _1,
                _2,
                static_cast<const unsigned char*>(NULL),
                0,
                boost::ref(*output)
            );
        }
        else if (record_s == "list") {
            output_callback = boost::bind(
                output_record_list,
                _1,
                _2,
                &input_buffer[overlap_size],
                boost::ref(pre_block),
                boost::ref(*output)
            );
        }
        else if (record_s == "count") {
            output_callback = boost::bind(
                output_record_count,
                _1,
                boost::ref(counts)
            );
        }
        else if (record_s == "nop") {
            no_output = true;
            output_callback = output_record_nop;
        }
        else {
            cout << "Error: Unknown output record " << record_s << endl;
            return 1;
        }
        OutputHandler output_handler(ti, output_transform, output_callback);

        // Check for -L
        if (list_output) {
            rc = ia_eudoxus_all_outputs(
                eudoxus,
                c_output_callback,
                reinterpret_cast<void*>(&output_handler)
            );
            if (rc != IA_EUDOXUS_OK) {
                output_eudoxus_result(eudoxus, rc);
                return 1;
            }
            return 0;
        }

        // Run Engine
        for (size_t i = 0; i < n || n == 0; ++i) {
            ia_eudoxus_state_t* state;
            rc = ia_eudoxus_create_state(
                &state,
                eudoxus,
                c_output_callback,
                reinterpret_cast<void*>(&output_handler)
            );
            if (rc != IA_EUDOXUS_OK) {
                output_eudoxus_result(eudoxus, rc);
                return 1;
            }

            bool at_end = false;
            pre_block = 0;

            istream* input = &cin;
            if (! input_s.empty()) {
                input_mem.reset(new ifstream(input_s.c_str()));
                input = input_mem.get();
                if (! *input) {
                    cout << "Error: Could not open " << input_s << " for reading."
                         << endl;
                    return 1;
                }
            }

            while (! at_end && *input) {
                TimingInfo local_ti;
                // Shift overlap to front.
                copy(
                    &input_buffer[block_size - overlap_size], &input_buffer[block_size],
                    &input_buffer[0]
                );
                input->read(
                    reinterpret_cast<char*>(&input_buffer[overlap_size]),
                    block_size - overlap_size
                );

                size_t read = input->gcount();
                if (read == 0) {
                    break;
                }
                ti.switch_event(TimingInfo::EUDOXUS);
                if (no_output || final) {
                    rc = ia_eudoxus_execute_without_output(
                        state,
                        &input_buffer[overlap_size],
                        read
                    );
                }
                else {
                    rc = ia_eudoxus_execute(
                        state,
                        &input_buffer[overlap_size],
                        read
                    );
                }
                ti.switch_event(TimingInfo::DEFAULT);
                switch (rc) {
                case IA_EUDOXUS_OK:
                    // nop
                    break;
                case IA_EUDOXUS_END:
                    cout << "Reached end of automata." << endl;
                    at_end = true;
                    break;
                default:
                    output_eudoxus_result(eudoxus, rc);
                    return 1;
                }

                pre_block += read;
            }
            if (final) {
                ia_eudoxus_execute(state, NULL, 0);
            }
            ia_eudoxus_destroy_state(state);
        }

        // If counts, report output.
        if (record_s == "count") {
            BOOST_FOREACH(const output_record_map_t::value_type& v, counts) {
                cout << boost::format("%20s %d\n") % v.first % v.second;
            }
        }

        // Report timing.
        cout << "Timing: eudoxus="
             << ti.elapsed_ms(TimingInfo::EUDOXUS)
             << " output=" << ti.elapsed_ms(TimingInfo::OUTPUT)
             << endl;

        ia_eudoxus_destroy(eudoxus);
    }
    catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }

    return 0;
}
