#pragma once
#include <sys/types.h>
#include <WinSock2.h>
#include <ws2tcpip.h>
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

#define NUM_CLIENTS() s->ClientSessions.size()
#define DEFAULT_BUFLEN 512

class Session {
public:
	SOCKET connexion = INVALID_SOCKET;
	int dest_len = 0;
	size_t id = 0;
	_Field_size_bytes_(dest_len) struct sockaddr destination = { 0 };
	std::string cache = "";
	Session(SOCKET);
	Session(const Session&);
	operator SOCKET();
	~Session();
};

class Server {
private:
	WSADATA dat = { 0 };
	struct addrinfo* result = NULL, * ptr = NULL, hints;
	SOCKET ListenSocket = INVALID_SOCKET;
	char recvbuf[DEFAULT_BUFLEN];
	int iResult, iSendResult;
	int recvbuflen = DEFAULT_BUFLEN;
	WCHAR ws_addr[100] = { 0 };
public:
	char* port = 0;
	std::string dir = "";
	std::string temp = "";
	std::vector<Session> ClientSessions = {};

	Server();

	static void connexionListener(Server* s);

	static int listener(Server* s, Session newClient);

	bool start();
	void terminate();
};

int socketBounded(std::vector<Session>&, sockaddr);