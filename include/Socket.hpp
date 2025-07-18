#pragma once
#include "Session.hpp"
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

#define NUM_CLIENTS() sessions.size()

struct WebIPM {
	bool busy = false;
	bool requested = false;
	bool reached = false;
	bool partial = false;
	SOCKET connection = INVALID_SOCKET;
	char data[0x1000] = { 0 };
};

class alignas(8) HTTP_Server {
public:
	SOCKET ListenSocket;
	bool should_stop = false;
	std::string port = "";
	std::string dir = "";
	std::string temp = "";
	std::string key = "";
	std::string cert = "";

	HTTP_Server();

	bool start();
	void terminate();
};

int listener(HTTP_Server* s, Session* newClient);
void connectionListener(HTTP_Server* s);