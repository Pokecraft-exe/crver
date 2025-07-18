#pragma once
#define _CRT_SECURE_NO_WARNINGS
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
#include "ipm.hpp"
#define VERSION "c_rver API / 4.0.0 (Windows)"
#define MakeEnvironnemnt(x) ::crver::HTTPENV x;::crver::makeHTTPENV(&x, argc, argv)

#ifndef IPM_NAME
#error "IPM_NAME not defined"
#endif

typedef struct QUIC_HANDLE* HQUIC;

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
		std::map<std::string, std::string> GET = {};
		std::map<std::string, std::string> POST = {};
	};

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

	struct WebIPM {
		bool busy;
		bool requested;
		bool reached;
		bool partial;
		SOCKET connexion;
		char data[0x1000];
	};

	interProcessMemory<WebIPM> ipm;

	HTTPRequest InitRequest() {
		HTTPRequest req = { none, "", {}, {} };
		std::stringstream request(ipm.data);
		std::string get("");
		int pos;
		if (ipm.data[0] == 'G') {
			req.method = GET;
			req.GET = std::map<std::string, std::string >();
			req.page = "";
			request >> get >> get;
	
			// replace & and = to space to >> operator works
			while ((pos = get.rfind('&')) != std::string::npos) {
				get.erase(pos, 1);
				get.insert(pos, " ");
			}
			while ((pos = get.rfind('=')) != std::string::npos) {
				get.erase(pos, 1);
				get.insert(pos, " ");
			}
			if ((pos = get.rfind('?')) != std::string::npos) {
				get.erase(pos, 1);
				get.insert(pos, " ");
			}
	
			// retrive the actual page
			request = std::stringstream(get);
			request >> req.page;
			req.page = req.page.substr(0, req.page.find('?'));
	
			// retrive the actual get map
			while (!request.eof()) {
				std::string one;
				std::string two;
	
				request >> one;
				request >> two;
	
				req.GET.insert({ one, two });
			}
	
			// retrive the session id cookie
			request = std::stringstream(ipm.data);
			while (std::getline(request, get)) {
				if (get.substr(0, 7).compare("Cookies") == 0) {
					pos = get.rfind('=');
					get.erase(pos, 1);
					get.insert(pos, " ");
	
					std::stringstream subRequest(get);
	
					subRequest >> get >> get;
					if (get.compare("Session-ID") == 0) {
						throw std::runtime_error("Session-ID not found");
						//subRequest >> req.SESSION_ID;
					}
				}
			}
		}
		else if (ipm.data[0] == 'P') {
			req.method = POST;
			req.POST = std::map<std::string, std::string >();
			req.page = "";
			request >> get >> get;
	
			// replace & and = to space to >> operator works
			while ((pos = get.rfind('&')) != std::string::npos) {
				get.erase(pos, 1);
				get.insert(pos, " ");
			}
			while ((pos = get.rfind('=')) != std::string::npos) {
				get.erase(pos, 1);
				get.insert(pos, " ");
			}
			if ((pos = get.rfind('?')) != std::string::npos) {
				get.erase(pos, 1);
				get.insert(pos, " ");
			}
	
			// retrive the actual page
			request = std::stringstream(get);
			request >> req.page;
			req.page = req.page.substr(0, req.page.find('?'));
		}
		else return req;
		return req;
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

	std::string HTTPResponse(int status_code, std::string content_type) {
		std::time_t t_date = time(nullptr);
		std::stringstream response("");

		response << "HTTP/3 " << status_code << " " << status_code_str[status_code] << "\n"
						"Date : " << std::put_time(gmtime(&t_date), (char*)"%a, %d %b %Y %T %Z") <<
						"\nServer: " VERSION "\n"
						"Content-Type: " << content_type << "\n"
						"Content-Length:" << std::to_string(__HTTPbuffer__.str().length() + 43 + head.length()) << '\n'
						<< header << "\r\n\r\n\r\n" << "<!DOCTYPE html><html><head>" << head << "</head>" <<
						__HTTPbuffer__.str() << "</html>\r\n";

		__HTTPbuffer__.str("");
		__HTTPbuffer__.clear();

		memcpy(ipm.data, response.str().c_str(), response.str().length());
		ipm.reached = true;
		ipm.requested = false;
		
		ipm.Global().update();

		return response.str();
	};

	std::string HTTP404() {
		std::cout.clear();

		std::cout << 
		"<html><head><title>404 Not Found</title></head>"
		"<body><center><h1>404 Not Found</h1><br>c_rver/1.0.0 (Windows)"
		"</center></body></html>";

		return HTTPResponse(404, "text/html");
	}
}

int WebMain(crver::HTTPRequest re);

int main() {
	using namespace crver;

	ipm.Open(IPM_NAME);

	// redirect stdout to write the http responde with header on top of it
	crver::stdoutBuffer = std::cout.rdbuf();
	std::cout.rdbuf(__HTTPbuffer__.rdbuf());

	if (ipm.Ready() == IPM_FAILED) {
		std::cerr << "Failed to open interprocess memory" << std::endl;
		return 1;
	}

	while (1) {
		if (ipm.Direct()->requested) {
			ipm.Local().update();
			ipm.requested = false;
			ipm.reached = true;
			HTTPRequest re = InitRequest();
			WebMain(re);
			ipm.Global().update();
		}
	}
}