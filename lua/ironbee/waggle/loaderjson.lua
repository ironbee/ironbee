
-- ###########################################################################
-- LoaderJSON - generate a rules.conf or similar
-- ###########################################################################
local LoaderJSON = {}

LoaderJSON.__index = LoaderJSON
LoaderJSON.type = 'loaderjson'
LoaderJSON.new = function(self)
    local l = {}
    return setmetatable(l, self)
end

-- Rule loading that is common to all rule types.
--
-- @returns Table of values used to build the rule and a sig object. 
--          data, sig = self:loadCommonRule(jsonsig)
LoaderJSON.loadCommonRule = function(self, jsonsig)
    error("JSON generation not supported from Lua in IronBee")
end
LoaderJSON.applyCommonRule = function(self, jsonsig, sig, data)
    error("JSON generation not supported from Lua in IronBee")
end
LoaderJSON.loadExtRule = function(self, jsonsig, sig, data, db)
    error("JSON generation not supported from Lua in IronBee")
end
LoaderJSON.loadStrRule = function(self, jsonsig, sig, data, db)
    error("JSON generation not supported from Lua in IronBee")
end
LoaderJSON.loadRule = function(self, jsonsig, sig, data, db)
    error("JSON generation not supported from Lua in IronBee")
end
LoaderJSON.load = function(self, json, db)
    error("JSON generation not supported from Lua in IronBee")
end

return LoaderJSON
