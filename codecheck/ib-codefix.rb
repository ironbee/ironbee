#!/usr/bin/env ruby

require 'rubygems'
require 'rake'

module Util
  def self.normalize_string(str)
    str.gsub(/\s+/m, ' ')
  end
end

module CodeFix
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

  # Return true if no license text is found in the string.
  def self.license?(str)
    Util::normalize_string(str).index(@@LICENSE_COMPRESSED).nil?
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
      str.gsub!(/^(.*\S)[ \t]+$/, '\1')
    else
      # In normal mode allow for there to be no .*\S pattern (an empty capture)
      # which results in empty strings being trimmed to 0 length.
      str.gsub!(/^(.*\S|)[ \t]+$/, '\1')
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
        fl.exclude('./test/gtest/**/*')

        # This file is auto generated.
        fl.exclude('./engine/config-parser.c')
      end
    end.each { |f| yield f }
  end
        
end

CodeFix::each_file do |f|

  # Options used in the below call to indent.
  #   This is the apache indent line: 
  #     http://httpd.apache.org/dev/styleguide.html
  #   -i4 - Indent 4
  #   -npsl - No proc names starting lines
  #   -di0 - Delcarations go in column 0.
  #   -br - Put braces on the same line as an if.
  #   -nce - No cuddle else
  #   -d0  - Comments are indented to column 0.
  #   -cli0 - Case labels are indented 0 spaces.
  #   -npcs - No space after function calls.
  #   -nfc1 - Do not touch first-column comments. Leaves our license alone.
  #   -nut - Do not use tabs.
  system("indent -i4 -npsl -di0 -br -nce -d0 -cli0 -npcs -nfc1 -nut %s"%[f])

  # Read in text.
  txt = File.open(f) { |io| io.read } 

  # Update licensing.
  CodeFix::add_license!(txt) if CodeFix::license?(txt)

  if f =~ /.py$/
    CodeFix::remove_trailing_ws!(txt, :python => true)
  else
    CodeFix::remove_trailing_ws!(txt, :python => nil)
  end

  # Write resultant file.
  File.open(f, 'w') { |io| write(txt) }
end
