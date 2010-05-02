>>>
GET / HTTP/1.0
User-Agent: Mozilla
Cookie: a=1; b =2; =3; c
X-Authorization: Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==
Authorization: Digest username="Mufasa",
                 realm="testrealm@host.com",
                 nonce="dcd98b7102dd2f0e8b11d0f600bfb0c093",
                 uri="/dir/index.html",
                 qop=auth,
                 nc=00000001,
                 cnonce="0a4f113b",
                 response="6629fae49393a05397450978507c4ef1",
                 opaque="5ccc069c403ebaf9f0171e9517f40e41"




<<<
HTTP/1.0 200 OK
Date: Mon, 31 Aug 2009 20:25:50 GMT
Server: Apache
Connection: close
Content-Type: text/html
Content-Length: 12

Hello World!