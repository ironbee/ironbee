----
-- Predicate Lua Front End
--
-- This Lua module provides a front end to Predicate.  See
-- predicate/lua_frontend.txt for a description of this module from a user
-- perspective.
--
-- This module defines a private class hierarchy with literal_mt, call_mt,
-- and raw_mt all inheriting from all_mt, and then provides a variety of
-- methods to allow easy construction of these objects.  Ultimately, these
-- objects can be called to convert them to their sexpr as strings.
--
-- Waggle provides a `predicate` method for all signature types that takes a
-- single argument: either a string or an expression object, and sets up the
-- predicate action.
----

local _M = {}
_M._COPYRIGHT = "Copyright (C) 2014 Qualys, Inc."
_M._DESCRIPTION = "IronBee Lua Predicate Frontend"
_M._VERSION = "1.1"

_M.Util = {}

-- Template define directive name.
local PREDICATE_DEFINE = 'PredicateDefine'

-- Private helpers

local function merge(name, a, b)
  local r = {}
  local append = function (n)
    if type(n) == 'table' and n.type == 'call' and n.name == name then
      for i,v in ipairs(n.children) do
        table.insert(r, v)
      end
    else
      table.insert(r, n)
    end
  end
  append(a)
  append(b)
  return _M.C(name, unpack(r))
end

local function decapitalize(s)
  return s:gsub("^%u", string.lower)
end

local function escape_string(s)
  return s:gsub(".", function (c)
    if c == '\\' or c == "'" then
      return "\\" .. c
    else
      return c
    end
  end)
end

-- Lua doesn't lookup operators via __index.
local common_operators = {
  __add = function (a, b) return merge('and', a, b)  end,
  __div = function (a, b) return merge('or', a, b)   end,
  __unm = function (a)    return _M.Not(a)    end,
  __sub = function (a, b) return a + (-b)            end,
  __pow = function (a, b) return _M.Xor(a, b) end
}

-- Expression object and their methods.

local all_mt = {}
function all_mt:new(type)
  local r = {type = type, IsPredicateObject = true}
  for k, v in pairs(common_operators) do
    r[k] = v
  end
  setmetatable(r, self)
  self.__index = self
  return r
end

local literal_mt = all_mt:new('mt')

function literal_mt:new(value)
  local r = all_mt.new(self, 'literal')
  r.value = value
  return r
end

function literal_mt:__call()
  local v = self.value
  if v == nil then
    return ":"
  end
  if type(v) == 'string' then
    return "'" .. escape_string(v) .. "'"
  end
  if type(v) == 'number' then
    return "" .. v
  end
  if type(v) == 'boolean' then
    if v then
      return "''"
    else
      return ":"
    end
  end
  if type(v) == 'table' then
    local member_strings = {}
    for _,s in ipairs(v) do
      if not s.IsPredicateObject then
        if s.type == "call" then
          error("Cannot have calls in lists.")
        end
        s = _M.L(s)
      end
      table.insert(member_strings, s())
    end
    return "[" .. table.concat(member_strings, " ") .. "]"
  end
  error("Unsupported type: " .. type(v))
end

local call_mt = all_mt:new('mt')

function call_mt:new(name, ...)
  local r = all_mt.new(self, 'call')
  local children = {}

  for _,v in ipairs({...}) do
    if type(v) == "table" and v.IsPredicateObject then
      table.insert(children, v)
    else
      table.insert(children, _M.L(v))
    end
  end
  r.name = name
  r.children = children
  return r
end

function call_mt:__call()
  local r = "(" .. self.name
  for _,c in ipairs(self.children) do
    r = r .. " " .. c()
  end
  r = r .. ")"
  return r
end

local named_mt = all_mt:new('mt')

function named_mt:new(name, value)
  local r = all_mt.new(self, 'named')
  r.name = name
  if type(v) == "table" and v.IsPredicateObject then
    r.value = value
  else
    r.value = _M.L(value)
  end
  return r
end

function named_mt:__call()
  local r
  local name = self.name
  if name ~= nil and name ~= "" then
    local not_a_name = (name:find("[^-a-zA-Z0-9_.]") ~= nil) or (name:find("^[^a-zA-Z0-9_]") ~= nil)
    if not_a_name then
      r = "'" .. escape_string(name) .. "'"
    else
      r = name
    end
    r = r .. ":"
  end
  r = r .. self.value()
  return r
end

local raw_mt = all_mt:new('mt')
function raw_mt:new(value)
  local r = all_mt.new(self, 'raw')
  r.value = value
  return r
end
function raw_mt:__call()
  return self.value
end

-- Fundamentals

function _M.R(value)
  return raw_mt:new(value)
end

function _M.L(value)
  return literal_mt:new(value)
end

function _M.C(name, ...)
  return call_mt:new(name, ...)
end

function _M.N(name, value)
  return named_mt:new(name, value)
end

_M.Null = literal_mt:new(nil)
_M.True = _M.C("true")
_M.False = _M.C("false")

-- Calls

local calls = {
  -- Boolean
  {'And', -1},
  {'Or', -1},
  {'AndSC', -1},
  {'OrSC', -1},
  {'Not', 1},
  -- If takes 2 or 3

  -- List
  {'SetName', 2},
  {'PushName', 1},
  {'Cat', -1},
  {'List', -1},
  {'First', 1},
  {'Rest', 1},
  {'Nth', 2},
  {'Flatten', 1},
  {'Focus', 2},

  -- String
  {'StringReplaceRx', 3},
  {'Length', 1},

  -- Filters
  {'Eq', 2},
  {'Ne', 2},
  {'Lt', 2},
  {'Le', 2},
  {'Gt', 2},
  {'Ge', 2},
  {'Typed', 2},
  {'Named', 2},
  {'NamedI', 2},
  {'Sub', 2},
  {'NamedRx', 2},
  {'Longer', 2},

  -- Predicates
  {'IsLonger', 2},
  {'IsFinished', 1},
  {'IsLiteral', 1},
  {'IsList', 1},

  -- Math
  {'Add', 2},
  {'Mult', 2},
  {'Neg', 1},
  {'Recip', 1},
  {'Max', 1},
  {'Min', 1},

  -- Phase
  {'WaitPhase', 2},
  {'FinishPhase', 2},

  -- IronBee
  -- Var takes 1 or 3
  {'Ask', 2},
  {'Operator', 3},
  -- FOperator has special naming rules.
  {'Transformation', 3},

  -- Development
  {'P', -1},
  -- Sequence takes 2 or 3
  {'Identity', 1},

  -- Templates
  {'Ref', 1}
}
local special_calls = {
  'If',
  'Var',
  'Sequence',
  'Xor',
  'Nand',
  'Nor',
  'Nxor'
}

local arity_table = {
  [1] = function (n)
    local lower_n = decapitalize(n)
    _M[n] = function (a)
      return _M.C(lower_n, a)
    end
  end,
  [2] = function (n)
    local lower_n = decapitalize(n)
    _M[n] = function (a, b)
      return _M.C(lower_n, a, b)
    end
  end,
  [3] = function (n)
    local lower_n = decapitalize(n)
    _M[n] = function (a, b, c)
      return _M.C(lower_n, a, b, c)
    end
  end,
  [-1] = function (n)
    local lower_n = decapitalize(n)
    _M[n] = function (...)
      return _M.C(lower_n, ...)
    end
  end
}

for i,info in ipairs(calls) do
  local name = info[1]
  local arity = info[2]
  arity_table[arity](name)
  if arity == 1 then
    all_mt[decapitalize(name)] = function (self)
      return _M[name](self)
    end
  else
    all_mt[decapitalize(name)] = function (self, ...)
      return _M[name](..., self)
    end
  end
end
for _,name in ipairs(special_calls) do
  all_mt[decapitalize(name)] = function (self, ...)
    return _M[name](..., self)
  end
end
all_mt.fOperator = function (self, ...)
  return _M.FOperator(..., self)
end

-- Special cases
_M.FOperator = function (a, b, c)
  return _M.C('foperator', a, b, c)
end
_M.If = function (...)
  if #{...} ~= 3 and #{...} ~= 2 then
    error("If must have 2 or 3 arguments.")
  end
  return _M.C('if', ...)
end
_M.Var = function (...)
  if #{...} ~= 3 and #{...} ~= 1 then
    error("If must have 1 or 3 arguments.")
  end
  return _M.C('var', ...)
end
_M.Sequence = function (...)
  if #{...} ~= 3 and #{...} ~= 2 then
    error("If must have 2 or 3 arguments.")
  end
  return _M.C('sequence', ...)
end

-- Indirect Calls

function _M.Xor(a, b)
  return (a - b) / (b - a)
end
function _M.Nand(...)
  return -_M.And(...)
end
function _M.Nor(...)
  return -_M.Or(...)
end
function _M.Nxor(...)
  return -_M.Xor(...)
end

-- Utility

function _M.Util.FromLua(v)
  return _M.L(v)
end

function _M.Util.ToLua(v)
  if type(v) ~= "table" or not v.IsPredicateObject then
    return v, false
  end
  if v.type == nil then return nil, false end
  if v.type == 'call' then
    if v.name == 'true' then
      return true, true
    elseif v.name == 'false' then
      return false, true
    else
      return nil, false
    end
  end
  if v.type == 'literal' then
    return v.value
  end
  return nil, false
end

function _M.Util.PP(s)
  local indent = 0
  local r = ''
  local i = 1
  local n = s:len()
  local function slice_prefix(pattern)
    local a,b = s:find(pattern, i)
    if a then
      i = i + b - a + 1
      return s:sub(a,b)
    end
    return nil
  end
  while i <= n do
    slice_prefix('^ *')
    local line = slice_prefix('^[^()]+') or
                 slice_prefix('^%([^()]+%)') or
                 slice_prefix('^%([^ ]+') or
                 slice_prefix('^%)')
    if line == nil then
      error("Insanity error with i = " .. tostring(i))
    end
    if line == ')' then
      indent = indent - 2
    end
    for i=1,indent do
      r = r .. ' '
    end
    r = r .. line .. '\n'
    if line:sub(1,1) == '(' and line:sub(line:len(),line:len()) ~= ')' then
      indent = indent + 2
    end
  end
  return r
end

-- Template Support
function _M.Util.Declare(name, predicate_name)
  predicate_name = predicate_name or name
  _M[name] = function (...)
    return _M.C(predicate_name, ...)
  end
  return nil
end

function _M.Util.Define(name, args, body)
  if type(body) ~= 'string' then
    body = body()
  end
  local args_string = table.concat(args, ' ')
  _M.Util.Declare(name)

  if IB == nil then
    -- Not running in IronBee
    local info = debug.getinfo(2, "l")
    print("Define " .. info.currentline .. " " .. name .. "(" .. args_string .. "): " .. body)
  else
    IB:config_directive_process(PREDICATE_DEFINE, name, args_string, body)
  end
  return function (...)
    return _M[name](...)
  end
end

return _M
