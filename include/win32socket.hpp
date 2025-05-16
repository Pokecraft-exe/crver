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

struct WebIPM {
	bool busy;
	bool requested;
	bool reached;
	bool partial;
	SOCKET connexion;
	char data[0x1000];
};

#define NUM_CLIENTS() sessions.size()
#define DEFAULT_BUFLEN 512

class Session {
public:
	bool running = false;
	SOCKET connexion = INVALID_SOCKET;
	int dest_len = 0;
	size_t id = 0;
	_Field_size_bytes_(dest_len) struct sockaddr destination = { 0 };
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
	bool should_stop = false;
	std::string port = "";
	std::string dir = "";
	std::string temp = "";;

	Server();

	static void connexionListener(Server* s);

	static int listener(Server* s, Session newClient);

	bool start();
	void terminate();
};

int socketBounded(std::vector<Session>&, sockaddr);