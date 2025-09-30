#pragma once
#ifdef __linux__
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

extern std::vector<std::map<std::string, std::string>> Sessions;

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
