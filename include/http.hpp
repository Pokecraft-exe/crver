#pragma once
#include <vector>
#include <string>
#include <map>
#include "win32socket.hpp"

#define HTTPResponse(DATE, CONTENT, LEN, SESSIONID, CONTENT_TYPE) \
				(char*)(std::string("HTTP/1.1 200 OK\nDate : ") + timeToString(DATE) +		\
				"\nServer: c_rver / 2.0.0 (Windows)\n"								\
				"Content-Type: " + CONTENT_TYPE + "\n"					\
				"Content-Length:" + std::to_string(LEN) + "\n"						\
				"Connection: keep-alive\n"											\
				"Referrer-Policy: strict-origin-when-cross-origin\n"				\
				"X-Content-Type-Options: nosniff\n"									\
				"Set-Cookie: Session-ID=" + std::to_string(SESSIONID) + "\n"		\
				"Feature-Policy: accelerometer 'none'; camera 'none'; geolocation 'none'; gyroscope 'none'; magnetometer 'none'; microphone 'none'; payment 'none'; usb 'none'\n" \
				+ std::string(CONTENT) + "\r\n").c_str()

#define HTTP404(DATE) \
						(char*)(std::string("HTTP/1.1 404 Not Found\nDate : ") + timeToString(date) + \
						"\nServer: c_rver / 2.0.0 (Windows)\n"\
						"Connection: keep-alive\n"\
						"Referrer-Policy: strict-origin-when-cross-origin\r\n"\
						"Content-Type: text/html\n"\
						"Content-Length: 148\n"\
						"X-Content-Type-Options: nosniff\r\n\r\n"\
						"<html><head><title>404 Not Found</title></head><body><center>"\
						"<h1>404 Not Found</h1></center><hr><center>c_rver/1.0.0 (Windows)"\
						"</center></body></html>\r\n").c_str()

enum HTTPMethod {
	none = 0,
	GET,
	POST
};

struct HTTPRequest {
	HTTPMethod method = HTTPMethod::none;
	std::string page = "";
	std::string host = "";
	std::map<std::string, std::string> GET = {};
	std::map<std::string, std::string> POST = {};
	int SESSION_ID = -1;
};

HTTPRequest HTTP(WSABUF buffer, int len);