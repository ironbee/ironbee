----
-- Predicate Lua Front End
--
-- This Lua module provides a front end to Predicate.  See
-- predicate/predicate.md for an overview of Predicate and
-- predicate/frontend.md for a description of this module from a user
-- perspective.
--
-- This module defines a private class hierarchy with string_mt, call_mt,
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
_M._VERSION = "1.0"

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
  local r = {type = type}
  for k, v in pairs(common_operators) do
    r[k] = v
  end
  setmetatable(r, self)
  self.__index = self
  return r
end

local string_mt = all_mt:new('mt')
function string_mt:new(value)
  local r = all_mt.new(self, 'string')
  local v
  if type(value) == 'string' then
    v = value
  else
    error("String argument must be string.")
  end
  v = v:gsub(".", function (c)
    if c == '\\' or c == "'" then
      return "\\" .. c
    else
      return c
    end
  end)
  r.value = v
  return r
end

function string_mt:length()
  return _M.S(self.value:len())
end
function string_mt:streq(o)
  return _M.Streq(self, o)
end
function string_mt:istreq(o)
  return _M.Istreq(self, o)
end

function string_mt:__call()
  return "'" .. self.value .. "'"
end

local numeric_mt = all_mt:new('mt')
function numeric_mt:new(value)
  local r = all_mt.new(self, 'numeric')
  local v
  if type(value) == 'number' then
    v = value
  else
    error("Numeric argument must be numeric.")
  end
  r.value = v
  return r
end

function numeric_mt:__call()
  return "" .. self.value
end

local call_mt = all_mt:new('mt')

-- Import string functions.
for _,k in ipairs({'streq', 'istreq'})  do
  call_mt[k] = string_mt[k]
end

function call_mt:new(name, ...)
  local r = all_mt.new(self, 'call')
  local children = {}

  for _,v in ipairs({...}) do
    table.insert(children, _M.from_lua(v))
  end
  r.name = name
  r.children = children
  return r
end

local call_members0 = {
  "scatter",
  "gather",
  "first",
  "rest",
  "isSimple",
  "isFinished",
  "isHomogeneous"
}
for i,n in ipairs(call_members0) do
  local capitalized = n:gsub("^%l", string.upper)
  call_mt[n] = function (self) return _M[capitalized](self) end
end

local call_members1 = {
  "sub",
  "setName",
  "streq",
  "istreq",
  "eq",
  "ne",
  "lt",
  "gt",
  "le",
  "ge",
  "nth",
  "isLonger",
  "isComplete",
  "named",
  "namedI",
  "namedRx",
  "waitPhase",
  "finishPhase",
  "ask"
}
for i,n in ipairs(call_members1) do
  local capitalized = n:gsub("^%l", string.upper)
  call_mt[n] = function (self, value) return _M[capitalized](value, self) end
end

local call_membersn = {
  "p"
}
for i,n in ipairs(call_membersn) do
  local capitalized = n:gsub("^%l", string.upper)
  call_mt[n] = function (self, ...) return _M[capitalized](..., self) end
end

function call_mt:__call()
  r = "(" .. self.name
  for _,c in ipairs(self.children) do
      r = r .. " " .. c()
  end
  r = r .. ")"
  return r
end

local raw_mt = all_mt:new('mt')
function raw_mt:new(value)
  r = all_mt.new(self, 'raw')
  r.value = value
  return r
end
function raw_mt:__call()
  return self.value
end

-- Fundamentals

function _M.Raw(value)
  return raw_mt:new(value)
end
_M.R = _M.Raw

function _M.String(value)
  return string_mt:new(value)
end
_M.S = _M.String
function _M.Number(value)
  return numeric_mt:new(value)
end
_M.N = _M.Number
function _M.Call(name, ...)
  return call_mt:new(name, ...)
end
_M.C = _M.Call

_M.Null = all_mt:new("null")
_M.Null.__call = function() return "null" end
_M.True = _M.C("true")
_M.False = _M.C("false")

-- Calls

local param1 = {
  'Not',
  'Scatter',
  'Gather',
  'First',
  'Rest',
  'isLiteral',
  'IsSimple',
  'IsFinished',
  'StreamRequestBody',
  'StreamResponseBody',
  'Fast',
  'Ref'
}
local param2 = {
  'SetName',
  'Transformation',
  'Nth',
  'Sub',
  'IsComplete',
  'Named',
  'NamedRx',
  'Typed',
  'Eq',
  'Ne',
  'Lt',
  'Gt',
  'Le',
  'Ge',
  'WaitPhase',
  'FinishPhase',
  'Ask'
}
local param3 = {
  'Operator',
  'FOperator'
}
local paramn = {
  'Or',
  'And',
  'Cat',
  'OrSC',
  'AndSC',
  'isHomogeneous',
  'Field',
  'P',
  'Sequence',
}
for i,n in ipairs(param1) do
  _M[n] = function (a) return _M.C(n:lower(), a) end
end
for i,n in ipairs(param2) do
  _M[n] = function (a, b) return _M.C(n:lower(), a, b) end
end
for i,n in ipairs(param3) do
  _M[n] = function (a, b, c) return _M.C(n:lower(), a, b, c) end
end
for i,n in ipairs(paramn) do
  _M[n] = function (...) return _M.C(n:lower(), ...) end
end

local operators = {
  'rx'
}
for i,n in ipairs(operators) do
  local capitalized = n:gsub("^%l", string.upper)
  _M[capitalized] = function (a, b) return _M.Operator(n, a, b) end
  _M["F" .. capitalized] = function (a, b) return _M.FOperator(n, a, b) end
  call_mt[n] = function (self) return _M[capitalized](self) end
  call_mt["f" .. n] = function (self) return _M["F" .. capitalized](self) end
end

local tfns = {
  'normalizePathWin',
  'normalizePath',
  'htmlEntityDecode',
  'urlDecode',
  'min',
  'max',
  'count',
  'length',
  'compressWhitespace',
  'removeWhitespace',
  'trim',
  'trimRight',
  'trimLeft',
  'lc',
  'lowercase',
  'ceil',
  'floor',
  'round',
  'name',
  'names',
  'toString',
  'toInteger',
  'toFloat'
}
for i,n in ipairs(tfns) do
  local capitalized = n:gsub("^%l", string.upper)
  _M[capitalized] = function (a) return _M.Transformation(n, a) end
  call_mt[n] = function (self) return _M[capitalized](self) end
end

function _M.If(a, b, c)
  a = _M.from_lua(a)
  b = _M.from_lua(b)
  c = _M.from_lua(c)

  local is_true, is_converted = _M.to_lua(a)
  if is_converted then
    if is_true then
      return b
    else
      return c
    end
  end

  return P.Call('if', a, b, c)
end

-- Symmetric Calls -- First argument must be string literal.  Arguments can
-- be swapped.

local sym = {
  Streq  = function (a,b) return a.value           == b.value           end,
  Istreq = function (a,b) return a.value:lower()   == b.value:lower()   end,
}
for n,s in pairs(sym) do
  _M[n] = function (a, b)
    a = _M.from_lua(a)
    b = _M.from_lua(b)
    if a.type ~= 'string' and b.type ~= 'string' then
      error(n .. " requires at least one string literal argument.")
    end
    if a.type == 'string' and b.type == 'string' then
      if s(a, b) then
        return _M.True
      else
        return _M.False
      end
    end
    if a.type ~= 'string' then
      n = sym_swap[n] or n
      return _M.C(n:lower(), b, a)
    else
      return _M.C(n:lower(), a, b)
    end
  end
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

function _M.from_lua(v)
  if type(v) == 'string'then
    return _M.String(v)
  end
  if type(v) == 'number' then
    return _M.Number(v)
  end
  if type(v) == 'boolean' then
    if v then
      return _M.True
    else
      return _M.False
    end
  end
  return v
end

function _M.to_lua(v)
  if v.type == nil then return nil, false end
  if v.type == 'null' then return nil, true end
  if v.type == 'call' then
    if v.name == 'true' then
      return true, true
    elseif v.name == 'false' then
      return false, true
    else
      return nil, false
    end
  end
  if v.type == 'string' or v.type == 'numeric' then
    return v.value
  end
  return nil, false
end

function _M.pp(s)
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
function _M.declare(name, predicate_name)
  predicate_name = predicate_name or name
  _M[name] = function (...)
    return _M.C(predicate_name, ...)
  end
  return nil
end

function _M.define(name, args, body)
  if type(body) ~= 'string' then
    body = body()
  end
  local args_string = table.concat(args, ' ')
  _M.declare(name)

  if IB == nil then
    print("Not running in IronBee?")
    print("Define " .. name .. "(" .. args_string .. "): " .. body)
  else
    IB:config_directive_process(PREDICATE_DEFINE, name, args_string, body)
  end
  return function (...)
    return _M[name](...)
  end
end

return _M
