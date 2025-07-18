#pragma once
#ifdef __linux__
typedef unsigned long long int SOCKET;
#endif

#include <msquic/msquic.h>

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
