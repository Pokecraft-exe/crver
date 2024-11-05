#pragma once
#include <vector>
#include <string>
#include <map>
#include "win32socket.hpp"

#define HTTPResponse(DATE, CONTENT, LEN, SESSIONID) \
						"HTTP/1.1 200 OK\n"														\
						<< "Date : " << std::put_time(DATE, (char*)"%a, %d %b %Y %T %Z")		\
						<< "\nServer: c_rver / 1.0.0 (Windows)\n"								\
						<< "Content-Type: text/html\n"											\
						<< "Content-Length:" << std::to_string(LEN)								\
						<< "Connection: keep-alive\n"											\
						<< "Referrer-Policy: strict-origin-when-cross-origin\n"					\
						<< "X-Content-Type-Options: nosniff\n"									\
						<< "Set-Cookie: Session-ID=" << SESSIONID								\
						<< "Feature-Policy: accelerometer 'none'; camera 'none'; geolocation 'none'; gyroscope 'none'; magnetometer 'none'; microphone 'none'; payment 'none'; usb 'none'\n" \
						<< "Content-Security-Policy: default-src 'self'; script-src cdnjs.cloudflare.com 'self'; style-src cdnjs.cloudflare.com 'self' fonts.googleapis.com 'unsafe-inline'; font-src fonts.googleapis.com fonts.gstatic.com cdnjs.cloudflare.com; frame-ancestors 'none'; report-uri https://scotthelme.report-uri.com/r/d/csp/enforce\r\n\r\n" \
						<< CONTENT << "\r\n"

#define HTTP404(DATE) \
						"HTTP/1.1 404 Not Found\n"												\
						<< "Date : " << std::put_time(DATE, (char*)"%a, %d %b %Y %T %Z")		\
						<< "\nServer: c_rver / 1.0.0 (Windows)\n"								\
						<< "Connection: keep-alive\n"											\
						<< "Referrer-Policy: strict-origin-when-cross-origin\r\n"				\
						<< "Content-Type: text/html\n"											\
						<< "Content-Length: 149\n"												\
						<< "X-Content-Type-Options: nosniff\r\n\r\n"							\
						<< "<html><head><title>404 Not Found</title></head><body><center>"		\
						<< "<h1>404 Not Found</h1></center><hr><center>c_rver/1.0.0 (Windows)"	\
						<< "</center></body></html>\r\n"

enum HTTPMethod {
	none = 0,
	GET,
	POST
};

struct HTTPRequest {
	HTTPMethod method;
	std::string page;
	std::string host;
	std::shared_ptr < std::map<std::string, std::string> > GET;
	std::vector<std::string> POST;
	int SESSION_ID;
};

HTTPRequest HTTP(WSABUF buffer, int len);