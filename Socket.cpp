#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "Socket.hpp"
#include "compress.hpp"
#include "sites-includes/crver.hpp"

constexpr size_t BUFFER_SIZE = 0x1000;
constexpr DWORD KEEPALIVE_INTERVAL_MS = 10000;  // 10 seconds

#if defined(__linux__)

#define LOG_FILENAME "/var/log/crver/all.log"
inline int WSARecv(SOCKET s, WSABUF* lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, void* null1, void* null2) {
	if (dwBufferCount != 1) return SOCKET_ERROR;
	int result = recv(s, lpBuffers[0].buf, lpBuffers[0].len, lpFlags ? *lpFlags : 0);
	if (result >= 0 && lpNumberOfBytesRecvd) *lpNumberOfBytesRecvd = result;
	return result >= 0 ? 0 : SOCKET_ERROR;
}

inline int WSASend(SOCKET s, WSABUF* lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, void* null1, void* null2) {
	if (dwBufferCount != 1) return SOCKET_ERROR;
	int result = send(s, lpBuffers[0].buf, lpBuffers[0].len, dwFlags);
	if (result >= 0 && lpNumberOfBytesSent) *lpNumberOfBytesSent = result;
	return result >= 0 ? 0 : SOCKET_ERROR;
}

#else

#define LOG_FILENAME "all.log"

#endif

extern std::map<std::string, std::string> urls;
extern std::map<std::string, endpoint_type> endpoints;
extern std::map<std::string, std::string> extentions;
extern std::map<std::string, std::string> cache;
extern std::map<std::string, void*> webMains;

std::vector<Session> sessions = {};
std::ofstream logFile;
std::mutex logMutex;

inline void log(std::string msg) {
	while (logMutex.try_lock()) {
		time_t t_date = time(nullptr);
		std::tm* date = gmtime(&t_date);
		logFile << std::put_time(date, "%Y-%m-%d %H:%M:%S") << " : " << msg << std::endl;
		logMutex.unlock();
		return;
	}
	return;
}

inline std::string timeToString(const std::tm* timeObj) {
	std::ostringstream oss;
	oss << std::put_time(timeObj, (char*)"%a, %d %b %Y %T %Z");
	return oss.str();
}

inline void getPage(std::string& page, const WSABUF& buffer) {
	if (!strcmp(buffer.buf, "")) return;

	// cstyle cuz need to b fast
	int end = 0;
	int start = 0;
	for (end; end < buffer.len; end++) {
		if (buffer.buf[end] == ' ' &&
			buffer.buf[end + 1] == 'H' &&
			buffer.buf[end + 2] == 'T' &&
			buffer.buf[end + 3] == 'T' &&
			buffer.buf[end + 4] == 'P' &&
			buffer.buf[end + 5] == '/')
			break;
		if (buffer.buf[end] == '?')
			break;
	}

	for (start; start < end; start++) {
		if (buffer.buf[start] == ' ') break;
	}

	start++;

	page.resize(end - start);

	for (int i = start; i <= end; i++) page[i - start] = buffer.buf[i];
}

/*
 * Returns all the content of a file
 */
inline bool readFile(const std::filesystem::path& filename, Session s, bool isUpload) {
	std::ifstream file(filename, std::ios::binary);
	std::array<char, BUFFER_SIZE> contents;
	unsigned long bytesSent = 0;
	WSABUF buffer = { BUFFER_SIZE, contents.data() };
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
		header = "HTTP/1.1 200 OK\nDate : " + timeToString(date) + \
			"\nHTTP_Server: c_rver / 2.0.0 (Windows)\n"								\
			"Content-Disposition: attachment; filename=\"" + filename.filename().string() + "\"\n"\
			"Content-Length:" + std::to_string(fileSize) + "\n"					\
			"Content-Transfer-Encoding: binary\n"								\
			"Connection: keep-alive\r\n\r\n";
		ZeroMemory(buffer.buf, BUFFER_SIZE);
		if (header.length() > BUFFER_SIZE) {
			log("Header too long, truncating to fit buffer size.");
			log(buffer.buf);
			memcpy(buffer.buf, header.c_str(), BUFFER_SIZE);
		}
		memcpy(buffer.buf, header.c_str(), header.length());
	}
	else {
		header = "HTTP/1.1 200 OK\nDate : " + timeToString(date) + \
			"\nHTTP_Server: c_rver / 2.0.0 (Windows)\n"								\
			"Content-Length:" + std::to_string(fileSize) + "\n"					\
			"Content-Type: " + extentions[filename.extension().string()] + "\n"	\
			"Connection: keep-alive\r\n\r\n";
		ZeroMemory(buffer.buf, BUFFER_SIZE);
		if (header.length() > BUFFER_SIZE) {
			log("Header too long, truncating to fit buffer size.");
			log(buffer.buf);
			memcpy(buffer.buf, header.c_str(), BUFFER_SIZE);
		}
		else
			memcpy(buffer.buf, header.c_str(), header.length());
	}

	buffer.len = header.size();
	WSASend(s, &buffer, 1, &bytesSent, MSG_PARTIAL, NULL, NULL);
	int error = WSAGetLastError();
	if (bytesSent == 0 && error != 10053) {
		std::cerr << "Error sending header: " << WSAGetLastError() << std::endl;
		ZeroMemory(buffer.buf, BUFFER_SIZE);
		return false;
	}
	buffer.len = BUFFER_SIZE;
	// Clear contents after sending
	ZeroMemory(buffer.buf, BUFFER_SIZE);

	// Read the file 1kb by 1kb
	while (file) {
		file.read(buffer.buf, buffer.len);
		WSASend(s, &buffer, 1, &bytesSent, 0, NULL, NULL);
		if (bytesSent == 0) {
			std::cerr << "Error sending file: " << WSAGetLastError() << std::endl;
			ZeroMemory(buffer.buf, BUFFER_SIZE);
			return false;
		}
		ZeroMemory(buffer.buf, BUFFER_SIZE);
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
inline bool executeAndDump(std::string dll, Session* session, char* request) {
	unsigned long bytesSent = 0;

	crver::HTTPRequest req = crver::InitRequest(request, Sessions.size());
	WSABUF buffer = reinterpret_cast<WSABUF(*)(crver::HTTPRequest*)>(webMains[dll])(&req);

	WSASend(session->NEW_CONNECTION.http.Connection, &buffer, 1, &bytesSent, 0, NULL, NULL);
	if (bytesSent == 0) {
		std::cerr << "Error sending file: " << WSAGetLastError() << std::endl;
		return false;
	}

	return true;
}

void connectionListener(HTTP_Server* s) {
	printf("Listening for incoming connections...\n");

	while (!s->should_stop) {
		Session* newClient = new Session(INVALID_SOCKET);
		newClient->NEW_CONNECTION.http.Connection = WSAAccept(s->ListenSocket, &newClient->NEW_CONNECTION.http.Destination, &newClient->NEW_CONNECTION.http.Dest_len, nullptr, (DWORD_PTR) nullptr);
		if (newClient->NEW_CONNECTION.http.Connection == INVALID_SOCKET) {
			printf("accept failed: %d\n", WSAGetLastError());
			closesocket(s->ListenSocket);
			s->terminate();
		}
		newClient->active = true;
		std::thread t = std::thread(listener, s, newClient);
		t.detach();
	}
	return;
}

int listener(HTTP_Server* s, Session* CurrentSession) {
	int status = 1;
	size_t clientNumber = 0;
	WSABUF buffer = { BUFFER_SIZE, new char[BUFFER_SIZE] };

#if defined(_WIN64) || defined(_WIN32)
	int optval = 1;
	setsockopt(CurrentSession->NEW_CONNECTION.http.Connection, IPPROTO_TCP, TCP_NODELAY, (char*)&optval, sizeof(optval));
	setsockopt(CurrentSession->NEW_CONNECTION.http.Connection, SOL_SOCKET, SO_KEEPALIVE, (char*)&optval, sizeof(optval));
	setsockopt(CurrentSession->NEW_CONNECTION.http.Connection, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval));

	int cur = 0; int curlen = sizeof(cur);
	if (getsockopt(CurrentSession->NEW_CONNECTION.http.Connection, IPPROTO_TCP, TCP_NODELAY, (char*)&cur, &curlen) == 0) {
		if (cur != 1) {
			printf("WARN: TCP_NODELAY not set on accepted socket!\n");
		}
	}

	struct tcp_keepalive ka;
	ka.onoff = 1;
	ka.keepalivetime = 30000;
	ka.keepaliveinterval = KEEPALIVE_INTERVAL_MS;
	DWORD dwBytesReturned;
	WSAIoctl(CurrentSession->NEW_CONNECTION.http.Connection, SIO_KEEPALIVE_VALS,
		&ka, sizeof(ka), nullptr, 0, &dwBytesReturned, nullptr, nullptr);
#endif

	while (status > 0 && !s->should_stop) {
		ZeroMemory(buffer.buf, BUFFER_SIZE);
		status = recv(CurrentSession->NEW_CONNECTION.http.Connection, buffer.buf, buffer.len, 0);
		if (status < 0)
			wprintf(L"\trecv failed with error: %d\n", WSAGetLastError());

		std::time_t t_date = time(nullptr);
		std::tm* date = gmtime(&t_date);
		std::string page;

		getPage(page, buffer);

		std::filesystem::path file;

		clientNumber = NUM_CLIENTS();

		// 404 ?
		if (urls.find(page) != urls.end())
		{
			bool sent = false;
			endpoint_type& endpoint = endpoints[page];
			std::string& url = urls[page];
			file = s->dir + "." + url;
			
			if (endpoint == endpoint_type::EXECUTABLE)
				sent = executeAndDump(url, CurrentSession, buffer.buf);
			else
				sent = readFile(file.string(), *CurrentSession, endpoint == endpoint_type::UPLOAD);

			if (!sent) {
				std::cerr << "Error sending file: " << file.string() << std::endl;
				break;
			}
		} else {
			std::string s = "";
			file = page;
			if (endpoints[page] == endpoint_type::EXECUTABLE ||
				!file.extension().string().compare(".html"))
				s = "HTTP/1.1 404 Not Found\nDate : " + timeToString(date) +
					"\nHTTP_Server: c_rver / 2.0.0 (Windows)\n"
					"Connection: keep-alive\n"
					"Referrer-Policy: strict-origin-when-cross-origin\r\n"
					"Content-Type: text/html\n"
					"Content-Length: 148\n"
					"X-Content-Type-Options: nosniff\r\n\r\n"
					"<html><head><title>404 Not Found</title></head><body><center>"
					"<h1>404 Not Found</h1></center><hr><center>c_rver/1.0.0 (Windows)"
					"</center></body></html>\r\n\r\n";
			else
				s = "HTTP/1.1 404 Not Found\nDate : " + timeToString(date) +
					"\nHTTP_Server: c_rver / 2.0.0 (Windows)\n"
					"Connection: keep-alive\n"
					"Referrer-Policy: strict-origin-when-cross-origin\r\n"
					"Content-Type: text/html\n"
					"Content-Length: 0\n"
					"X-Content-Type-Options: nosniff\r\n\r\n";

			WSABUF toSendBuffer = { (ULONG)s.length(), s.data() };
			unsigned long bytesSent = 0;
			WSASend(CurrentSession->NEW_CONNECTION.http.Connection, &toSendBuffer, 1, &bytesSent, MSG_DONTROUTE, NULL, NULL);
			int error = WSAGetLastError();
			if (bytesSent == 0 && error != 10053) {
				std::cerr << "Error sending header: " << error << std::endl;
				ZeroMemory(buffer.buf, buffer.len);
				break;
			}
		}
	};

	closesocket(*CurrentSession);
	delete CurrentSession;
	delete[] buffer.buf;

	return 0;
}

bool HTTP_Server::start(void) {
	logFile = std::ofstream(LOG_FILENAME);
	if (!logFile.is_open())
	{
	    std::cerr << "Failed to open log file (" << LOG_FILENAME << ')' << std::endl;
	    std::terminate();
	}

#if defined(__linux__)
	sockaddr_in server_address_in;
	server_address_in.sin_family = AF_INET;

	server_address_in.sin_addr.s_addr = htonl(INADDR_ANY);

	int iport = atoi(port.c_str());
	server_address_in.sin_port = htons(iport);

	this->ListenSocket = socket(AF_INET, SOCK_STREAM, 0);

	bind(this->ListenSocket, (sockaddr*)&server_address_in, sizeof(server_address_in));
#else
	WSADATA dat;
	addrinfo hints, * result = NULL;

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

	// Create a SOCKET for the HTTP_Server to listen for client connections
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
#endif

	if (listen(this->ListenSocket, SOMAXCONN) == SOCKET_ERROR) {
		closesocket(this->ListenSocket);
		terminate();
		return 1;
	}

	connectionListener(this);

	return 0;
}

void HTTP_Server::terminate(void) {
	bool isOneClientRunning = false;
	should_stop = true;

	// wait for all threads to finish
	for (auto& client : sessions) {
		if (client.active) {
			closesocket(client);
		}
	}

	// verify if the clients are still running
	while (isOneClientRunning) {
		isOneClientRunning = false;
		for (auto& client : sessions) {
			if (client.active) {
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

#if defined(_WIN64) || defined(_WIN32)
	WSACleanup();
#endif

	log("Successfully terminated!");
	logFile.close();
	return;
}

//1.	Use thread pools instead of creating new threads for each connection
//2.	Add RAII for socket management and proper exception handling
