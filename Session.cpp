#include <cstring>
#include <vector>
#include <map>
#include <string>
#include "Session.hpp"

std::vector<std::map<std::string, std::string>> Sessions = {};

Session::Session(SOCKET socket) : active(false), buf(nullptr) {
    // Initialize the http_connection part of the union
    memset(&NEW_CONNECTION, 0, sizeof(NEW_CONNECTION));
    NEW_CONNECTION.http.Connection = socket;
    NEW_CONNECTION.http.Dest_len = sizeof(sockaddr);
    memset(&NEW_CONNECTION.http.Destination, 0, sizeof(sockaddr));
}

Session::Session(const Session& sess) : active(sess.active) {
    // Copy the entire union
    memcpy(&NEW_CONNECTION, &sess.NEW_CONNECTION, sizeof(NEW_CONNECTION));
    
    // Handle buffer copying if it exists
    if (sess.buf != nullptr) {
        buf = new QUIC_BUFFER();
        buf->Length = sess.buf->Length;
        if (sess.buf->Buffer != nullptr && sess.buf->Length > 0) {
            buf->Buffer = new uint8_t[sess.buf->Length];
            memcpy(buf->Buffer, sess.buf->Buffer, sess.buf->Length);
        } else {
            buf->Buffer = nullptr;
        }
    } else {
        buf = nullptr;
    }
}

Session::operator SOCKET() {
    return NEW_CONNECTION.http.Connection;
}