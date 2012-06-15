#!/usr/bin/env ruby

require 'rubygems'
require 'rake'
require 'optparse'

# General utility methods.
module Util
  # Return a string with all white space reduced to a single space character.
  def self.normalize_string(str)
    str.gsub(/\s+/, ' ')
  end
end

# Code fixing (and checking) routines and constants.
module CodeFix

  # Official IronBee license from website.
  @@LICENSE = <<-EOF
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
 *****************************************************************************/
  EOF

  # Compress the license to only have single space characters for all
  # whitespace. This makes comparision more reliable.
  @@LICENSE_COMPRESSED = Util::normalize_string(@@LICENSE)

  # Return 1 if a perfect match, 2 if a near match, or nil otherwise.
  def self.license?(str)
    verbatim_license =
      ! Util::normalize_string(str).index(@@LICENSE_COMPRESSED).nil?

    # A perfect match? STOP!
    return 1 if verbatim_license

    heuristic_license =
      ! ( str.index('http://www.apache.org/licenses/LICENSE-2.0') &&
          str.index('Licensed to Qualys') ).nil?

    return 2 if heuristic_license

    nil

  end

  # Return true if the doxygen tag @file appears in the string.
  def self.doxy_file?(str)
    ! str.index(' @file').nil?
  end

  # Return true if the doxygen tag @author appears in the string.
  def self.doxy_author?(str)
    ! str.index(' @author ').nil?
  end

  # Modify the passed in string to contain the license. This is almost always
  # the first line in the file.
  def self.add_license!(str)
    str.sub!(/^.*$/m, "%s\n%s"%[ @@LICENSE, str])
  end

  # Using a regex, strip off trailing white space from each line.
  # Empty lines are reduced to 0 spaces.
  #
  # [options] If this contains :python set to true, then
  # lines compoosed of only spaces are left intact. Otherwise they are
  # stripped.
  def self.remove_trailing_ws!(str, options = {})
    if options[:python]
      # In "python" mode we require a \S char be on the line to trim it.
      str.gsub!(/(\S)[ \t]+$/, '\1')
    else
      # In normal mode allow for there to be no .*\S pattern (an empty capture)
      # which results in empty strings being trimmed to 0 length.
      str.gsub!(/[ \t]+$/, '')
    end
  end

  # Apply the given code block to each build file.
  def self.each_file(dir='.')
    Dir.chdir(dir) do

      # Start with a list of all files.
      Rake::FileList.new('./**/*.[ch]') do |fl|

        # We don't own anything in libs, so don't update it.
        fl.exclude('./libs/**/*')

        # We do not own the GTest code, so don't update it.
        fl.exclude('./tests/gtest/**/*')

        # This file is auto generated.
        fl.exclude('./engine/config-parser.c')

        # Autogen .h files
        fl.exclude('./**/*_auto_gen*')
      end
    end.each { |f| yield f }
  end

end

# This class will use the utility functions in CodeFix to
# fix or detect code problems. Those problems are reported and tracked.
class CodeMangler

  # By default 0, this is the number of errors produced analzing code.
  attr_accessor :errors

  # Similar to errors, but less severe.
  attr_accessor :warnings


  # If set, no file will be rewritten.
  attr_accessor :check_only

  # If set, files will not be indented with the indent cmd.
  attr_accessor :no_indent

  # If set, whitespace will not be trimmed.
  attr_accessor :no_trimws

  def initialize(srcdir='.')
    @srcdir = srcdir

    @indent_opts = [
      # Options used in the below call to indent.
      #   This is the apache indent line:
      #     http://httpd.apache.org/dev/styleguide.html
      '-i4',   # - Indent 4
      '-npsl', # - No proc names starting lines
      '-di0',  # - Delcarations go in column 0.
      '-br',   # - Put braces on the same line as an if.
      '-nce',  # - No cuddle else
      '-d0',   # - Comments are indented to column 0.
      '-cli0', # - Case labels are indented 0 spaces.
      '-npcs', # - No space after function calls.
      '-nfc1', # - Do not touch first-column comments. Leaves our license alone.
      '-nut'   # - Do not use tabs.
    ]

    @errors = 0
    @warnings = 0
  end

  def log(level, file, line, msg = nil)
    # Are we missing the optional line argument?
    if msg.nil?
      msg = line
      loc = file
    else
      loc = "%s:%s"%[ file, line.to_s ]
    end

    print "[%s] %s %s\n"%[ level, loc, msg ]
  end



  # Report a warning and increment @warnings. Line is an optional parameter.
  def report_warning(file, line, msg=nil)
    @warnings += 1
    log('WARN', file, line, msg)
  end

  # Report an error and increment @errors. Line is an optional parameter.
  def report_error(file, line, msg=nil)
    @errors += 1
    log('ERROR', file, line, msg)
  end

  def indent(file)
    unless system("indent",  *(@indent_opts + [ file ]))
      report_error(file, "The indent command exited non-zero.")
    end
  end

  # Run the CodeMangler.
  def call()
    CodeFix::each_file do |f|

      indent(f) unless @check_only || @no_indent

      # Read in text.
      txt = File.open(f) { |io| io.read }

      CodeFix::remove_trailing_ws!(txt, :python => (f=~/.py$/)
        ) unless @no_trimws

      case CodeFix::license?(txt)
        when 2: report_warning(f, "Verbatim license not detected.")
        when nil: report_error(f, "License not detected.")
      end

      if f =~ /\.h$/
        report_error(f, "@author not found.") unless CodeFix::doxy_author?(txt)
        report_error(f, "@file not found.") unless CodeFix::doxy_file?(txt)
      end

      # Write resultant file.
      File.open(f, 'w') { |io| io.write(txt) } unless @check_only
    end
  end
end


########################################################################
# Main
########################################################################
#
cm = CodeMangler.new

OptionParser.new do |op|
  op.on('--check-only') { |val| cm.check_only = true }
  op.on('--no-indent') { |val| cm.no_indent = true }
  op.on('--no-trimws') { |val| cm.no_trimws = true }
end.parse!

cm.call

if cm.warnings > 0
  print "#{cm.warnings} warnings.\n"
end

if cm.errors > 0
  print "#{cm.errors} errors.\n"
  exit 1
end
