#!/usr/bin/env ruby

$:.unshift(File.dirname(__FILE__))
require 'all-code'
require 'set'

EXCEPTION_INCLUDES = Set.new [
  '<ironautomata/eudoxus_subautomata.h>',
  '"eudoxus_subengine.c"'
]

CANONICAL_INCLUDE_ORDER = [
  '"ironbee_config_auto.h"',

  :self,
  :private,

  '"config-parser.h"',

  # Dirt hack section
  '"user_agent_private.h"',
  '<ironbee/module_sym.h>',

  '<ironbeepp/internal/catch.hpp>',
  '<ironbeepp/internal/data.hpp>',
  '<ironbeepp/internal/throw.hpp>',

  '<ironbeepp/abi_compatibility.hpp>',
  '<ironbeepp/byte_string.hpp>',
  '<ironbeepp/clock.hpp>',
  '<ironbeepp/common_semantics.hpp>',
  '<ironbeepp/configuration_directives.hpp>',
  '<ironbeepp/configuration_map.hpp>',
  '<ironbeepp/configuration_parser.hpp>',
  '<ironbeepp/connection.hpp>',
  '<ironbeepp/connection_data.hpp>',
  '<ironbeepp/context.hpp>',
  '<ironbeepp/engine.hpp>',
  '<ironbeepp/exception.hpp>',
  '<ironbeepp/field.hpp>',
  '<ironbeepp/hooks.hpp>',
  '<ironbeepp/ironbee.hpp>',
  '<ironbeepp/list.hpp>',
  '<ironbeepp/memory_pool.hpp>',
  '<ironbeepp/module.hpp>',
  '<ironbeepp/module_bootstrap.hpp>',
  '<ironbeepp/module_delegate.hpp>',
  '<ironbeepp/notifier.hpp>',
  '<ironbeepp/parsed_name_value.hpp>',
  '<ironbeepp/parsed_request_line.hpp>',
  '<ironbeepp/parsed_response_line.hpp>',
  '<ironbeepp/server.hpp>',
  '<ironbeepp/site.hpp>',
  '<ironbeepp/transaction.hpp>',
  '<ironbeepp/transaction_data.hpp>',

  '"ahocorasick_private.h"',
  '"core_audit_private.h"',
  '"core_private.h"',
  '"engine_private.h"',
  '"ironbee_private.h"',
  '"rule_engine_private.h"',
  '"rules_lua_private.h"',
  '"state_notify_private.h"',
  '"user_agent_private.h"',
  '"lua/ironbee.h"',

  # Automata
  '<ironautomata/bits.h>',
  '<ironautomata/eudoxus.h>',
  '<ironautomata/eudoxus_automata.h>',
  '<ironautomata/vls.h>',

  '<ironautomata/buffer.hpp>',
  '<ironautomata/deduplicate_outputs.hpp>',
  '<ironautomata/eudoxus_compiler.hpp>',
  '<ironautomata/intermediate.hpp>',
  '<ironautomata/intermediate.pb.h>',
  '<ironautomata/logger.hpp>',
  '<ironautomata/optimize_edges.hpp>',
  '<ironautomata/translate_nonadvancing.hpp>',
  # End Automata

  '<ironbee/action.h>',
  '<ironbee/ahocorasick.h>',
  '<ironbee/array.h>',
  '<ironbee/build.h>',
  '<ironbee/bytestr.h>',
  '<ironbee/cfgmap.h>',
  '<ironbee/clock.h>',
  '<ironbee/config.h>',
  '<ironbee/core.h>',
  '<ironbee/debug.h>',
  '<ironbee/decode.h>',
  '<ironbee/dso.h>',
  '<ironbee/engine.h>',
  '<ironbee/engine_types.h>',
  '<ironbee/escape.h>',
  '<ironbee/expand.h>',
  '<ironbee/field.h>',
  '<ironbee/hash.h>',
  '<ironbee/ip.h>',
  '<ironbee/ipset.h>',
  '<ironbee/list.h>',
  '<ironbee/lock.h>',
  '<ironbee/logformat.h>',
  '<ironbee/module.h>',
  '<ironbee/module_sym.h>',
  '<ironbee/mpool.h>',
  '<ironbee/operator.h>',
  '<ironbee/path.h>',
  '<ironbee/ipset.h>',
  '<ironbee/parsed_content.h>',
  '<ironbee/provider.h>',
  '<ironbee/regex.h>',
  '<ironbee/release.h>',
  '<ironbee/rule_defs.h>',
  '<ironbee/rule_engine.h>',
  '<ironbee/server.h>',
  '<ironbee/state_notify.h>',
  '<ironbee/string.h>',
  '<ironbee/stream.h>',
  '<ironbee/transformation.h>',
  '<ironbee/types.h>',
  '<ironbee/util.h>',
  '<ironbee/uuid.h>',

  '<boost/any.hpp>',
  '<boost/bind.hpp>',
  '<boost/chrono.hpp>',
  '<boost/date_time/posix_time/posix_time.hpp>',
  '<boost/date_time/posix_time/ptime.hpp>',
  '<boost/exception/all.hpp>',
  '<boost/filesystem.hpp>',
  '<boost/filesystem/fstream.hpp>',
  '<boost/foreach.hpp>',
  '<boost/format.hpp>',
  '<boost/function.hpp>',
  '<boost/iterator/iterator_facade.hpp>',
  '<boost/lexical_cast.hpp>',
  '<boost/make_shared.hpp>',
  '<boost/mpl/or.hpp>',
  '<boost/noncopyable.hpp>',
  '<boost/operators.hpp>',
  '<boost/program_options.hpp>',
  '<boost/scoped_array.hpp>',
  '<boost/scoped_ptr.hpp>',
  '<boost/shared_ptr.hpp>',
  '<boost/static_assert.hpp>',
  '<boost/type_traits/is_class.hpp>',
  '<boost/type_traits/is_convertible.hpp>',
  '<boost/type_traits/is_same.hpp>',
  '<boost/type_traits/is_signed.hpp>',
  '<boost/type_traits/is_unsigned.hpp>',
  '<boost/type_traits/remove_const.hpp>',
  '<boost/utility.hpp>',
  '<boost/utility/enable_if.hpp>',
  '<boost/uuid/uuid.hpp>',

  '<google/protobuf/io/gzip_stream.h>',
  '<google/protobuf/io/zero_copy_stream_impl_lite.h>',

  '<dslib.h>',
  '<GeoIP.h>',
  '<htp.h>',
  '<lauxlib.h>',
  '<lua.h>',
  '<lualib.h>',
  '<pcre.h>',
  '<pcreposix.h>',
  '<pthread.h>',
  '<uuid.h>',
  '<valgrind/memcheck.h>',

  '<algorithm>',
  '<fstream>',
  '<iostream>',
  '<list>',
  '<map>',
  '<ostream>',
  '<queue>',
  '<set>',
  '<string>',
  '<vector>',

  '<cassert>',

  '<assert.h>',
  '<ctype.h>',
  '<dlfcn.h>',
  '<errno.h>',
  '<fcntl.h>',
  '<glib.h>',
  '<getopt.h>',
  '<glob.h>',
  '<inttypes.h>',
  '<libgen.h>',
  '<limits.h>',
  '<math.h>',
  '<regex.h>',
  '<stdarg.h>',
  '<stdbool.h>',
  '<stddef.h>',
  '<stdint.h>',
  '<stdio.h>',
  '<stdlib.h>',
  '<string.h>',
  '<strings.h>',
  '<time.h>',
  '<unistd.h>',
  '<sys/ipc.h>',
  '<sys/sem.h>',
  '<sys/stat.h>',
  '<sys/time.h>',
  '<sys/types.h>',
  '<arpa/inet.h>',
  '<netinet/in.h>'
]

def extract_includes(path)
  result = []
  IO::foreach(path) do |line|
    line.strip!
    if line =~ /^#include (.+[">])$/
      result << $1
    end
  end
  result
end

all_ironbee_code do |path|
  next if path =~ /test/
  last_index = -1
  self_name = nil
  private_name = nil
  if path =~ /(\.c(pp)?)$/
    self_name = Regexp.new(
      "iron(bee|automata)/" + File.basename(path, $1) + '\.h' + ($2 || "")
    )
    private_name = Regexp.new(
      File.basename(path, $1) + '_private\.h' + ($2 || "")
    )
  end
  extract_includes(path).each do |i|
    next if EXCEPTION_INCLUDES.member?(i)
    index = nil
    if i =~ self_name
      index = CANONICAL_INCLUDE_ORDER.index(:self)
    elsif i =~ private_name
      index = CANONICAL_INCLUDE_ORDER.index(:private)
    end
    index ||= CANONICAL_INCLUDE_ORDER.index(i)

    if index.nil?
      puts "Unknown include in #{path}: #{i}"
    elsif index <= last_index
      puts "Include out of order in #{path}: #{i}"
    else
      last_index = index
    end
  end
end
