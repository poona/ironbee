#!/usr/bin/env ruby

$:.unshift(File.dirname(__FILE__))
require 'all-code'
require 'set'

EXCEPTION_INCLUDES = Set.new [
  '<ironautomata/eudoxus_subautomata.h>',
  '"eudoxus_subengine.c"',
  '<boost/preprocessor/iteration/local.hpp>'
]

CANONICAL_INCLUDE_ORDER = [
  '"ironbee_config_auto.h"',

  :self,
  :private,

  '"config-parser.h"',

  # Dirty hack section
  '"user_agent_private.h"',
  '<ironbee/module_sym.h>',

  # predicate
  '<predicate/bfs.hpp>',
  '<predicate/call_factory.hpp>',
  '<predicate/call_helpers.hpp>',
  '<predicate/dag.hpp>',
  '<predicate/dot.hpp>',
  '<predicate/ironbee.hpp>',
  '<predicate/less.hpp>',
  '<predicate/leaves.hpp>',
  '<predicate/merge_graph.hpp>',
  '<predicate/meta_call.hpp>',
  '<predicate/parse.hpp>',
  '<predicate/pre_eval_graph.hpp>',
  '<predicate/reporter.hpp>',
  '<predicate/standard.hpp>',
  '<predicate/standard_boolean.hpp>',
  '<predicate/standard_development.hpp>',
  '<predicate/standard_filter.hpp>',
  '<predicate/standard_ironbee.hpp>',
  '<predicate/standard_predicate.hpp>',
  '<predicate/standard_template.hpp>',
  '<predicate/standard_valuelist.hpp>',
  '<predicate/transform_graph.hpp>',
  '<predicate/tree_copy.hpp>',
  '<predicate/validate.hpp>',
  '<predicate/validate_graph.hpp>',

  '<ironbeepp/abi_compatibility.hpp>',
  '<ironbeepp/all.hpp>',
  '<ironbeepp/byte_string.hpp>',
  '<ironbeepp/catch.hpp>',
  '<ironbeepp/clock.hpp>',
  '<ironbeepp/common_semantics.hpp>',
  '<ironbeepp/configuration_directives.hpp>',
  '<ironbeepp/configuration_map.hpp>',
  '<ironbeepp/configuration_parser.hpp>',
  '<ironbeepp/connection.hpp>',
  '<ironbeepp/connection_data.hpp>',
  '<ironbeepp/context.hpp>',
  '<ironbeepp/c_trampoline.hpp>',
  '<ironbeepp/data.hpp>',
  '<ironbeepp/engine.hpp>',
  '<ironbeepp/exception.hpp>',
  '<ironbeepp/field.hpp>',
  '<ironbeepp/hash.hpp>',
  '<ironbeepp/hooks.hpp>',
  '<ironbeepp/ironbee.hpp>',
  '<ironbeepp/list.hpp>',
  '<ironbeepp/memory_pool.hpp>',
  '<ironbeepp/module.hpp>',
  '<ironbeepp/module_bootstrap.hpp>',
  '<ironbeepp/module_delegate.hpp>',
  '<ironbeepp/notifier.hpp>',
  '<ironbeepp/operator.hpp>',
  '<ironbeepp/parsed_name_value.hpp>',
  '<ironbeepp/parsed_request_line.hpp>',
  '<ironbeepp/parsed_response_line.hpp>',
  '<ironbeepp/server.hpp>',
  '<ironbeepp/site.hpp>',
  '<ironbeepp/test_fixture.hpp>',
  '<ironbeepp/throw.hpp>',
  '<ironbeepp/transaction.hpp>',
  '<ironbeepp/transaction_data.hpp>',
  '<ironbeepp/transformation.hpp>',
  '<ironbeepp/var.hpp>',

  '"collection_manager_private.h"',
  '"core_audit_private.h"',
  '"core_private.h"',
  '"engine_private.h"',
  '"engine_manager_log_private.h"',
  '"ironbee_private.h"',
  '"json_yajl_private.h"',
  '"kvstore_private.h"',
  '"lua/ironbee.h"',
  '"lua_common_private.h"',
  '"lua_modules_private.h"',
  '"lua_private.h"',
  '"lua_rules_private.h"',
  '"lua_runtime_private.h"',
  '"managed_collection_private.h"',
  '"moddevel_private.h"',
  '"parser_suite.hpp"',
  '"persistence_framework.h"',
  '"persistence_framework_private.h"',
  '"rule_engine_private.h"',
  '"rule_logger_private.h"',
  '"rules_lua_private.h"',
  '"state_notify_private.h"',
  '"user_agent_private.h"',

  # Automata
  '<ironautomata/bits.h>',
  '<ironautomata/eudoxus.h>',
  '<ironautomata/eudoxus_automata.h>',
  '<ironautomata/vls.h>',

  '<ironautomata/buffer.hpp>',
  '<ironautomata/deduplicate_outputs.hpp>',
  '<ironautomata/eudoxus_compiler.hpp>',
  '<ironautomata/generator/aho_corasick.hpp>',
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
  '<ironbee/capture.h>',
  '<ironbee/cfgmap.h>',
  '<ironbee/clock.h>',
  '<ironbee/collection_manager.h>',
  '<ironbee/config.h>',
  '<ironbee/context.h>',
  '<ironbee/context_selection.h>',
  '<ironbee/core.h>',
  '<ironbee/decode.h>',
  '<ironbee/dso.h>',
  '<ironbee/engine.h>',
  '<ironbee/engine_manager.h>',
  '<ironbee/engine_state.h>',
  '<ironbee/engine_types.h>',
  '<ironbee/escape.h>',
  '<ironbee/field.h>',
  '<ironbee/file.h>',
  '<ironbee/flags.h>',
  '<ironbee/hash.h>',
  '<ironbee/ip.h>',
  '<ironbee/ipset.h>',
  '<ironbee/json.h>',
  '<ironbee/kvstore.h>',
  '<ironbee/kvstore_filesystem.h>',
  '<ironbee/list.h>',
  '<ironbee/lock.h>',
  '<ironbee/log.h>',
  '<ironbee/logevent.h>',
  '<ironbee/logformat.h>',
  '<ironbee/logger.h>',
  '<ironbee/module.h>',
  '<ironbee/module_sym.h>',
  '<ironbee/mpool.h>',
  '<ironbee/operator.h>',
  '<ironbee/path.h>',
  '<ironbee/ipset.h>',
  '<ironbee/parsed_content.h>',
  '<ironbee/queue.h>',
  '<ironbee/regex.h>',
  '<ironbee/release.h>',
  '<ironbee/resource_pool.h>',
  '<ironbee/rule_defs.h>',
  '<ironbee/rule_engine.h>',
  '<ironbee/rule_logger.h>',
  '<ironbee/server.h>',
  '<ironbee/site.h>',
  '<ironbee/state_notify.h>',
  '<ironbee/string.h>',
  '<ironbee/string_assembly.h>',
  '<ironbee/strval.h>',
  '<ironbee/stream.h>',
  '<ironbee/transformation.h>',
  '<ironbee/types.h>',
  '<ironbee/util.h>',
  '<ironbee/uuid.h>',
  '<ironbee/var.h>',
  '<ironbee/vector.h>',

  '<boost/algorithm/string/classification.hpp>',
  '<boost/algorithm/string/join.hpp>',
  '<boost/algorithm/string/predicate.hpp>',
  '<boost/algorithm/string/split.hpp>',
  '<boost/any.hpp>',
  '<boost/assign.hpp>',
  '<boost/bind.hpp>',
  '<boost/bind/protect.hpp>',
  '<boost/chrono.hpp>',
  '<boost/date_time/gregorian/gregorian_types.hpp>',
  '<boost/date_time/local_time/local_time.hpp>',
  '<boost/date_time/posix_time/posix_time.hpp>',
  '<boost/date_time/posix_time/ptime.hpp>',
  '<boost/date_time/time_zone_base.hpp>',
  '<boost/enable_shared_from_this.hpp>',
  '<boost/exception/all.hpp>',
  '<boost/filesystem.hpp>',
  '<boost/filesystem/fstream.hpp>',
  '<boost/foreach.hpp>',
  '<boost/format.hpp>',
  '<boost/function.hpp>',
  '<boost/function_output_iterator.hpp>',
  '<boost/fusion/adapted.hpp>',
  '<boost/iterator/iterator_adaptor.hpp>',
  '<boost/iterator/iterator_facade.hpp>',
  '<boost/iterator/transform_iterator.hpp>',
  '<boost/lexical_cast.hpp>',
  '<boost/make_shared.hpp>',
  '<boost/mpl/or.hpp>',
  '<boost/noncopyable.hpp>',
  '<boost/operators.hpp>',
  '<boost/preprocessor/arithmetic/add.hpp>',
  '<boost/preprocessor/arithmetic/sub.hpp>',
  '<boost/preprocessor/cat.hpp>',
  '<boost/preprocessor/iteration/local.hpp>',
  '<boost/preprocessor/repetition.hpp>',
  '<boost/program_options.hpp>',
  '<boost/range/iterator_range.hpp>',
  '<boost/regex.hpp>',
  '<boost/scoped_array.hpp>',
  '<boost/scoped_ptr.hpp>',
  '<boost/shared_ptr.hpp>',
  '<boost/spirit/include/phoenix.hpp>',
  '<boost/spirit/include/qi.hpp>',
  '<boost/static_assert.hpp>',
  '<boost/thread.hpp>',
  '<boost/tuple/tuple.hpp>',
  '<boost/type_traits/is_class.hpp>',
  '<boost/type_traits/is_convertible.hpp>',
  '<boost/type_traits/is_const.hpp>',
  '<boost/type_traits/is_float.hpp>',
  '<boost/type_traits/is_pointer.hpp>',
  '<boost/type_traits/is_same.hpp>',
  '<boost/type_traits/is_signed.hpp>',
  '<boost/type_traits/is_unsigned.hpp>',
  '<boost/type_traits/remove_const.hpp>',
  '<boost/utility.hpp>',
  '<boost/utility/enable_if.hpp>',
  '<boost/uuid/uuid.hpp>',
  '<boost/version.hpp>',
  # Special case for engine_shutdown.cpp
  '<boost/atomic.hpp>',

  '<google/protobuf/io/gzip_stream.h>',
  '<google/protobuf/io/zero_copy_stream_impl_lite.h>',

  '<curl/curl.h>',
  '<dslib.h>',
  '<GeoIP.h>',
  '<htp.h>',
  '<htp_private.h>',
  '<lauxlib.h>',
  '<libinjection.h>',
  '<lua.h>',
  '<lualib.h>',
  '<modp_ascii.h>',
  '<pcre.h>',
  '<pcreposix.h>',
  '<pthread.h>',
  '<sqli_normalize.h>',
  '<sqli_fingerprints.h>',
  '<sqlparse.h>',
  '<sqlparse_private.h>',
  '<sqltfn.h>',
  '<uuid.h>',
  '<valgrind/memcheck.h>',
  '<yajl/yajl_gen.h>',
  '<yajl/yajl_parse.h>',
  '<yajl/yajl_tree.h>',

  '<algorithm>',
  '<fstream>',
  '<iostream>',
  '<list>',
  '<map>',
  '<ostream>',
  '<queue>',
  '<set>',
  '<sstream>',
  '<string>',
  '<stdexcept>',
  '<vector>',

  '<cassert>',

  '<assert.h>',
  '<ctype.h>',
  '<dirent.h>',
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
  '<signal.h>',
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
      "(ironbee|ironautomata|predicate)/(.+/)?" + File.basename(path, $1) + '\.h' + ($2 || "")
    )
    private_name = Regexp.new(
      '^"' + File.basename(path, $1) + '(_private)?\.h' + ($2 || "")
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
      puts "Include out of order in #{path}: #{i} (#{index} vs #{last_index})"
    else
      last_index = index
    end
  end
end
