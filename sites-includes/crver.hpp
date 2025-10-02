#pragma once
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
#define VERSION "c_rver API / 4.0.0 (Windows)"
#define MakeEnvironnemnt(x) ::crver::HTTPENV x;::crver::makeHTTPENV(&x, argc, argv)
#define STRINGIFY(x) R#x
#define HTML(x) crver::out << STRINGIFY((x))

#if defined (__linux__)
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <cstring>
#include <cerrno>
#include <sys/types.h>
#else
#if defined(_WIN32) || defined(_WIN64)
#define _CRT_SECURE_NO_WARNINGS
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#endif
#endif

#if defined(_MSC_VER)
#define WEBMAIN extern "C" __declspec(dllexport) WSABUF
#else
#if defined(__GNUC__) || defined(__clang__)
#define WEBMAIN extern "C" __attribute__((visibility("default"))) WSABUF
#else
#define WEBMAIN extern "C" WSABUF
#endif
#endif

typedef struct QUIC_HANDLE* HQUIC;

namespace crver {

#if (defined(_WIN32) || defined(_WIN64)) && !defined(crver)
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
		HTTP_GET,
		HTTP_POST,
		HTTP_PUT,
		HTTP_DELETE,
		HTTP_HEAD,
		HTTP_PATCH,
		HTTP_OPTIONS
	};

	struct HTTPRequest {
		HTTPMethod method;
		std::string endpoint = "";
		std::string url = "";
		std::map<std::string, std::string> SESSION = {};
		std::map<std::string, std::string> GET = {};
		std::map<std::string, std::string> POST = {};
		std::map<std::string, std::string> COOKIES = {};
	};

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

	std::stringstream out;
	static std::string save = "";

	// Encode string into application/x-www-form-urlencoded
	std::string url_encode(const std::string& value) {
		std::ostringstream encoded;
		for (unsigned char c : value) {
			if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
				encoded << c;
			}
			else if (c == ' ') {
				encoded << '+';
			}
			else {
				encoded << '%' << std::uppercase << std::hex
					<< std::setw(2) << std::setfill('0') << int(c);
			}
		}
		return encoded.str();
	}

	// Decode application/x-www-form-urlencoded string
	std::string url_decode(const std::string& value) {
		std::ostringstream decoded;
		for (size_t i = 0; i < value.size(); ++i) {
			char c = value[i];
			if (c == '+') {
				decoded << ' ';
			}
			else if (c == '%' && i + 2 < value.size() &&
				std::isxdigit(value[i + 1]) && std::isxdigit(value[i + 2])) {
				std::string hex = value.substr(i + 1, 2);
				char decoded_char = static_cast<char>(std::stoi(hex, nullptr, 16));
				decoded << decoded_char;
				i += 2;
			}
			else {
				decoded << c;
			}
		}
		return decoded.str();
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

	void addHeader(std::string newHeader) {
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

	WSABUF HTTPResponse(int status_code, std::string content_type, HTTPRequest* req) {
		std::time_t t_date = time(nullptr);
		std::stringstream response("");

		for (auto& cookie : req->COOKIES) {
			header += "\nSet-Cookie: " + url_encode(cookie.first) + "=" + url_encode(cookie.second) + "; HttpOnly; SameSite=Strict; Path=/; Secure";
		}
		// write session data back to shared memory
		if (req->COOKIES.find("SESSIONID") != req->COOKIES.end()) {
#if defined(_WIN64) || defined(_WIN32)
			std::wstring fileName = L"IPM_SESSION_" + std::wstring(req->COOKIES["SESSIONID"].begin(), req->COOKIES["SESSIONID"].end());
			size_t fileSize = 0x1000;
			HANDLE hMapFile = NULL;
			char* buffer = NULL;
			int status = 0;

			hMapFile = CreateFileMappingW(
				INVALID_HANDLE_VALUE,
				NULL,
				PAGE_READWRITE,
				0,
				fileSize,
				fileName.c_str());

			if (hMapFile == NULL) {
				status = 0;
				throw std::runtime_error("Could not create file mapping object: " + std::to_string(GetLastError()));
			}

			buffer = reinterpret_cast<char*>(MapViewOfFile(hMapFile,
				FILE_MAP_ALL_ACCESS,
				0,
				0,
				fileSize));

			if (buffer == NULL)
			{
				CloseHandle(hMapFile);
				status = 0;
				throw std::runtime_error("Could not map view of file: " + std::to_string(GetLastError()));
			}

			std::stringstream ss; // session data is stored as key=value\nkey2=value2\n
			for (auto& cookie : req->SESSION) {
				ss << url_encode(cookie.first) << '=' << url_encode(cookie.second) << '\n';
			}
			std::string sessionData = ss.str();
			if (sessionData.length() > fileSize) {
				sessionData = sessionData.substr(0, fileSize);
			}
			CopyMemory(buffer, sessionData.c_str(), sessionData.length());
			UnmapViewOfFile(buffer);
			CloseHandle(hMapFile);
#else
#if defined(__linux__)
			std::string fileName = "/IPM_SESSION_" + req.COOKIES["SESSIONID"];
			size_t fileSize = 0x1000;
			int fd = shm_open(fileName.c_str(), O_CREAT | O_RDWR, 0666);
			char* buffer = reinterpret_cast<char*>(mmap(NULL, fileSize,
				PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
			std::stringstream ss;
			// session data is stored as key=value\nkey2=value2\n
			for (auto& cookie : req.SESSION) {
				ss << url_encode(cookie.first) << '=' << url_encode(cookie.second) << '\n';
			}
			std::string sessionData = ss.str();
			if (sessionData.length() > fileSize) {
				sessionData = sessionData.substr(0, fileSize);
			}
			memcpy(buffer, sessionData.c_str(), sessionData.length());
			munmap(buffer, fileSize);
			close(fd);
#else
#endif
#endif
		}

		tm date = {};
		gmtime_s(&date, &t_date);

		response << "HTTP/3 " << status_code << " " << status_code_str[status_code] << "\n"
			"Date : " << std::put_time(&date, (char*)"%a, %d %b %Y %T %Z") <<	
			"\nServer: " VERSION "\n"
			"Content-Type: " << content_type << "\n"
			"Content-Length:" << std::to_string(out.str().length() + 43 + head.length()) << '\n'
			<< header << "\r\n\r\n\r\n" << "<!DOCTYPE html><html><head>" << head << "</head>" <<
			out.str() << "</html>\r\n";

		out.str("");
		out.clear();

		// copy to WSABUF

		save = response.str();

		WSABUF buffer = { static_cast<ULONG>(save.size()), const_cast<CHAR*>(save.data())};

		return buffer;
	};

	WSABUF HTTP404(HTTPRequest* Request) {
		out.str("");
		out.clear();

		HTML(
			<html>
			<head>
			<title>404 Not Found</title>
			</head>
			<body>
			<center>
			<h1>404 Not Found</h1><br>
			c_rver / 1.0.0 (Windows)
			</center>
			</body>
			</html>
		);

		return HTTPResponse(404, "text/html", Request);
	}
}