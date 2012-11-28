>>>
GET http://example.com/thing HTTP/1.1
Host: example.com


<<<
HTTP/1.1 200 OK
Date: Mon, 26 Apr 2010 13:56:31 GMT
Content-Length: 10

12345678
>>>
GET http://example.com/thing HTTP/1.1
Host: foo.com


<<<
HTTP/1.1 200 OK
Date: Mon, 26 Apr 2010 13:56:31 GMT
Content-Length: 10

12345678
