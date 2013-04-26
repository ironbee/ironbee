Predicate = {}

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
  return Predicate.C(name, unpack(r))
end

-- Lua doesn't lookup operators via __index.
local common_operators = {
  __add = function (a, b) return merge('and', a, b)  end,
  __div = function (a, b) return merge('or', a, b)   end,
  __unm = function (a)    return Predicate.Not(a)    end,
  __sub = function (a, b) return a + (-b)            end,
  __pow = function (a, b) return Predicate.Xor(a, b) end
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
  elseif type(value) == 'number' then
    v = tostring(value)
  else
    error("String argument must be string or number.")
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
  return Predicate.S(self.value:len())
end
function string_mt:streq(o)
  return Predicate.Streq(self, o)
end
function string_mt:istreq(o)
  return Predicate.Istreq(self, o)
end

function string_mt:__call()
  return "'" .. self.value .. "'"
end

local call_mt = all_mt:new('mt')

-- Import string functions.
for i,k in ipairs({'streq', 'istreq'})  do
  call_mt[k] = string_mt[k]
end

function call_mt:new(name, ...)
  local r = all_mt.new(self, 'call')
  local children = {}
  for i,v in ipairs(arg) do
    table.insert(children, Predicate.from_lua(v))
  end
  r.name = name
  r.children = children
  return r
end

function call_mt:sub(field_name)
  return Predicate.Sub(field_name, self)
end
function call_mt:suball(field_name)
  return Predicate.Suball(field_name, self)
end
function call_mt:streq(value)
  return Predicate.Streq(value, self)
end
function call_mt:istreq(value)
  return Predicate.Istreq(value, self)
end
function call_mt:eq(value)
  return Predicate.Eq(value, self)
end

function call_mt:__call()
  r = "(" .. self.name
  for i,c in ipairs(self.children) do
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

function Predicate.Raw(value)
  return raw_mt:new(value)
end
Predicate.R = Predicate.Raw

function Predicate.String(value)
  return string_mt:new(value)
end
Predicate.S = Predicate.String
function Predicate.Call(name, ...)
  return call_mt:new(name, unpack(arg))
end
Predicate.C = Predicate.Call

Predicate.Null = all_mt:new("null")
Predicate.Null.__call = function() return "null" end
Predicate.True = Predicate.C("true")
Predicate.False = Predicate.C("false")

-- Calls

local param1 = {'LLength', 'Not', 'Field'}
local param2 = {'Sub', 'Suball', 'Name', 'Gt', 'Ge', 'Lt', 'Le', 'Rx', 'Transformation'}
local param3 = {'Operator'}
local paramn = {'Or', 'And', 'List'}
for i,n in ipairs(param1) do
  Predicate[n] = function (a) return Predicate.C(n:lower(), a) end
end
for i,n in ipairs(param2) do
  Predicate[n] = function (a, b) return Predicate.C(n:lower(), a, b) end
end
for i,n in ipairs(param3) do
  Predicate[n] = function (a, b, c) return Predicate.C(n:lower(), a, b, c) end
end
for i,n in ipairs(paramn) do
  Predicate[n] = function (...) return Predicate.C(n:lower(), unpack(arg)) end
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
  'lowercase'
}
for i,n in ipairs(tfns) do
  local capitalized = n:gsub("%l", string.upper)
  Predicate[capitalized] = function (a) return Predicate.C(n, a) end
  call_mt[n] = function (self) return Predicate[capitalized](self) end
end

function Predicate.If(a, b, c)
  a = Predicate.from_lua(a)
  b = Predicate.from_lua(b)
  c = Predicate.from_lua(c)

  local is_true, is_converted = Predicate.to_lua(a)
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
-- be swapped, possibly with an operator change.

local sym = {
  Streq  = function (a,b) return a.value           == b.value          end,
  Istreq = function (a,b) return a.value:lower()   == b.value:lower()   end,
  Eq     = function (a,b) return tonumber(a.value) == tonumber(b.value) end,
  Gt     = function (a,b) return tonumber(a.value) >  tonumber(b.value) end,
  Ge     = function (a,b) return tonumber(a.value) >= tonumber(b.value) end,
  Lt     = function (a,b) return tonumber(a.value) <  tonumber(b.value) end,
  Le     = function (a,b) return tonumber(a.value) <= tonumber(b.value) end
}
local sym_swap = {
  Gt = 'Lt',
  Lt = 'Gt',
  Ge = 'Le',
  Le = 'Ge'
}
for n,s in pairs(sym) do
  Predicate[n] = function (a, b)
    a = Predicate.from_lua(a)
    b = Predicate.from_lua(b)
    if a.type ~= 'string' and b.type ~= 'string' then
      error(n .. " requires at least one string literal argument.")
    end
    if a.type == 'string' and b.type == 'string' then
      if s(a, b) then
        return Predicate.True
      else
        return Predicate.False
      end
    end
    if a.type ~= 'string' then
      n = sym_swap[n] or n
      return Predicate.C(n:lower(), b, a)
    else
      return Predicate.C(n:lower(), a, b)
    end
  end
end

-- Indirect Calls

function Predicate.Xor(a, b)
  return (a - b) / (b - a)
end
function Predicate.Nand(...)
  return -Predicate.And(unpack(arg))
end
function Predicate.Nor(...)
  return -Predicate.Or(unpack(arg))
end
function Predicate.Nxor(...)
  return -Predicate.Xor(unpack(arg))
end

-- Utility

function Predicate.from_lua(v)
  if type(v) == 'string' or type(v) == 'number' then
    return Predicate.String(v)
  end
  if type(v) == 'boolean' then
    if v then
      return Predicate.True
    else
      return Predicate.False
    end
  end
  return v
end

function Predicate.to_lua(v)
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
  if v.type == 'string' then
    return v.value
  end
  return nil, false
end

function Predicate.pp(s)
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

return Predicate
