--[[ -------------------------------------------------------------------------
--   Define Predicate primitives for data inspection.
--]] -------------------------------------------------------------------------

-- -----------------------------------------------------------
-- RequestMethodList(space-separated-method-list)
--
-- EX: P.RequestMethodList("GET HEAD POST")
-- -----------------------------------------------------------
PUtil.Define(
    "RequestMethodList",
    { "methods" },
    P.Operator(
        "imatch",
        P.Ref("methods"),
        P.Var("REQUEST_METHOD")
    )
)

-- -----------------------------------------------------------
-- RequestMethodRegex(regex)
--
-- EX: P.RequestMethodRegex([=[(?i)^(?:GET|HEAD|POST)$]=])
-- -----------------------------------------------------------
PUtil.Define(
    "RequestMethodRegex",
    { "patt" },
    P.Operator(
        "rx",
        P.Ref("patt"),
        P.Var("REQUEST_METHOD")
    )
)

-- -----------------------------------------------------------
-- RequestPathRegex(regex)
--
-- NOTE: Path is first normalized
--
-- EX: P.RequestPathRegex([=[^/my/path/prefix(?:/|$)]=])
-- -----------------------------------------------------------
PUtil.Define(
    "RequestPathRegex",
    { "patt" },
    P.OrSC(
        -- First try what is supposed to be the normalized path
        P.Operator(
            "rx",
            P.Ref("patt"),
            P.Var("REQUEST_URI_PATH")
        ),
        -- Then try normalizing raw version via transformation
        P.Operator(
            "rx",
            P.Ref("patt"),
            P.Transformation(
                "normalizePath",
                P.Var("REQUEST_URI_PATH_RAW")
            )
        ),
        -- Finally try the Windows version of normalized raw
        P.Operator(
            "rx",
            P.Ref("patt"),
            P.Transformation(
                "normalizePathWin",
                P.Var("REQUEST_URI_PATH_RAW")
            )
        )
    )
)

-- -----------------------------------------------------------
-- ClientAddress(space-separated-cidr4-list)
--
-- EX: P.ClientAddress("192.168.0.0/16 172.16.0.0/12 10.0.0.0/8")
-- -----------------------------------------------------------
PUtil.Define(
    "ClientAddress",
    { "cidr4" },
    P.Operator(
        "ipmatch",
        P.Ref("cidr4"),
        P.Var("REMOTE_ADDR")
    )
)

-- -----------------------------------------------------------
-- RequestHeaderRegex(patt, name)
--
-- EX: P.RequestHeaderRegex([=[my-patt]=], "X-MyHeader")
-- -----------------------------------------------------------
PUtil.Define(
    "RequestHeaderRegex",
    { "patt", "name" },
    P.Operator(
        "rx",
        P.Ref("patt"),
        P.Sub(
            P.Ref("name"),
            P.Var("REQUEST_HEADERS")
        )
    )
)

-- -----------------------------------------------------------
-- ResponseHeaderRegex(patt, name)
--
-- EX: P.ResponseHeaderRegex([=[my-patt]=], "X-MyHeader")
-- -----------------------------------------------------------
PUtil.Define(
    "ResponseHeaderRegex",
    { "patt", "name" },
    P.Operator(
        "rx",
        P.Ref("patt"),
        P.Sub(
            P.Ref("name"),
            P.Var("RESPONSE_HEADERS")
        )
    )
)

-- -----------------------------------------------------------
-- UserAgentRegex(patt)
--
-- EX: P.UserAgentRegex([=[^curl/]=])
-- -----------------------------------------------------------
PUtil.Define(
    "UserAgentRegex",
    { "patt" },
    P.RequestHeaderRegex(
        P.Ref("patt"),
        "User-Agent"
    )
)

-- -----------------------------------------------------------
-- RequestHostList(space-separated-host-list)
--
-- EX: RequestHostList("foo.com www.foo.com")
-- -----------------------------------------------------------
PUtil.Define(
    "RequestHostList",
    { "hosts" },
    P.OrSC(
        -- Try parsed host first
        P.Operator(
            "imatch",
            P.Ref("hosts"),
            P.Var("REQUEST_HOST")
        ),
        -- Then try parsed from URI
        P.Operator(
            "imatch",
            P.Ref("hosts"),
            P.Var("REQUEST_URI_HOST")
        ),
        -- Finally fail to HTTP Host heder
        P.Operator(
            "imatch",
            P.Ref("hosts"),
            -- Filter out potential port
            P.FOperator(
                "rx",
                [=[^([^:]+)]=],
                P.Sub(
                    "Host",
                    P.Var("REQUEST_HEADERS")
                )
            )
        )
    )
)

-- -----------------------------------------------------------
-- RequestHostRegex(regex)
--
-- NOTE: Path is first normalized
--
-- EX: P.RequestHostRegex([=[^/my/path/prefix(?:/|$)]=])
-- -----------------------------------------------------------
PUtil.Define(
    "RequestHostRegex",
    { "patt" },
    P.OrSC(
        -- Try parsed host first
        P.Operator(
            "rx",
            P.Ref("patt"),
            P.Var("REQUEST_HOST")
        ),
        -- Then try parsed from URI
        P.Operator(
            "rx",
            P.Ref("patt"),
            P.Var("REQUEST_URI_HOST")
        ),
        -- Finally fail to HTTP Host heder
        P.Operator(
            "rx",
            P.Ref("patt"),
            -- Filter out potential port
            P.FOperator(
                "rx",
                [=[^([^:]+)]=],
                P.Sub(
                    "Host",
                    P.Var("REQUEST_HEADERS")
                )
            )
        )
    )
)

-- -----------------------------------------------------------
-- RequestPortList(space-separated-port-list)
--
-- EX: RequestPortList("80 8080")
-- -----------------------------------------------------------
PUtil.Define(
    "RequestPortList",
    { "ports" },
    P.Operator(
        "match",
        P.Ref("ports"),
        -- SERVER_PORT is numeric, so need to convert to string
        P.Transformation(
            "toString",
            P.Var("SERVER_PORT")
        )
    )
)

-- -----------------------------------------------------------
-- RequestPortRegex(port)
--
-- EX: RequestPortRegex(8080)
-- -----------------------------------------------------------
PUtil.Define(
    "RequestPortRegex",
    { "patt" },
    P.Operator(
        "rx",
        P.Ref("patt"),
        P.Transformation(
            "toString",
            P.Var("SERVER_PORT")
        )
    )
)




--[[
Template:

-- -----------------------------------------------------------
-- -----------------------------------------------------------
PUtil.Define(
    "",
    { "" },
    P.Operator(
        "",
        P.Ref(""),
        P.(
        )
    )
)

--]]
