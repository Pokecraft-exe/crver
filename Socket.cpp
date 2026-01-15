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
#include "Worker.hpp"
#include "compress.hpp"

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
int sessionsNum = 0;
std::queue<Session*> SocketQueue;

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

inline bool getPage(std::string& page, const WSABUF& buffer) {
	if (!strcmp(buffer.buf, "")) return false;

	// cstyle cuz need to b fast
	unsigned long long int end = 0;
	unsigned long long int start = 0;
	for (end; end < buffer.len; end++) {
		if (buffer.buf[end] == ' '     &&
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

	if (start < end)
		page.resize(end - start);
	else
		return false;

	for (int i = start; i <= end; i++) page[i - start] = buffer.buf[i];
	return true;
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
 Execute a dll and send the response.
  std::string dll    Name of the dll to execute (must be in the urls map)
  Session& session   Session to send the response to
  char* request      Buffer containing the HTTP request
 */
inline bool executeAndDump(std::string& dll, Session* session, char* request) {
	unsigned long bytesSent = 0;

	HTTPRequest req = InitRequest(request, sessionsNum);
	WSABUF buffer = reinterpret_cast<WSABUF(*)(HTTPRequest*)>(webMains[dll])(&req);

	WSASend(session->NEW_CONNECTION.http.Connection, &buffer, 1, &bytesSent, 0, NULL, NULL);
	if (bytesSent == 0) {
		std::cerr << "Error sending buffer: " << WSAGetLastError() << std::endl;
		return false;
	}

	return true;
}

/*
 Encode string into application / x - www - form - urlencoded
 */
std::string url_encode(const std::string& value) {
	std::ostringstream encoded;
	for (unsigned char c : value) {
		if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
			encoded << c;
		}
		else if (c == ' ') {
			encoded << '+';
		}
		else {
			encoded << '%' << std::uppercase << std::hex
				<< std::setw(2) << std::setfill('0') << int(c);
		}
	}
	return encoded.str();
}

// Decode application/x-www-form-urlencoded string
std::string url_decode(const std::string& value) {
	std::ostringstream decoded;
	for (size_t i = 0; i < value.size(); ++i) {
		char c = value[i];
		if (c == '+') {
			decoded << ' ';
		}
		else if (c == '%' && i + 2 < value.size() &&
			std::isxdigit(value[i + 1]) && std::isxdigit(value[i + 2])) {
			std::string hex = value.substr(i + 1, 2);
			char decoded_char = static_cast<char>(std::stoi(hex, nullptr, 16));
			decoded << decoded_char;
			i += 2;
		}
		else {
			decoded << c;
		}
	}
	return decoded.str();
}

HTTPRequest InitRequest(std::string data, int maxsession) {
	HTTPRequest req = { none, "", {}, {} };
	std::stringstream request(data);
	std::string get("");
	int pos;
	if (data[0] == 'G' || request.str().find("?") != std::string::npos) {
		req.method = HTTP_GET;
		req.GET = std::map<std::string, std::string >();
		req.endpoint = "";
		request >> get >> get;

		// replace & and = to space so >> operator works
		while ((pos = get.rfind('&')) != std::string::npos) {
			get.erase(pos, 1);
			get.insert(pos, " ");
		}
		while ((pos = get.rfind('=')) != std::string::npos) {
			get.erase(pos, 1);
			get.insert(pos, " ");
		}
		if ((pos = get.rfind('?')) != std::string::npos) {
			get.erase(pos, 1);
			get.insert(pos, " ");
		}

		// retrive the actual endpoint
		request = std::stringstream(get);
		request >> req.endpoint;
		req.endpoint = req.endpoint.substr(0, req.endpoint.find('?'));

		// retrive the actual get map
		while (!request.eof()) {
			std::string one;
			std::string two;

			request >> one;
			request >> two;

			req.GET.insert({ one, two });
		}
	}
	else if (data[0] == 'P' && data[1] == 'O') {
		req.method = HTTP_POST;
		req.POST = std::map<std::string, std::string >();
		req.endpoint = "";
		request >> get >> get;

		// replace & and = to space to >> operator works
		while ((pos = get.rfind('&')) != std::string::npos) {
			get.erase(pos, 1);
			get.insert(pos, " ");
		}
		while ((pos = get.rfind('=')) != std::string::npos) {
			get.erase(pos, 1);
			get.insert(pos, " ");
		}
		if ((pos = get.rfind('?')) != std::string::npos) {
			get.erase(pos, 1);
			get.insert(pos, " ");
		}

		// retrive the actual endpoint
		request = std::stringstream(get);
		request >> req.endpoint;
		req.endpoint = req.endpoint.substr(0, req.endpoint.find('?'));
	}
	// retrive all cookies
	request = std::stringstream(data);
	while (std::getline(request, get)) {
		if (get.substr(0, 7).compare("Cookies") == 0) {
			std::stringstream cookies(get.substr(8, get.length() - 8));
			while (std::getline(cookies, get, ';')) {
				std::string key = get.substr(0, get.find('='));
				std::string value = get.substr(get.find('=') + 1, get.length() - (get.find('=') + 1));
				req.COOKIES.insert({ url_decode(key), url_decode(value) });
			}
		}
	}

	req.SESSION = std::map<std::string, std::string >();
	if (req.COOKIES.find("SESSIONID") == req.COOKIES.end())
		req.COOKIES.insert({ "SESSIONID", std::to_string(maxsession) });

	// load session data from shared memory
#if defined(_WIN64) || defined(_WIN32)
	std::wstring fileName = L"IPM_SESSION_" + std::wstring(req.COOKIES["SESSIONID"].begin(), req.COOKIES["SESSIONID"].end());
	size_t fileSize = 0x1000;
	HANDLE hMapFile = CreateFileMappingW(
		INVALID_HANDLE_VALUE,
		NULL,
		PAGE_READWRITE,
		0,
		fileSize,
		fileName.c_str());

	char* buffer = reinterpret_cast<char*>(MapViewOfFile(hMapFile,
		FILE_MAP_ALL_ACCESS,
		0,
		0,
		fileSize));
#else
#if defined(__linux__)
	std::string fileName = "/IPM_SESSION_" + req.COOKIES["SESSIONID"];
	size_t fileSize = 0x1000;
	int fd = shm_open(fileName.c_str(), O_RDONLY, 0666);
	char* buffer = reinterpret_cast<char*>(mmap(NULL, fileSize,
		PROT_READ, MAP_SHARED, fd, 0));
#endif
#endif
	std::stringstream ss(buffer);
	// session data is stored as key=value\nkey2=value2\n
	while (std::getline(ss, get)) {
		std::string key = get.substr(0, get.find('='));
		std::string value = get.substr(get.find('=') + 1, get.length() - (get.find('=') + 1));
		req.SESSION.insert({ url_decode(key), url_decode(value) });
	}

	// close handles
#if defined(_WIN64) || defined(_WIN32)
	UnmapViewOfFile(buffer);
	CloseHandle(hMapFile);
#else
#if defined(__linux__)
	munmap(buffer, fileSize);
	close(fd);
#endif
#endif
	return req;
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
		SocketQueue.push(newClient);
	}
	return;
}

int listener(HTTP_Server* s, Session* CurrentSession, Worker* w) {
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

		// append the buffer to "/buffer.buf" file with a separator of ---------------
		std::ofstream os("/buffer.buf", std::ios::app);
		os << buffer.buf << "\n---------------\n";

		bool pageGot = getPage(page, buffer);

		if (!pageGot) {
			WSAAddressToStringA(&CurrentSession->NEW_CONNECTION.http.Destination, CurrentSession->NEW_CONNECTION.http.Dest_len, NULL, buffer.buf, (LPDWORD)&buffer.len);
			log("[" + std::string(buffer.buf) + "] [Error] Couldn't retrieve page from request.");
			break;
		}

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

	Worker* CurrentWorker = Workers;

	for (int i = 0; i < 100; i++) {
		CurrentWorker->initialize(this);
		CurrentWorker->createNewWorker();
		CurrentWorker = CurrentWorker->getNextWorker();
		WorkersCount++;
	}

	printf("%d Workers ready.\n", WorkersCount);
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