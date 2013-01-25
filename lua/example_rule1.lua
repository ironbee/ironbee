-- Setup
local ib = ...

local log = function(...)
  io.stderr:write(" -- ")
  io.stderr:write(...)
  io.stderr:write("\n")
end
  
if ib == nil then
  log("ib is null. No action")
  return 0
end

if ibapi == nil then
  log("ibapi is null. No action")
  return 0
end

ibapi:logInfo("ib %s=%s", type(ib), tostring(ib))
for i,v in pairs(ib) do
    ibapi:logInfo("ib:%s = %s", i, tostring(v))
end

ibapi:logInfo("ibapi %s=%s", type(ibapi), tostring(ibapi))
for i,v in pairs(ibapi) do
    ibapi:logInfo("ibapi:%s = %s", i, tostring(v))
end

ibapi:logInfo("ibapi.engineapi %s=%s", type(ibapi.engineapi), tostring(ibapi.engineapi))
for i,v in pairs(ibapi.engineapi) do
    ibapi:logInfo("ibapi.engineapi:%s = %s", i, tostring(v))
end

ibapi:logInfo("ibapi.txapi %s=%s", type(ibapi.txapi), tostring(ibapi.txapi))
for i,v in pairs(ibapi.txapi) do
    ibapi:logInfo("ibapi.txapi:%s = %s", i, tostring(v))
end

ibapi:logInfo("ibapi.ruleapi %s=%s", type(ibapi.ruleapi), tostring(ibapi.ruleapi))
for i,v in pairs(ibapi.ruleapi) do
    ibapi:logInfo("ibapi.ruleapi:%s = %s", i, tostring(v))
end

-- Return a "no match"
return 0
