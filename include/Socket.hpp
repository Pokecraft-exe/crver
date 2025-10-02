#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include <sys/types.h>
#include <iostream>
#include <vector>
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
#include <map>
#include <mutex>
#include <queue>
#include "Session.hpp"

#define NUM_CLIENTS() sessions.size()

#if defined(__linux__)

typedef unsigned long DWORD;
typedef unsigned long* LPDWORD;
typedef void* DWORD_PTR;

typedef struct {
        unsigned long len;
        char* buf;
} WSABUF;

#define INVALID_SOCKET (SOCKET)(~0)
#define SOCKET_ERROR (-1)
#define MSG_PARTIAL 0
#define SO_KEEPALIVE 9
#define SO_REUSEADDR 2
#define TCP_NODELAY 1
#define SOL_SOCKET 1

#define closesocket(x) close(x)
#define WSAGetLastError() errno
#define ZeroMemory(x, y) memset(x, 0, y)
#define CopyMemory(x, y, z) memcpy(x, y, z)
#define WSAaccept(a, b, c, d, e) accept(a, b, c)

#endif

extern std::queue<Session*> SocketQueue;
extern int sessionsNum;

class alignas(8) HTTP_Server {
public:
	SOCKET ListenSocket;
	bool should_stop = false;
	std::string port = "";
	std::string dir = "";
	std::string temp = "";
	std::string key = "";
	std::string cert = "";

	HTTP_Server() {};

	bool start();
	void terminate();
};

int listener(HTTP_Server* s, Session* newClient);
HTTPRequest InitRequest(std::string data, int maxsession);
void connectionListener(HTTP_Server* s);
