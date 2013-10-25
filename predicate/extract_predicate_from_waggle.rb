#!/usr/bin/env ruby

require 'rubygems'
require 'language/lua'

WAGGLE_PATH = File.expand_path(File.join(
  File.expand_path(File.dirname(__FILE__)),
  '..', 'lua'
))
SCRIPT = DATA.read

lua = Language::Lua.new

script = ""
script += "package.path = package.path .. \";#{WAGGLE_PATH}/?.lua\";"
script += "local arg = {" + ARGV.collect {|x| "\"#{x}\""}.join(',') + "};"
script += SCRIPT
lua.eval(script)

__END__
local Waggle    = require "ironbee/waggle";
local Predicate = require "ironbee/predicate";

local SkipTable = {
    _DESCRIPTION = 1,
    _COPYRIGHT = 1,
    __index = 1,
    _VERSION = 1
}

-- Pull the Waggle DSL into the main environment.
for k,v in pairs(Waggle) do
    if not SkipTable[k] then
        _G[k] = v
    end
end
_G['P'] = Predicate

-- If the user gives no input, setup for a clean exit.
if (arg == nil) then
    arg = {}
end

-- Load all the files.
for _, file in ipairs(arg) do
    -- Load a Waggle file.
    dofile(file)
end

-- Extract Predicate.
for rule_id, rule_table in Waggle.all_ids() do
    for _, action in ipairs(rule_table.data.actions) do
        if action.name == "predicate" then
            print("PREDICATE", rule_id, rule_table.meta.line, action.argument)
        end
    end
end
