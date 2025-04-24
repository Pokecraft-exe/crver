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

#define VERSION "c_rver API / 2.0.0 (Windows)"
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

	std::map<int, const char*> status_code_str = {
		{100, "Continue"},
		{101, "Switching Protocols"},
		{102, "Processing"},
		{103, "Early Hints"},
		{200, "OK"},
		{201, "Created"},
		{202, "Accepted"},
		{203, "Non-Authoritative Information"},
		{204, "No Content"},
		{205, "Reset Content"},
		{206, "Partial Content"},
		{207, "Multi-Status"},
		{208, "Already Reported"},
		{226, "IM Used"},
		{300, "Multiple Choices"},
		{301, "Moved Permanently"},
		{302, "Found"},
		{303, "See Other"},
		{304, "Not Modified"},
		{305, "Use Proxy"},
		{306, "unused"},
		{307, "Temporary Redirect"},
		{308, "Permanant Redirect"},
		{400, "Bad Request"},
		{401, "Unauthorized"},
		{402, "Payment Required"},
		{403, "Forbidden"},
		{404, "Not Found"},
		{405, "Method Not Allowed"},
		{406, "Not Acceptable"},
		{407, "Proxy Authentification Required"},
		{408, "Request Timeout"},
		{409, "Conflicts"},
		{410, "Gone"},
		{411, "Length Required"},
		{412, "Precondition Failed"},
		{413, "Content Too Large"},
		{414, "URI Too Long"},
		{415, "Unsupported Media Type"},
		{416, "Range Not Satisfiable"},
		{417, "Expectation Failed"},
		{418, "I'm a teapot"},
		{421, "Misdirected Request"},
		{422, "Unprocessable Content"},
		{423, "Locked"},
		{424, "Failed Dependancy"},
		{425, "Too Early"},
		{426, "Upgrade Required"},
		{428, "Precondition Required"},
		{429, "Too Many Requests"},
		{431, "Request Header Fields Too Large"},
		{451, "Unavaliable For Legal Reasons"},
		{500, "Internal Server Error"},
		{501, "Not Implemented"},
		{502, "Bad Gateway"},
		{503, "Service Unavailable"},
		{504, "Gateway Timeout"},
		{505, "HTTP Version Not Supported"},
		{506, "Variant Also Negotiates"},
		{508, "Insufficient Storage"},
		{509, "Loop Detected"},
		{510, "Not Extended"},
		{511, "Network Authentication Required"},
	};

	enum class contentType {
		none = 0,
		textHTML,
		textCSS,
		textJS
	};

	std::string header = "Connection: keep-alive\n"
						"Referrer-Policy: strict-origin-when-cross-origin\n"
						"X-Content-Type-Options: nosniff";

	std::string head = "";

	void addHeader(std::string newHeader){
		header += '\n' + newHeader;
		return;
	}
	 void link(std::string rel, std::string type, std::string href) {
		head += "<link rel=\"" + rel + "\" type=\"" + type + "\" href=\"" + href + "\" />";
		return;
	}
	void title(std::string title) {
		head += "<title>" + title + "</title>";
		return;
	}
	 void meta(std::string name, std::string content, std::string href) {
		head += "<meta name=\"" + name + "\" content=\"" + content + "\" />";
		return;
	}
	 void metaCharset(std::string charset) {
		head += "<meta charset=\"" + charset + "\" />";
		return;
	}

	int HTTPResponse(int status_code, std::string content_type) {
		::std::cout.rdbuf(stdoutBuffer);

		std::time_t t_date = time(nullptr);
		std::cout << "HTTP/1.1 " << status_code << " " << status_code_str[status_code] << "\n"
						"Date : " << std::put_time(gmtime(&t_date), (char*)"%a, %d %b %Y %T %Z") <<
						"\nServer: " VERSION "\n"
						"Content-Type: " << content_type << "\n"
						"Content-Length:" << std::to_string(__HTTPbuffer__.str().length() + 43 + head.length()) << '\n'
						<< header << "\r\n\r\n\r\n" << "<!DOCTYPE html><html><head>" << head << "</head>" <<
						__HTTPbuffer__.str() << "</html>\r\n";

		return 0;
	};

	int HTTP404() {
		std::cout.clear();

		std::cout << 
		"<html><head><title>404 Not Found</title></head>"
		"<body><center><h1>404 Not Found</h1><br>c_rver/1.0.0 (Windows)"
		"</center></body></html>";

		return HTTPResponse(404, "text/html");
	}
}