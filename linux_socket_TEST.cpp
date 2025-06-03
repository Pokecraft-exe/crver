#pragma once

#ifdef __linux__

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <csignal>
#include <cstring>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <thread>
#include <vector>
#include <map>
#include <chrono>
#include <ctime>
#include <sstream>

#include "compress.hpp"
#include "ipm.hpp" // Reuse the portable IPM from previous step

#define DEFAULT_BUFLEN 4096

extern std::map<std::string, std::string> urls;
extern std::map<std::string, std::string> endpoints;
extern std::map<std::string, std::string> extentions;
extern std::map<std::string, interProcessMemory<WebIPM>> ipms;

struct Session {
    int sockfd;
    sockaddr_storage destination;
    socklen_t dest_len;
    size_t id;
    bool running;

    Session() = default;
    Session(int fd) : sockfd(fd), dest_len(sizeof(destination)), running(true) {}
    Session(const Session& s) = default;
    operator int() const { return sockfd; }
    ~Session() = default;
};

inline std::string timeToString(const std::tm* timeObj) {
    std::ostringstream oss;
    oss << std::put_time(timeObj, "%a, %d %b %Y %T %Z");
    return oss.str();
}

inline bool readFile(const std::filesystem::path& filename, Session& s, bool isUpload) {
    std::ifstream file(filename, std::ios::binary);
    std::vector<char> buffer(DEFAULT_BUFLEN);
    std::time_t t_date = time(nullptr);
    std::tm* date = gmtime(&t_date);

    if (!file.is_open()) return false;

    file.seekg(0, std::ios::end);
    std::streamsize fileSize = file.tellg();
    file.seekg(0);

    std::ostringstream header;
    if (isUpload) {
        header << "HTTP/1.1 200 OK\nDate: " << timeToString(date)
               << "\nServer: c_rver / 2.0.0 (Linux)\n"
               << "Content-Disposition: attachment; filename=\"" << filename.filename().string() << "\"\n"
               << "Content-Length: " << fileSize << "\n"
               << "Connection: keep-alive\n\r\n";
    } else {
        header << "HTTP/1.1 200 OK\nDate: " << timeToString(date)
               << "\nServer: c_rver / 2.0.0 (Linux)\n"
               << "Content-Length: " << fileSize << "\n"
               << "Content-Type: " << extentions[filename.extension().string()] << "\n"
               << "Connection: keep-alive\n\r\n";
    }

    send(s.sockfd, header.str().c_str(), header.str().size(), 0);

    while (file.read(buffer.data(), buffer.size()) || file.gcount()) {
        send(s.sockfd, buffer.data(), file.gcount(), 0);
    }
    return true;
}

inline bool executeAndDump(std::string cmd, Session* session, char* request) {
    interProcessMemory<WebIPM>* ipm = &ipms[cmd];
    while (ipm->busy) {};

    ipm->busy = true;
    ipm->requested = true;
    ipm->reached = false;
    ipm->partial = false;

    memcpy(ipm->data, request, sizeof(Session));
    ipm->Global().update();

    while (!ipm->reached) {
        ipm->Local().update();
    }

    send(session->sockfd, ipm->Direct()->data, DEFAULT_BUFLEN, 0);

    ipm->busy = false;
    ipm->reached = false;
    ipm->Global().update();

    return true;
}

class Server {
public:
    int sockfd;
    sockaddr_in addr;
    std::string port = "8080";
    std::string dir = "./";
    bool should_stop = false;

    std::vector<Session> sessions;

    void start() {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        int opt = 1;
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(std::stoi(port));

        bind(sockfd, (struct sockaddr*)&addr, sizeof(addr));
        listen(sockfd, SOMAXCONN);

        std::cout << "Listening on port " << port << "...\n";
        while (!should_stop) {
            Session client;
            client.sockfd = accept(sockfd, (sockaddr*)&client.destination, &client.dest_len);
            if (client.sockfd < 0) continue;

            std::thread(&Server::listener, this, client).detach();
        }
    }

    void listener(Session client) {
        std::vector<char> buffer(DEFAULT_BUFLEN);

        ssize_t recv_len = recv(client.sockfd, buffer.data(), buffer.size(), 0);
        if (recv_len <= 0) return;

        std::stringstream ss(std::string(buffer.data(), recv_len));
        std::string method, uri, version;
        ss >> method >> uri >> version;
        uri = uri.substr(0, uri.find('?'));

        std::filesystem::path file;

        if (urls.find(uri) != urls.end()) {
            file = dir + "." + urls[uri];
            if (file.extension() == ".exe") {
                if (endpoints[uri] == "throw") {
                    executeAndDump(urls[uri], &client, buffer.data());
                } else {
                    readFile(file, client, true);
                }
            } else {
                readFile(file, client, false);
            }
        } else {
            std::string notfound =
                "HTTP/1.1 404 Not Found\n"
                "Content-Length: 0\n"
                "Connection: close\n\r\n";
            send(client.sockfd, notfound.c_str(), notfound.size(), 0);
        }

        close(client.sockfd);
    }
};

#endif // __linux__
