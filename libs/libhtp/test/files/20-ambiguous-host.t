>>>
GET http://example.com/thing HTTP/1.1
Host: example.com


<<<
HTTP/1.1 200 OK
Date: Mon, 26 Apr 2010 13:56:31 GMT
Content-Length: 8

12345678
>>>
GET http://example.com/thing HTTP/1.1
Host: foo.com


<<<
HTTP/1.1 200 OK
Date: Mon, 26 Apr 2010 13:56:31 GMT
Content-Length: 8

12345678
>>>
POST http://www.example.com:8001/ HTTP/1.1
Host: www.example.com:8001
Content-Length: 8

12345678
<<<
HTTP/1.1 200 OK
Date: Mon, 26 Apr 2010 13:56:31 GMT
Content-Length: 8

12345678
>>>
POST http://www.example.com:8002/ HTTP/1.1
Host: www.example.com:8003
Content-Length: 8

12345678
<<<
HTTP/1.1 200 OK
Date: Mon, 26 Apr 2010 13:56:31 GMT
Content-Length: 8

12345678