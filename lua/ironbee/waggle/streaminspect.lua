
-- ###########################################################################
-- StreamInspect
-- ###########################################################################
local Signature = require('ironbee/waggle/signature')

local StreamInspect = {}
StreamInspect.__index = StreamInspect
StreamInspect.type = "streaminspect"
setmetatable(StreamInspect, Signature)

-- Create a new external rule. The operator of this should be
-- similar to "lua:/path/to/script". Notice no @.
StreamInspect.new = function(self, id, rev, db)
    local si = Signature:new(id, rev, db)
    si.data.rule_type = 'StreamInspect'
    return setmetatable(si, self)
end

return StreamInspect
