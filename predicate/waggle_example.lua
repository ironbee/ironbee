local getHeader = P.define('getHeader', {'which'},
    P.Sub(P.Ref('which'), P.Var('REQUEST_HEADERS'))
)

local isGet = P.Rx('GET', P.Var('REQUEST_METHOD'));
local isPost = P.Rx('POST', P.Var('REQUEST_METHOD'));

Action("IsGet", "1"):
  action([[clipp_announce:IsGet]]):
  predicate(isGet)

Action("IsPost", "1"):
  action([[clipp_announce:IsPost]]):
  predicate(isPost)

Action("NoHost", "1"):
    action([[clipp_announce:IsPost]]):
    predicate((isGet / isPost) + P.Not(P.getHeader('Host')))

Action("ContentLengthZero", "1"):
    action([[clipp_announce:IsPost]]):
    predicate((isGet / isPost) + P.Eq(0, P.getHeader('Content-Length')))

Action("ThisHasAnError", "1"):
    predicate(P.Sub(P.Var('a'), 'foo'))
