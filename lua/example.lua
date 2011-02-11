----------------------------------------------------------------------------
-- Define local versions of globals that will be used
----------------------------------------------------------------------------
local base = _G
local ironbee = require("ironbee")

----------------------------------------------------------------------------
-- Declare the rest of the file as a module and register the module
-- table with ironbee.
----------------------------------------------------------------------------
module(...)
ironbee.register_module(_M)

----------------------------------------------------------------------------
-- Setup some module metadata
----------------------------------------------------------------------------
_COPYRIGHT = "Copyright (C) 2010-2011 Qualys, Inc."
_DESCRIPTION = "IronBee example Lua module"
_VERSION = "0.1"

----------------------------------------------------------------------------
-- Event Handlers
--
-- NOTE: As a best practice, you should avoid using the "onEvent" prefix
-- in any public functions that are NOT to be used as event handlers as
-- these may be treated specially by the engine.
----------------------------------------------------------------------------

-- This is called when the request headers are avalable to inspect.
function onEventHandleRequestHeaders(...)
    local ib, tx = ...

    ironbee.log_debug(ib, 4, "Lua: %s.onEventHandleRequestHeaders", _NAME)
 
    -- Do something interesting

    return 0
end
