#!/usr/bin/env ruby


GLOB = 'module-*.adoc'
TYPES = [
  'Action',
  'Directive',
  'Metadata',
  'Modifier',
  'Operator',
  'Transformation',
  'Var'
]
BASE_METADATA = ['Type', 'Module', 'Description', 'Version']

# When adding to this be sure to add to METADATA_ORDER as well.
METADATA = {
  'Action'         => BASE_METADATA + ['Syntax', 'Cardinality'],
  'Directive'      => BASE_METADATA + ['Default', 'Context', 'Syntax', 'Cardinality'],
  'Metadata'       => BASE_METADATA + ['Syntax', 'Cardinality'],
  'Modifier'       => BASE_METADATA + ['Syntax', 'Cardinality'],
  'Operator'       => BASE_METADATA + ['Syntax', 'Types', 'Capture'],
  'Transformation' => BASE_METADATA + ['InputType', 'OutputType'],
  'Var'            => BASE_METADATA + ['ValueType', 'Scope'],
}
METADATA_ORDER = [
  'Description',
  'Type',
  'Syntax',
  'Default',
  'Context',
  'Cardinality',
  'InputType',
  'OutputType',
  'ValueType',
  'Types',
  'Capture',
  'Scope',
  'Module',
  'Version'
]

# Map of type to (map of name to metadata)
items = Hash.new {|h,k| h[k] = {}}

Dir.glob(GLOB).each do |file|
  file_mod = nil
  current = nil
  current_line = nil
  previous = nil
  section = nil
  anchor = nil
  next_anchor = nil
  metadata = {}
  line_number = 0
  last_metadata_index = nil
  last_section_index = nil
  last_item = nil

  error = -> (item, msg) do
    n = current_line || line_number
    if item
      puts "#{file}:#{n} #{item} Error: #{msg}"
    else
      puts "#{file}:#{n} Error: #{msg}"
    end
  end

  process_item = -> do
    return if ! current

    # Require certain keys
    if ! metadata['Type']
      error.(current, "Missing type.")
      return
    end
    type = metadata['Type']
    if ! TYPES.member?(type)
      error.(current, "Unknown type: #{type}")
      return
    end
    METADATA[type].each do |m|
      if ! metadata[m]
        error.(current, "Missing metadata: #{m}")
      end
    end
    metadata.each_key do |m|
      if ! METADATA[type].member?(m)
        error.(current, "Unexpected metadata: #{m}")
      end
    end
    correct_anchor = "#{type.downcase}.#{current}"
    if anchor != correct_anchor
      error.(current, "Anchor is  #{anchor}; should be #{correct_anchor}.")
      return
    end
    s = type.gsub(/s$/,'')
    if s != section
      error.(current, "Section is #{section}; should be #{s}.")
      return
    end
    mod = metadata['Module']
    file_mod ||= mod
    if file_mod != mod
      error.(current, "Module is #{mod}; should be #{file_mod}?")
      return
    end
    if last_item
      if current.downcase <= last_item.downcase
        error.(current, "Out of order.")
      end
    end

    last_item = current
    items[type][current] = metadata
  end

  IO.foreach(file) do |line|
    line_number += 1
    line.chomp!

    if line =~ /^==== (.+)$/
      # New section.
      new_section = $1
      process_item.()
      section = new_section
      section.gsub!(/s$/,'')

      index = TYPES.index(section)
      if ! index
        error.(nil, "Unknown section: #{section}")
      end
      if last_section_index && last_section_index >= index
        error.(nil, "Section out of order: #{section}")
      end
      if ! next_anchor
        error.(nil, "Missing section anchor.")
      end

      last_section_index = index

      current = nil
      current_line = nil
      previous = nil
      anchor = nil
      next_anchor = nil
      last_item = nil
    elsif line =~ /^\[\[(.+)\]\]$/
      next_anchor = $1
    elsif line =~ /^===== (.+)/
      new_current = $1
      # New item.
      process_item.()
      previous = current
      current = new_current
      current_line = line_number
      if previous && previous.downcase > current.downcase
        error.(current, "Out of order; after #{previous}.")
      end
      metadata = {}
      anchor = next_anchor
      last_metadata_index = nil
    elsif line =~ /^\|\s*(\w+)\|(.+)$/
      # Metadata.
      metadata[$1] = $2
      index = METADATA_ORDER.index($1)
      if ! index
        error.(current, "Unknown metadata: #{$1}")
      elsif last_metadata_index && last_metadata_index >= index
        error.(current, "Metadata out of order: #{$1}")
      end
      last_metadata_index = index
    end
  end
  process_item.()
end

items.each do |type, items|
  File.open("modules-index-#{type.downcase}.adoc", "w") do |w|
    items.each do |name, metadata|
      mod = metadata['Module']
      description = metadata['Description']
      w.puts "<<#{type.downcase}.#{name},#{name}>> (<<module.#{mod},#{mod}>>) -- #{description} +"
    end
  end
end