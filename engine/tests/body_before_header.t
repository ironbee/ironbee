>>>
GET /robots.txt HTTP/1.1
Cache-Control: no-cache
Connection: Keep-Alive
Pragma: no-cache
Accept: */*
Accept-Encoding: 
From: bingbot(at)microsoft.com

<<<
HTTP/1.1 200 OK
Accept-Ranges: bytes
ETag: W/"73-1301493866000"
Last-Modified: Wed, 30 Mar 2011 14:04:26 GMT
Content-Type: text/plain
Content-Length: 73
Vary: Accept-Encoding
Strict-Transport-Security: max-age=31536000
Keep-Alive: timeout=15, max=100
Connection: Keep-Alive

>>>
GET /foo HTTP/1.1
User-Agent: Mozilla/5.0 (Macintosh; U; Intel Mac OS X 10_6_6; en-us) AppleWebKit/533.20.25 (KHTML, like Gecko) Version/5.0.4 Safari/533.20.27
Accept: application/xml,application/xhtml+xml,text/html;q=0.9,text/plain;q=0.8,image/png,*/*;q=0.5
Accept-Language: en-us
Accept-Encoding: gzip, deflate
Connection: keep-alive
Proxy-Connection: keep-alive

<<<
HTTP/1.1 200 OK
Content-Type: text/html;charset=UTF-8
Vary: Accept-Encoding
Content-Encoding: gzip
Strict-Transport-Security: max-age=31536000
Content-Length: 1574
Keep-Alive: timeout=15, max=98
Connection: Keep-Alive

