
-- ###########################################################################
-- StreamInspect
-- ###########################################################################
local Rule = require('ironbee/waggle/signature')

local StreamInspect = {}
StreamInspect.__index = StreamInspect
StreamInspect.type = "streaminspect"
setmetatable(StreamInspect, Rule)

-- Create a new external rule. The operator of this should be
-- similar to "lua:/path/to/script". Notice no @.
StreamInspect.new = function(self, id, rev, db)
    local si = Rule:new(id, rev, db)
    si.data.rule_type = 'StreamInspect'
    return setmetatable(si, self)
end

-- Report if this rule type is a stream rule or not.
StreamInspect.is_streaming = function(self)
    return true
end

return StreamInspect
