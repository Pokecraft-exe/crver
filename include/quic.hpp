#pragma once
#include "msquic/msquic.h"
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

extern const QUIC_API_TABLE* MsQuic;

_Function_class_(QUIC_LISTENER_CALLBACK)
long quic_listener(HQUIC Listener, void* Context, QUIC_LISTENER_EVENT* Event);

#define NUM_CLIENTS() sessions.size()
#define DEFAULT_BUFLEN 512

class HTTPS_Server {
public:
	SOCKET ListenSocket;
	bool should_stop = false;
	std::string port = "";
	std::string dir = "";
	std::string temp = "";
	std::string key = "";
	std::string cert = "";

	HTTPS_Server();

	bool start();
	void terminate();
};

_Function_class_(QUIC_CONNECTION_CALLBACK)
QUIC_STATUS QUIC_API handler(HQUIC Connection, Session* newClient, QUIC_CONNECTION_EVENT* Event);