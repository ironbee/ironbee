

local Rule = require('ironbee/waggle/signature')

-- ###########################################################################
-- ExternalSignature - A signature (rule) that is an outside script.
-- ###########################################################################
local ActionSignature = {}
ActionSignature.__index = ActionSignature
ActionSignature.type = "actionsignature"
setmetatable(ActionSignature, Rule)

-- Create a new external rule. The operator of this should be
-- similar to "lua:/path/to/script". Notice no @.
ActionSignature.new = function(self, id, rev, db)
    local es = Rule:new(id, rev, db)
    es.data.rule_type = 'Action'
    return setmetatable(es, self)
end

-- Set the script of this external rule.
ActionSignature.script = function(self, script)
    self.data.script = script
    return self
end

return ActionSignature
