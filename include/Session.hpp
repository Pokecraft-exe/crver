#pragma once
#if defined(__inux__) 
typedef unsigned long long int SOCKET;
#endif

#if defined(_WIN32) || defined(_WIN64)
#define _CRT_SECURE_NO_WARNINGS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include <map>
#include <msquic/msquic.h>

enum class endpoint_type {
    NONE = 0,
    THROW,
    UPLOAD,
    EXECUTABLE,
};

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

class alignas(8) Session {
public:
    union sock {
        struct httpsock {
            sockaddr Destination;
            socklen_t Dest_len;
            SOCKET Connection;
        } http;
        struct httpssock {
            QUIC_NEW_CONNECTION_INFO Info;
            HQUIC Connection;
            QUIC_TLS_SECRETS Secret;
        } quic;
    } NEW_CONNECTION;
    bool active = false;
    QUIC_BUFFER* buf = nullptr;
    Session(SOCKET socket);
    Session(const Session& sess);
    operator SOCKET();
};