local mod = ...

mod:declare_config {
    mod:num("num", 0),
    mod:string("str", '')
}

mod:conn_started_event(function(conn, ev)
    conn:logInfo("Num is %d", tonumber(conn.config.num))
    conn:logInfo("Str is %s", ffi.string(conn.config.str))
    return 0
end)

return 0