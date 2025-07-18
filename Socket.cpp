#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "Socket.hpp"
#include "compress.hpp"
#include "ipm.hpp"

constexpr size_t BUFFER_SIZE = 0x1000;
constexpr DWORD KEEPALIVE_INTERVAL_MS = 10000;  // 10 seconds

#if defined(__linux__)
typedef unsigned long DWORD;
typedef unsigned long* LPDWORD;
typedef void* DWORD_PTR;
typedef int socklen_t;

typedef struct {
	unsigned long len;
	char* buf;
} WSABUF;

#define INVALID_SOCKET (SOCKET)(~0)
#define SOCKET_ERROR (-1)
#define SOMAXCONN 128
#define MSG_DONTROUTE 0
#define MSG_PARTIAL 0
#define SO_KEEPALIVE 9
#define SO_REUSEADDR 2
#define TCP_NODELAY 1
#define IPPROTO_TCP 6
#define SOL_SOCKET 1

#define closesocket(x) close(x)
#define WSAGetLastError() errno()

inline int WSARecv(SOCKET s, WSABUF* lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags) {
	if (dwBufferCount != 1) return SOCKET_ERROR;
	int result = recv(s, lpBuffers[0].buf, lpBuffers[0].len, lpFlags ? *lpFlags : 0);
	if (result >= 0 && lpNumberOfBytesRecvd) *lpNumberOfBytesRecvd = result;
	return result >= 0 ? 0 : SOCKET_ERROR;
}

inline int WSASend(SOCKET s, WSABUF* lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags) {
	if (dwBufferCount != 1) return SOCKET_ERROR;
	int result = send(s, lpBuffers[0].buf, lpBuffers[0].len, dwFlags);
	if (result >= 0 && lpNumberOfBytesSent) *lpNumberOfBytesSent = result;
	return result >= 0 ? 0 : SOCKET_ERROR;
}
#endif

extern std::map<std::string, std::string> urls;
extern std::map<std::string, std::string> endpoints;
extern std::map<std::string, std::string> extentions;
extern std::map<std::string, std::string> cache;
extern std::map<std::string, interProcessMemory<WebIPM>> ipms;

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

/*
 * Constructor for HTTP_Server
 */
HTTP_Server::HTTP_Server(void) {}

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
	WSASend(s, &buffer, 1, &bytesSent, MSG_DONTROUTE | MSG_PARTIAL, NULL, NULL);
	if (bytesSent == 0) {
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
		WSASend(s, &buffer, 1, &bytesSent, MSG_DONTROUTE | MSG_PARTIAL, NULL, NULL);
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
inline bool executeAndDump(std::string cmd, Session* session, char* request) {
	FILE* fp;
	WSABUF buffer = { BUFFER_SIZE, NULL };
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

	WSASend(session->NEW_CONNECTION.http.Connection, &buffer, 1, &bytesSent, MSG_DONTROUTE | MSG_PARTIAL, NULL, NULL);
	if (bytesSent == 0) {
		std::cerr << "Error sending file: " << WSAGetLastError() << std::endl;
		ZeroMemory(buffer.buf, BUFFER_SIZE);
		return false;
	}
	ZeroMemory(buffer.buf, BUFFER_SIZE);

	ipm->busy = false;
	ipm->reached = false;
	ipm->Global().update();

	return true;
}

void connectionListener(HTTP_Server* s) {
	std::vector<std::thread> Listeners = {};

	std::cout << "Listening for incoming connections...\n";

	while (1 && !s->should_stop) {
		if (listen(s->ListenSocket, SOMAXCONN) == SOCKET_ERROR) {
			printf("Listen failed with error: %ld\n", WSAGetLastError());
			closesocket(s->ListenSocket);
			s->terminate();
			break;
		}

		Session* newClient = new Session(INVALID_SOCKET);
		newClient->NEW_CONNECTION.http.Connection = WSAAccept(s->ListenSocket, &newClient->NEW_CONNECTION.http.Destination, &newClient->NEW_CONNECTION.http.Dest_len, nullptr, (DWORD_PTR) nullptr);
		if (newClient->NEW_CONNECTION.http.Connection == INVALID_SOCKET) {
			printf("accept failed: %d\n", WSAGetLastError());
			closesocket(s->ListenSocket);
			s->terminate();
			break;
		}
		else {
			int optval = 1;
			setsockopt(newClient->NEW_CONNECTION.http.Connection, IPPROTO_TCP, TCP_NODELAY, (char*)&optval, sizeof(optval));
			setsockopt(newClient->NEW_CONNECTION.http.Connection, SOL_SOCKET, SO_KEEPALIVE, (char*)&optval, sizeof(optval));
			setsockopt(newClient->NEW_CONNECTION.http.Connection, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval));
			struct tcp_keepalive ka;
			ka.onoff = 1;
			ka.keepalivetime = 30000;  // Time before sending first keep-alive probe (30s)
			ka.keepaliveinterval = KEEPALIVE_INTERVAL_MS; // Interval between probes (10s)

			DWORD dwBytesReturned;
			WSAIoctl(newClient->NEW_CONNECTION.http.Connection, SIO_KEEPALIVE_VALS, &ka, sizeof(ka), nullptr, 0, &dwBytesReturned, nullptr, nullptr);

			newClient->active = true;
            std::thread t = std::thread(listener, s, newClient);
			t.detach();
		}
	}
	return;
}

int listener(HTTP_Server* s, Session* CurrentSession) {
	int status = 1;
	size_t clientNumber = 0;
	WSABUF buffer = { BUFFER_SIZE, new char[BUFFER_SIZE] };

	while (status > 0 && !s->should_stop) {
		status = recv(CurrentSession->NEW_CONNECTION.http.Connection, buffer.buf, buffer.len, 0);
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

		// 404 ?
		if (urls.find(page) != urls.end())
		{
			file = s->dir + "." + urls[page];;
			if (file.extension().string().compare(".exe") == 0) {
				if (endpoints[page].compare("throw") == 0) {
					bool sent = executeAndDump(urls[page], CurrentSession, buffer.buf);
				}
				else {
					if (extentions.find(file.extension().string()) != extentions.end()) {
						bool sent = readFile(file.string(), *CurrentSession, true);
						if (!sent) {
							std::cerr << "Error sending file: " << file.string() << std::endl;
							break;
						}
					}
				}
			}
			else {
				bool sent = readFile(file.string(), *CurrentSession, false);
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
				s = (char*)(std::string("HTTP/1.1 404 Not Found\nDate : ") + timeToString(date) +
					"\nHTTP_Server: c_rver / 2.0.0 (Windows)\n"
					"Connection: keep-alive\n"
					"Referrer-Policy: strict-origin-when-cross-origin\r\n"
					"Content-Type: text/html\n"
					"Content-Length: 148\n"
					"X-Content-Type-Options: nosniff\r\n\r\n"
					"<html><head><title>404 Not Found</title></head><body><center>"
					"<h1>404 Not Found</h1></center><hr><center>c_rver/1.0.0 (Windows)"
					"</center></body></html>\r\n").c_str();
			}
			else {
				s = "HTTP/1.1 404 Not Found\nDate : " + timeToString(date) +
					"\nHTTP_Server: c_rver / 2.0.0 (Windows)\n"
					"Connection: keep-alive\n"
					"Referrer-Policy: strict-origin-when-cross-origin\r\n"
					"Content-Type: text/html\n"
					"Content-Length: 0\n"
					"X-Content-Type-Options: nosniff\r\n\r\n";
			}

			WSABUF buffer = { s.length(), s.data() };
			unsigned long bytesSent = 0;
			WSASend(CurrentSession->NEW_CONNECTION.http.Connection, &buffer, 1, &bytesSent, MSG_DONTROUTE, NULL, NULL);
			if (bytesSent == 0) {
				std::cerr << "Error sending header: " << WSAGetLastError() << std::endl;
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


#if defined(__linux__)
	sockaddr_in server_address_in;
	server_address_in.sin_family = AF_INET;

	server_address_in.sin_addr.s_addr = htonl(INADDR_ANY);

	int iport = atoi(port.c_str());
	server_address_in.sin_port = htons(iport);

	SOCKET ListenSocket = socket(AF_INET, SOCK_STREAM, 0);

	bind(ListenSocket, (sockaddr*)&server_sockaddr_in, sizeof(server_address_in));
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

	std::thread AcceptThread = std::thread(connectionListener, this);
	AcceptThread.join();

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

#ifdef _WIN64 || _WIN32
	WSACleanup();
#endif

	log("Successfully terminated!");
	logFile.close();
	return;
}

//1.	Use thread pools instead of creating new threads for each connection
//2.	Add RAII for socket management and proper exception handling