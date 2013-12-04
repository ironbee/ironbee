
-- ###########################################################################
-- Predicate
-- ###########################################################################
local Rule = require('ironbee/waggle/signature')

local Predicate = {}
Predicate.__index = Predicate
Predicate.type = "predicate"
setmetatable(Predicate, Rule)

-- Create a new external rule. The operator of this should be
-- similar to "lua:/path/to/script". Notice no @.
Predicate.new = function(self, id, rev, db)
    local si = Rule:new(id, rev, db)
    si.data.rule_type = 'Predicate'

    -- Tailor functionality to predicate
    si.field = nil
    si.fields = nil
    si.op = nil
    si.expr = si.predicate
    si.predicate = nil

    return setmetatable(si, self)
end

-- Report if this rule type is a stream rule or not.
-- TODO: Change once/if predicate supports streaming
Predicate.is_streaming = function(self)
    return false
end

return Predicate
