#pragma once
#include <sys/types.h>
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <vector>
#include <map>
#include <thread>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <filesystem>
#include <fstream>
#include <array>
#include <memory>
#include <stdexcept>
#include <cstdio>

#define MacroHTTPResponse(DATE, CONTENT, LEN) "HTTP/1.1 200 OK\n" 							\
						<< "Date : " << std::put_time(DATE, (char*)"%a, %d %b %Y %T %Z")    \
						<< "\nServer: c_rver / 1.0.0 (Windows)\n"                           \
						<< "Content-Type: text/html\n"                                      \
						<< "Content-Length:" << std::to_string(LEN)                         \
						<< "Connection: keep-alive\n"                                       \
						<< "Referrer-Policy: strict-origin-when-cross-origin\n"             \
						<< "X-Content-Type-Options: nosniff\n"                              \
						<< "Feature-Policy: accelerometer 'none'; camera 'none'; geolocation 'none'; gyroscope 'none'; magnetometer 'none'; microphone 'none'; payment 'none'; usb 'none'\n" \
						<< "Content-Security-Policy: default-src 'self'; script-src cdnjs.cloudflare.com 'self'; style-src cdnjs.cloudflare.com 'self' fonts.googleapis.com 'unsafe-inline'; font-src fonts.googleapis.com fonts.gstatic.com cdnjs.cloudflare.com; frame-ancestors 'none'; report-uri https://scotthelme.report-uri.com/r/d/csp/enforce\r\n\r\n" \
						<< CONTENT << "\r\n"

#define MacroHTTP404(DATE) "HTTP/1.1 404 Not Found\n"					                      \
						<< "Date : " << std::put_time(DATE, (char*)"%a, %d %b %Y %T %Z")        \
						<< "\nServer: c_rver / 1.0.0 (Windows)\n"                               \
						<< "Connection: keep-alive\n"                                           \
						<< "Referrer-Policy: strict-origin-when-cross-origin\r\n"               \
						<< "Content-Type: text/html\n"                                          \
						<< "Content-Length: 149\n"                                              \
						<< "X-Content-Type-Options: nosniff\r\n\r\n"                            \
						<< "<html><head><title>404 Not Found</title></head><body><center>"      \
						<< "<h1>404 Not Found</h1></center><hr><center>c_rver/1.0.0 (Windows)"  \
						<< "</center></body></html>\r\n"

#define MakeEnvironnemnt(x) ::crver::HTTPENV x;::crver::makeHTTPENV(&x, argv)

namespace crver {
	#if defined(_WIN32) || defined(_WIN64)
		#include "WinSock2.h"
		class Session {
			public:
			SOCKET connexion = INVALID_SOCKET;

			private:
			int __unused__ = 0;
			size_t __unused2__ = 0;
			_Field_size_bytes_(__unused__) struct sockaddr __unused3__ = { 0 };

			public:
			std::string cache = "";
			~Session() {}
		};

	#endif

	enum HTTPMethod {
		none = 0,
		GET,
		POST
	};

	struct HTTPRequest {
		HTTPMethod method;
		std::string page = "";
		std::string host = "";
		std::map<std::string, std::string> GET = {};
		std::map<std::string, std::string> POST = {};
	};

	struct HTTPENV {
		Session session;
		HTTPRequest request;
		~HTTPENV() {}
	};

	typedef char** ENV;
	::std::streambuf* stdoutBuffer;
	::std::ostringstream __HTTPbuffer__;

	template <class _Elem, class _Traits>
	std::basic_ostream<_Elem, _Traits>& __CLRCALL_OR_CDECL endl(
    std::basic_ostream<_Elem, _Traits>& _Ostr) {
    _Ostr.put(_Ostr.widen('<'));
    _Ostr.put(_Ostr.widen('b'));
    _Ostr.put(_Ostr.widen('r'));
    _Ostr.put(_Ostr.widen('/'));
    _Ostr.put(_Ostr.widen('>'));
    return _Ostr;
	}

	void makeHTTPENV(HTTPENV* re, ENV e) {
		std::string cache = e[1], methodstr = "";
		std::ifstream ifCacheFile(cache, std::ios::binary);

		// redirect stdout to write the http responde with header on top of it
		stdoutBuffer = std::cout.rdbuf();
		std::cout.rdbuf(__HTTPbuffer__.rdbuf());

		// restore session and request data
		ifCacheFile >> re->session.connexion;
		re->session.cache = cache;

		ifCacheFile >> methodstr;
		if (!methodstr.compare("1")) {

			std::string first, second;
			re->request.method = GET;
			while (ifCacheFile >> first) {
				ifCacheFile >> second;

				re->request.GET[first] = second;
			}
		} else {
			std::string first, second;
			re->request.method = POST;
			while (ifCacheFile >> first) {
				ifCacheFile >> second;

				re->request.POST[first] = second;
			}
		}

		ifCacheFile.close();
		return;
	}

	int HTTPResponse() {
		::std::cout.rdbuf(stdoutBuffer);

		std::time_t t_date = time(nullptr);
		std::cout << MacroHTTPResponse(gmtime(&t_date), __HTTPbuffer__.str(), __HTTPbuffer__.str().length());

		return 0;
	};

	int HTTP404() {
		::std::cout.rdbuf(stdoutBuffer);

		std::time_t t_date = time(nullptr);
		std::cout << MacroHTTP404(gmtime(&t_date));

		return 1;
	}
}