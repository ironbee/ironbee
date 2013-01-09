# Copyright (c) 2009-2010 Open Information Security Foundation
# Copyright (c) 2010-2013 Qualys, Inc.
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
# 
# - Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.

# - Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.

# - Neither the name of the Qualys, Inc. nor the names of its
#   contributors may be used to endorse or promote products derived from
#   this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Author: Christopher Alfeld <calfeld@qualys.com>

module HTP
  # TODO: Lots to do.  Good inspect for all classes would be a good start.
  # As would an easier parsing interface that takes care of the return codes.
  
  class Cfg
    # Object.dup will just create a Config that points to the same underlying
    # htp_cfg_t.  By using #copy which maps to htp_config_copy, we can do
    # the expected dup behavior.
    alias :dup :copy

    SERVER_PERSONALITY_ASSOC = [
      [ :minimal, HTP_SERVER_MINIMAL ],
      [ :generic, HTP_SERVER_GENERIC ],
      [ :ids, HTP_SERVER_IDS ],
      [ :iis_4_0, HTP_SERVER_IIS_4_0 ],
      [ :iis_5_0, HTP_SERVER_IIS_5_0 ],
      [ :iis_5_1, HTP_SERVER_IIS_5_1 ],
      [ :iis_6_0, HTP_SERVER_IIS_6_0 ],
      [ :iis_7_0, HTP_SERVER_IIS_7_0 ],
      [ :iis_7_5, HTP_SERVER_IIS_7_5 ],
      [ :tomcat_6_0, HTP_SERVER_TOMCAT_6_0 ],
      [ :apache, HTP_SERVER_APACHE ],
      [ :apache_2_2, HTP_SERVER_APACHE_2_2 ]
    ].freeze
    
    def server_personality
      personality_id = spersonality
      personality = SERVER_PERSONALITY_ASSOC.rassoc( personality_id )[0]
      personality.nil? ? personality_id : personality
    end
    def server_personality=( personality )
      if personality.is_a?( String )
        personality = personality.to_sym
      end
      if personality.is_a?( Symbol )
        personality_id = SERVER_PERSONALITY_ASSOC.assoc( personality )[1]
        if personality_id.nil?
          raise TypeError.new( "Unknown personality: #{personality}" )
        end
        personality = personality_id
      end
      if ! personality.is_a?( Fixnum )
        raise TypeError.new( "Can't understand personality." ) 
      end
      set_server_personality( personality )
    end 
  end
    
  class Connp
    attr_reader :cfg
  end
  
  class Header
    def invalid?
      flags & HTP_FIELD_INVALID != 0
    end
    
    def folded?
      flags & HTP_FIELD_FOLDED != 0
    end
    
    def repeated?
      flags & HTP_FIELD_REPEATED != 0
    end
    
    def to_s
      r = "#{name}: #{value}"
      r += " <INVALID>" if invalid?
      r += " <FOLDER>" if folded?
      r += " <REPEATED>" if repeated?
      r
    end
    
    alias :inspect :to_s
    alias :to_str :to_s
  end
  
  class HeaderLine
    def invalid?
      flags & HTP_FIELD_INVALID != 0
    end
    
    def long?
      flags & HTP_FIELD_LONG != 0
    end
    
    def nul_byte?
      flags & HTP_FIELD_NUL_BYTE != 0
    end
    
    def to_s
      line
    end
    
    alias :inspect :to_s
    alias :to_str :to_s
  end
  
  class URI
    def to_s
      if hostname
        "http://" +
        ( username ? username : '' ) +
        ( password ? ":#{password}" : '' ) +
        ( hostname && ( username || password ) ? '@' : '' ) +
        ( hostname ? "#{hostname}:#{port}" : '' )
      else
        ''
      end + 
      ( path ? path : '' ) +
      ( query ? "?#{query}" : '' ) +
      ( fragment ? "##{fragment}" : '' )
    end
    
    alias :inspect :to_s
    alias :to_str :to_s
  end
  
  class Tx
    attr_reader :connp
    attr_reader :cfg
    
    # Here we cache a variety of values that are built on demand.
    [
      :request_params_query,
      :request_params_body,
      :request_cookies,
      :request_headers,
      :response_headers,
      :request_header_lines,
      :response_header_lines
    ].each do |name|
      raw_name = ( "_" + name.to_s ).to_sym
      alias_method( raw_name, name )
      private( raw_name )
      remove_method( name )
      define_method name do
        @cache ||= {}
        @cache[name] ||= send( raw_name )
      end
    end

    def invalid_chunking?
      flags & HTP_INVALID_CHUNKING != 0
    end
    
    def invalid_folding?
      flags & HTP_INVALID_FOLDING != 0
    end
    
    def request_smuggling?
      flags & HTP_REQUEST_SMUGGLING != 0
    end
    
    def multi_packet_header?
      flags & HTP_MULTI_PACKET_HEAD != 0
    end
    
    def field_unparseable?
      flags & HTP_FIELD_UNPARSABLE != 0
    end
    
    def request_params_as_hash
      if ! @request_params
        @request_params = Hash.new {|h,k| h[k] = []}
        [ request_params_query, request_params_body ].compact.each do |result|
          result.each do |k,v|
            @request_params[k] << v
          end
        end
      end
      @request_params
    end
    
    def request_cookies_as_hash
      if ! @request_cookies
        @request_cookies = Hash.new {|h,k| h[k] = []}
        result = request_cookies
        if result
          result.each do |k,v|
            @request_cookies[k] << v
          end
        end
      end
      @request_cookies
    end
    
    alias :to_s :request_line
    alias :to_str :to_s
    alias :inspect :to_s
  end
  
  class File
    alias :to_s :filename
    alias :inspect :to_s
    alias :to_str :to_s
  end 
  
  class Conn
    attr_reader :connp
    
    def pipelined_connection?
      flags & PIPELINED_CONNECTION
    end
    
    def to_s
      ( local_addr || "???" ) + ":#{local_port} -> " +
      ( remote_addr || "???" ) + ":#{remote_port}"
    end
    
    alias :to_str :to_s
    alias :inspect :to_s
  end
end