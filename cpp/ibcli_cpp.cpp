#include "input.hpp"

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include <string>

using namespace std;
using namespace IronBee::CLI;

//! A producer of input generators.
using input_factory_t     = boost::function<input_generator_t(const string&)>;

//! A map of command line argument to factory.
using input_factory_map_t = map<string,input_factory_t>;

input_generator_t init_audit_input( const string& arg );
input_generator_t init_raw_input(   const string& arg );

int main( int argc, char** argv )
{
  namespace po = boost::program_options;

  bool show_help = false;

  po::options_description desc(
    "All input options can be repeated.  Inputs will be processed in the"
    "order listed."
  );
  po::options_description general( "General:" );
  general.add_options()
    ( "help", po::bool_switch( &show_help ), "Output help message." )
    ;
  
  po::options_description input( "Input Options:" );
  input.add_options()
    ( "audit,A", po::value<string>(),
      "Mod Security Audit Log"
    )
    ( "raw,R", po::value<string>(),
      "Raw inputs.  Use comma separated pair: request path,response path.  "
      "Raw input will use bogus connection information."
    )
    ;
  desc.add( general ).add( input );
  
  po::variables_map vm;
  auto options = po::parse_command_line( argc, argv, desc );

  if ( show_help ) {
    cerr << desc << endl;
    return 1;
  }

  // Declare input types.
  input_factory_map_t input_factory_map;
  input_factory_map["audit"] = &init_audit_input;
  input_factory_map["raw"]   = &init_raw_input;

  // Set up inputs.
  using input_generator_vec_t = vector<input_generator_t>;
  input_generator_vec_t inputs;
  bool                  have_errors = false;

  for ( const auto& option : options.options ) {
    try {
      auto i = input_factory_map.find( option.string_key );
      if ( i != input_factory_map.end() ) {
        inputs.push_back( i->second( option.value[0] ) );
      }
    }
    catch ( const exception& e ) {
      cerr << "Error initializing "
           << option.string_key << " " << option.value[0] << ".  "
           << "Message = " << e.what()
           << endl;
      have_errors = true;
    }
  }
  if ( have_errors ) {
    return 1;
  }

  if ( inputs.empty() ) {
    cerr << "Need at least one input." << endl;
    cerr << desc << endl;
    return 1;
  }

  // XXX Do something.
  
  return 0;
}

input_generator_t init_audit_input( const string& )
{
  // XXX
  return input_generator_t();
}

input_generator_t init_raw_input( const string& arg )
{
  auto comma_i = arg.find_first_of( ',' );
  if ( comma_i == string::npos ) {
    throw runtime_error( "Raw inputs must be _request_,_response_." );
  }

  // XXX
  return input_generator_t();
}
