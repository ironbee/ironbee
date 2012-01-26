#include "input.hpp"
#include "audit_log_generator.hpp"
#include "raw_generator.hpp"
#include "ironbee.hpp"

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include <string>

using namespace std;
using namespace IronBee::CLI;

using IronBee::input_t;
using IronBee::input_generator_t;
using IronBee::buffer_t;

//! A producer of input generators.
using input_factory_t     = boost::function<input_generator_t(const string&)>;

//! A map of command line argument to factory.
using input_factory_map_t = map<string,input_factory_t>;

input_generator_t init_audit_input( const string& arg );
input_generator_t init_raw_input(   const string& arg );

bool on_error( const string& message );

int main( int argc, char** argv )
{
  namespace po = boost::program_options;

  bool   show_help = false;
  string config_path;

  po::options_description desc(
    "All input options can be repeated.  Inputs will be processed in the "
    "order listed."
  );

  po::options_description general_desc( "General:" );
  general_desc.add_options()
    ( "help", po::bool_switch( &show_help ), "Output help message." )
    ( "config,C", po::value<string>( &config_path ),
      "IronBee config file.  REQUIRED"
    )
    ;

  po::options_description input_desc( "Input Options:" );
  input_desc.add_options()
    ( "audit,A", po::value<vector<string>>(),
      "Mod Security Audit Log"
    )
    ( "raw,R", po::value<vector<string>>(),
      "Raw inputs.  Use comma separated pair: request path,response path.  "
      "Raw input will use bogus connection information."
    )
    ;
  desc.add( general_desc ).add( input_desc );

  auto options = po::parse_command_line( argc, argv, desc );

  po::variables_map vm;
  po::store( options, vm );
  po::notify( vm );

  if ( show_help ) {
    cerr << desc << endl;
    return 1;
  }

  if ( config_path.empty() ) {
    cerr << "Config required." << endl;
    cout << desc << endl;
    return 1;
  }

  // Declare input types.
  input_factory_map_t input_factory_map;
  input_factory_map["audit"] = &init_audit_input;
  input_factory_map["raw"]   = &init_raw_input;

  // Initialize IronBee.
  IronBee::IronBee ib;
  ib.load_config( config_path );

  // We loop through the options, generating and processing input generators
  // as needed to limit the scope of each input generator.  As input
  // generators can make use of significant memory, it is good to only have
  // one around at a time.
  for ( const auto& option : options.options ) {
    input_generator_t generator;
    try {
      auto i = input_factory_map.find( option.string_key );
      if ( i != input_factory_map.end() ) {
        generator = i->second( option.value[0] );
      } else {
        continue;
      }
    }
    catch ( const exception& e ) {
      cerr << "Error initializing "
           << option.string_key << " " << option.value[0] << ".  "
           << "Message = " << e.what()
           << endl;
      return 1;
    }

    // Process inputs.
    input_t input;
    while ( generator( input ) ) {
      cout << "Found input: " << input << endl;
    }
  }

  return 0;
}

input_generator_t init_audit_input( const string& str )
{
  return AuditLogGenerator( str, on_error );
}

input_generator_t init_raw_input( const string& arg )
{
  auto comma_i = arg.find_first_of( ',' );
  if ( comma_i == string::npos ) {
    throw runtime_error( "Raw inputs must be _request_,_response_." );
  }

  return RawGenerator(
    arg.substr( 0, comma_i ),
    arg.substr( comma_i+1 )
  );
}

bool on_error( const string& message )
{
  cerr << "ERROR: " << message << endl;
  return true;
}
