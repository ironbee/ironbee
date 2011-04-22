require 'mkmf'

dir_config( 'htp' )
have_library( 'htp', 'htp_connp_create' ) || abort( "Can't find HTP library." )
have_header( 'htp/htp.h' ) || abort( "Can't find htp.h" )
create_makefile( 'htp' )
