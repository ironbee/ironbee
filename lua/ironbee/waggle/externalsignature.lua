
local Rule = require('ironbee/waggle/signature')

-- ###########################################################################
-- ExternalSignature - A signature (rule) that is an outside script.
-- ###########################################################################
local ExternalSignature = {}
ExternalSignature.__index = ExternalSignature
ExternalSignature.type = "externalsignature"
setmetatable(ExternalSignature, Rule)

-- Create a new external rule. The operator of this should be
-- similar to "lua:/path/to/script". Notice no @.
ExternalSignature.new = function(self, id, rev, db)
    local es = Rule:new(id, rev, db)
    es.data.rule_type = 'RuleExt'
    return setmetatable(es, self)
end

-- Set the script of this external rule.
ExternalSignature.script = function(self, script)
    self.data.script = script
    return self
end

return ExternalSignature
