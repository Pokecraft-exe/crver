#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "win32socket.hpp"
#include <ws2tcpip.h>
#include <mstcpip.h>  // Required for tcp_keepalive
#include "compress.hpp"
#include "ipm.hpp"
#include <fstream>
#include <array>
#include <codecvt>
#include <stdexcept>
#include <map>

#define HTTPResponse(DATE, CONTENT, LEN, SESSIONID, CONTENT_TYPE) \
				(char*)(std::string("HTTP/1.1 200 OK\nDate : ") + timeToString(DATE) +		\
				"\nServer: c_rver / 2.0.0 (Windows)\n"								\
				"Content-Type: " + CONTENT_TYPE + "\n"					\
				"Content-Length:" + std::to_string(LEN) + "\n"						\
				"Connection: keep-alive\n"											\
				"Referrer-Policy: strict-origin-when-cross-origin\n"				\
				"X-Content-Type-Options: nosniff\n"									\
				"Set-Cookie: Session-ID=" + std::to_string(SESSIONID) + "\n"		\
				"Feature-Policy: accelerometer 'none'; camera 'none'; geolocation 'none'; gyroscope 'none'; magnetometer 'none'; microphone 'none'; payment 'none'; usb 'none'\n" \
				+ std::string(CONTENT) + "\r\n").c_str()

#define HTTP404(DATE) \
						(char*)(std::string("HTTP/1.1 404 Not Found\nDate : ") + timeToString(date) + \
						"\nServer: c_rver / 2.0.0 (Windows)\n"\
						"Connection: keep-alive\n"\
						"Referrer-Policy: strict-origin-when-cross-origin\r\n"\
						"Content-Type: text/html\n"\
						"Content-Length: 148\n"\
						"X-Content-Type-Options: nosniff\r\n\r\n"\
						"<html><head><title>404 Not Found</title></head><body><center>"\
						"<h1>404 Not Found</h1></center><hr><center>c_rver/1.0.0 (Windows)"\
						"</center></body></html>\r\n").c_str()

extern std::map<std::string, std::string> urls;
extern std::map<std::string, std::string> endpoints;
extern std::map<std::string, std::string> extentions;
extern std::map<std::string, std::string> cache;
extern std::map<std::string, interProcessMemory<WebIPM>> ipms;

std::vector<Session> sessions = {};

/*
 * Constructor and operator for Session to act as the socket for simplicity
 */
Session::Session(SOCKET sock) { connexion = sock; dest_len = sizeof(sockaddr); }
Session::Session(const Session& sess) { connexion = sess.connexion; dest_len = sess.dest_len; id = sess.id; destination = sess.destination; }
Session::operator SOCKET() { return connexion; }
Session::~Session() { }

/*
 * Constructor for Server
 */
Server::Server(void) {};

inline std::string timeToString(const std::tm* timeObj) {
	std::ostringstream oss;
	oss << std::put_time(timeObj, (char*)"%a, %d %b %Y %T %Z");
	return oss.str();
}

/*
 * Returns all the content of a file 
 */
inline bool readFile(const std::filesystem::path& filename, Session s, bool isUpload) {
	std::ifstream file(filename, std::ios::binary);
	std::array<char, 0x1000> contents;
	unsigned long bytesSent = 0;
	WSABUF buffer = { 0x1000, contents.data() };
	std::time_t t_date = time(nullptr);
	std::tm* date = gmtime(&t_date);
	std::string header = "";

	// Check if the file was opened successfully
	if (!file.is_open()) {
		std::cerr << "Error opening file: " << filename << std::endl;
		return false;
	}

	// Get the file's size
	file.seekg(0, std::ios::end);
	std::streampos fileSize = file.tellg();
	file.seekg(0, std::ios::beg);
	
	// Send the header
	if (isUpload) {
		header = "HTTP/1.1 200 OK\nDate : " + timeToString(date) +				\
			"\nServer: c_rver / 2.0.0 (Windows)\n"								\
			"Content-Disposition: attachment; filename=\"" + filename.filename().string() + "\"\n"\
			"Content-Length:" + std::to_string(fileSize) + "\n"					\
			"Content-Transfer-Encoding: binary"									\
			"Connection: keep-alive\n"											\
			"Referrer-Policy: strict-origin-when-cross-origin\n"				\
			"X-Content-Type-Options: nosniff\n"									\
			"Set-Cookie: Session-ID=" + std::to_string(s.id) + "\r\n\r\n";
		ZeroMemory(buffer.buf, 0x1000);
		memcpy(buffer.buf, header.c_str(), header.length());
	}
	else {
		header = "HTTP/1.1 200 OK\nDate : " + timeToString(date) +				\
			"\nServer: c_rver / 2.0.0 (Windows)\n"								\
			"Content-Length:" + std::to_string(fileSize) + "\n"					\
			"Content-Type: " + extentions[filename.extension().string()] + "\n"	\
			"Connection: keep-alive\n"											\
			"Referrer-Policy: strict-origin-when-cross-origin\n"				\
			"X-Content-Type-Options: nosniff\n"									\
			"Set-Cookie: Session-ID=" + std::to_string(s.id) + "\r\n\r\n";
		ZeroMemory(buffer.buf, 0x1000);
		memcpy(buffer.buf, header.c_str(), header.length());
	}
 
	buffer.len = header.size();
	WSASend(s, &buffer, 1, &bytesSent, MSG_DONTROUTE | MSG_PARTIAL, NULL, NULL);
	if (bytesSent == 0) {
		std::cerr << "Error sending header: " << WSAGetLastError() << std::endl;
		ZeroMemory(buffer.buf, 0x1000);
		return false;
	}
	buffer.len = 0x1000;
	// Clear contents after sending
	ZeroMemory(buffer.buf, 0x1000);

	// Read the file 1kb by 1kb
	while (file) {
		file.read(buffer.buf, buffer.len);	
		WSASend(s, &buffer, 1, &bytesSent, MSG_DONTROUTE | MSG_PARTIAL, NULL, NULL);
		if (bytesSent == 0) {
			std::cerr << "Error sending file: " << WSAGetLastError() << std::endl;
			ZeroMemory(buffer.buf, 0x1000);
			return false;
		}
		ZeroMemory(buffer.buf, 0x1000);
		// Check if the end of the file has been reached
		if (file.gcount() == 0)	break;
	}

	// Close the file
	file.close();
	return true;
}

/*
 * Execute a program with the session and the request
 */
inline bool executeAndDump(std::string cmd, Session* session, char* request) {
	FILE* fp;
	WSABUF buffer = { 0x1000, NULL };
	std::string sSize = std::to_string(sizeof(Session)) + "\n";
	unsigned long bytesSent = 0;

	interProcessMemory<WebIPM>* ipm = &ipms[cmd];

	while (ipm->busy) {};

	ipm->busy = true;
	ipm->requested = true;
	ipm->reached = false;
	ipm->partial = false;
	CopyMemory(ipm->data, request, sizeof(Session));
	ipm->Global().update();

	// wait for the process to finish
	while (!ipm->reached) {
		ipm->Local().update();
	}

	buffer.buf = ipm->Direct()->data;

	WSASend(session->connexion, &buffer, 1, &bytesSent, MSG_DONTROUTE | MSG_PARTIAL, NULL, NULL);
	if (bytesSent == 0) {
		std::cerr << "Error sending file: " << WSAGetLastError() << std::endl;
		ZeroMemory(buffer.buf, 0x1000);
		return false;
	}
	ZeroMemory(buffer.buf, 0x1000);
	
	ipm->busy = false;
	ipm->reached = false;
	ipm->Global().update();

	return true;
}

void Server::connexionListener(Server* s) {
	std::vector<std::thread> Listeners = {};

	std::cout << "Listening for incoming connexions...\n";

	while (1 && !s->should_stop) {
		if (listen(s->ListenSocket, SOMAXCONN) == SOCKET_ERROR) {
			printf("Listen failed with error: %ld\n", WSAGetLastError());
			closesocket(s->ListenSocket);
			s->terminate();
			break;
		}

		// Accept a client socket
		Session newClient = INVALID_SOCKET;
		newClient.connexion = WSAAccept(s->ListenSocket, &newClient.destination, &newClient.dest_len, nullptr,(DWORD_PTR) nullptr);
		if (newClient == INVALID_SOCKET) {
			printf("accept failed: %d\n", WSAGetLastError());
			closesocket(s->ListenSocket);
			s->terminate();
			break;
		}
		else {
			int optval = 1;
			setsockopt(newClient, IPPROTO_TCP, TCP_NODELAY, (char*)&optval, sizeof(optval));
			setsockopt(newClient, SOL_SOCKET, SO_KEEPALIVE, (char*)&optval, sizeof(optval));
			setsockopt(newClient, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval));
			struct tcp_keepalive ka;
			ka.onoff = 1;
			ka.keepalivetime = 30000;  // Time before sending first keep-alive probe (30s)
			ka.keepaliveinterval = 10000; // Interval between probes (10s)

			DWORD dwBytesReturned;
			WSAIoctl(newClient, SIO_KEEPALIVE_VALS, &ka, sizeof(ka), nullptr, 0, &dwBytesReturned, nullptr, nullptr);
			//InetNtop(s->hints.ai_family, &newClient.destination, s->ws_addr, 100);

			//listener(s, newClient);

			newClient.running = true;
			std::thread t = std::thread(listener, s, newClient);
			t.detach();
		}
	}
	return;
}

int Server::listener(Server* s, Session newClient) {
	int status = 1;
	Session CurrentSession = newClient;
	size_t clientNumber = 0;
	WSABUF buffer = { DEFAULT_BUFLEN, new char[0x1000]};

	while (status > 0 && !s->should_stop) {

		status = recv(CurrentSession, buffer.buf, buffer.len, 0);
		if (status < 0)
			wprintf(L"\trecv failed with error: %d\n", WSAGetLastError());

		std::time_t t_date = time(nullptr);
		std::tm* date = gmtime(&t_date);
		std::string page;
		
		{
			std::stringstream r(buffer.buf);
			std::string junk;
			r >> junk >> junk;
			r >> page;
			r = std::stringstream(junk);
			r >> page;
			page = page.substr(0, page.find('?'));
		}

		std::filesystem::path file;

		clientNumber = NUM_CLIENTS();
		//sessions.push_back(newClient);

		// 404 ?
		if (urls.find(page) != urls.end())
		{
			file = s->dir + "." + urls[page];;
			if (file.extension().string().compare(".exe") == 0) {
				if (endpoints[page].compare("throw") == 0) {
					bool sent = executeAndDump(urls[page], &CurrentSession, buffer.buf);
				}
				else {
					if (extentions.find(file.extension().string()) != extentions.end()) {
						bool sent = readFile(file.string(), CurrentSession, true);
						if (!sent) {
							std::cerr << "Error sending file: " << file.string() << std::endl;
							break;
						}
					}
				}
			}
			else {
				bool sent = readFile(file.string(), CurrentSession, false);
				if (!sent) {
					std::cerr << "Error sending file: " << file.string() << std::endl;
					break;
				}
			}
		}
		else 
		{
			std::string s = "";

			if (!file.extension().string().compare(".exe") ||
				!file.extension().string().compare(".html")) {
				s = HTTP404(date);
			}
			else {
				s = "HTTP/1.1 404 Not Found\nDate : " + timeToString(date) +
					"\nServer: c_rver / 2.0.0 (Windows)\n"
					"Connection: keep-alive\n"
					"Referrer-Policy: strict-origin-when-cross-origin\r\n"
					"Content-Type: text/html\n"
					"Content-Length: 0\n"
					"X-Content-Type-Options: nosniff\r\n\r\n";
			}

			WSABUF buffer = { s.length(), s.data() };
			unsigned long bytesSent = 0;
			WSASend(CurrentSession, &buffer, 1, &bytesSent, MSG_DONTROUTE, NULL, NULL);
			if (bytesSent == 0) {
				std::cerr << "Error sending header: " << WSAGetLastError() << std::endl;
				ZeroMemory(buffer.buf, buffer.len);
				break;
			}
		}
	};

	//std::cout << "Listener closed" << std::endl;
	//sessions[clientNumber].running = false;
	closesocket(CurrentSession);
	//sessions.erase(sessions.begin() + clientNumber-1);
	delete[] buffer.buf;

	return 0;
}

bool Server::start(void) {
	WSAStartup(MAKEWORD(2, 2), &dat);
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	if (getaddrinfo(NULL, port.c_str(), &hints, &result) != 0) {
		std::cout << "Failed to start socket.." << std::endl;
		return 1;
	}

	// Create a SOCKET for the server to listen for client connections
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET) {
		std::cout << "Error at socket(): " << WSAGetLastError() << std::endl;
		freeaddrinfo(result);
		return 1;
	}

	// Setup the TCP listening socket
	if (bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
		std::cout << "bind failed with error: " << WSAGetLastError() << std::endl;
		freeaddrinfo(result);
		closesocket(ListenSocket);
		terminate();
		return 1;
	}


	int optval = 1;
	setsockopt(ListenSocket, IPPROTO_TCP, TCP_NODELAY, (char*)&optval, sizeof(optval));
	setsockopt(ListenSocket, SOL_SOCKET, SO_KEEPALIVE, (char*)&optval, sizeof(optval));
	setsockopt(ListenSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval));

	hints.ai_addr = result->ai_addr;
	hints.ai_addrlen = result->ai_addrlen;

	freeaddrinfo(result);

	std::thread AcceptThread = std::thread(&connexionListener, this);
	AcceptThread.join();

	return 0;
}

void Server::terminate(void) {
	bool isOneClientRunning = false;
	should_stop = true;

	// wait for all threads to finish
	for (auto& client : sessions) {
		if (client.running) {
			closesocket(client);
		}
	}

	// verify if the clients are still running
	while (isOneClientRunning) {
		isOneClientRunning = false;
		for (auto& client : sessions) {
			if (client.running) {
				isOneClientRunning = true;
				break;
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	// close the listening socket
	if (ListenSocket != INVALID_SOCKET) {
		closesocket(ListenSocket);
		ListenSocket = INVALID_SOCKET;
	}

	sessions.erase(sessions.begin(), sessions.end());
	cache.erase(cache.begin(), cache.end());

	WSACleanup();
	return;
}