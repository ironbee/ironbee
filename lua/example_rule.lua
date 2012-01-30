-- Example lua rule
-- This example compares the Host from the request header
--  to a constant string, and returns 1 if it matches,
--  or 0 if not.
t=...

tx = ironbee.newTx(t.tx)
local host = ironbee.ib_data_get(tx.dpi(), "request_headers.Host")
if host ~= nil then
  if host.value() == "pages.cs.wisc.edu" then
    ironbee.ib_log_debug(tx.ib(), 4, "Host is pages.cs.wisc.edu")
    return 1
  end      
end

return 0
