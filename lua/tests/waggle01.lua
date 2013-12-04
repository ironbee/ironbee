Rule("sig01", 1):
    fields("request_uri"):
    phase("REQUEST_HEADER"):
    op('rx', [[f\x00?oo]]):
    action("setRequestHeader:X-Foo=bar")
Rule("sig02", 1):
    fields("request_uri"):
    phase("REQUEST_HEADER"):
    op('streq', [[f\x00?oo]]):
    action("setRequestHeader:X-Bar=baz")
