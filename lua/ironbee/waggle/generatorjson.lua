
-- ###########################################################################
-- GeneratorJSON - generate a rules.conf or similar
-- ###########################################################################
local Generator = require('ironbee/waggle/generator')
local GeneratorJSON = {}
GeneratorJSON.__index = GeneratorJSON
setmetatable(GeneratorJSON, Generator)
GeneratorJSON.type = 'generatorjson'
GeneratorJSON.new = function(self)
    local t = {}
    return setmetatable(t, self)
end

GeneratorJSON.generate = function(self, plan, db)
    error("JSON generation not supported from Lua in IronBee")
end

return GeneratorJSON
