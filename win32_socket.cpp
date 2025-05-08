#define _CRT_SECURE_NO_WARNINGS
#include "win32socket.hpp"
#include "http.hpp"
#include "compress.hpp"
#include "win32_interProcessMemory.hpp"
#include <fstream>
#include <codecvt>
#include <stdexcept>

extern std::map<std::string, std::string> urls;
extern std::map<std::string, std::string> endpoints;
extern std::map<std::string, std::string> extentions;

struct filebuf {
	size_t size;
	char* data;
};

/*
 * Constructor and operator for Session to act as the socket for simplicity
 */
Session::Session(SOCKET sock) { connexion = sock; dest_len = sizeof(sockaddr); }
Session::Session(const Session& sess) { connexion = sess.connexion; dest_len = sess.dest_len; id = sess.id; destination = sess.destination; cache = sess.cache; }
Session::operator SOCKET() { return connexion; }
Session::~Session() { cache.~basic_string(); }

/*
 * Constructor for Server
 */
Server::Server(void) {};

std::string timeToString(const std::tm* timeObj) {
	std::ostringstream oss;
	oss << std::put_time(timeObj, (char*)"%a, %d %b %Y %T %Z");
	return oss.str();
}

/*
 * Returns all the content of a file 
 */
filebuf readFile(const std::string& filename) {
	std::ifstream file(filename, std::ios::binary | std::ios::ate);
	std::vector<char> contents;

	// Check if the file was opened successfully
	if (!file.is_open()) {
		std::cerr << "Error opening file: " << filename << std::endl;
		return {};
	}

	// Get the file's size
	std::streampos fileSize = file.tellg();
	file.seekg(0, std::ios::beg);

	// Reserve memory for the string to avoid multiple reallocations
	contents.resize(fileSize);

	// Read the file's contents into the string
	if (!file.read(contents.data(), fileSize)) {
		file.close();
		throw std::runtime_error("cannot read the file " + filename);
	}

	// Close the file
	file.close();

	return { fileSize, contents.data() };
}

/*
 * Execute a program with the session and the request
 */
char* executeAndDump(std::string cmd, Session* session, HTTPRequest* request) {
	FILE* fp;
	std::array<char, 0x1000> buffer;
	std::string result;

	std::ofstream osCacheFile(session->cache, std::ios::out | std::ios::binary);
	std::string sSize = std::to_string(sizeof(Session)) + "\n";

	osCacheFile << session->connexion;
	osCacheFile << " ";
	osCacheFile << (int)request->method << " ";
	if (request->method == GET) {
		for (auto i : request->GET) {
			osCacheFile << i.first << " " << i.second << " ";
		}
	}
	else {
		std::cout << "writting POST method to cache file is not supported yet!\n";
	}

	osCacheFile.close();

	cmd += " " + session->cache;

	// Open the command for reading
	fp = _popen(cmd.c_str(), "r");
	if (fp == NULL) {
		printf("Failed to run command\n");
	}

	// Read the output a line at a time - output it
	while (fgets(buffer.data(), 0x1000 - 1, fp) != NULL) {
		result += buffer.data();
	}

	return (char*)result.c_str();
}

void Server::connexionListener(Server* s) {
	std::vector<std::thread> Listeners = {};

	std::cout << "Listening for incoming connexions...\n";

	while (1) {
		if (listen(s->ListenSocket, SOMAXCONN) == SOCKET_ERROR) {
			printf("Listen failed with error: %ld\n", WSAGetLastError());
			closesocket(s->ListenSocket);
			s->terminate();
			break;
		}

		// Accept a client socket
		Session newClient = INVALID_SOCKET;
		newClient.connexion = WSAAccept(s->ListenSocket, &newClient.destination, &newClient.dest_len, nullptr, 0);
		if (newClient == INVALID_SOCKET) {
			printf("accept failed: %d\n", WSAGetLastError());
			closesocket(s->ListenSocket);
			s->terminate();
			break;
		}
		else {
			InetNtop(s->hints.ai_family, &newClient.destination, s->ws_addr, 100);

			std::thread t = std::thread(listener, s, newClient);
			t.detach();
		}
	}
	return;
}

int Server::listener(Server* s, Session newClient) {
	int status = 1;
	Session CurrentSession = newClient;
	WSAOVERLAPPED RecvOverlapped;
	WSABUF buffer;
	DWORD bytesSent = 0, flags = 0;
	std::string sResponse_buf = "";

	while (status > 0) {
		buffer = { 0x1000, new char[0x1000] };
		ZeroMemory(buffer.buf, 0x1000);

		status = recv(CurrentSession, buffer.buf, buffer.len, 0);
		if (status < 0)
			wprintf(L"\trecv failed with error: %d\n", WSAGetLastError());

		HTTPRequest request = HTTP(buffer, status);
		char* responseBuffer = nullptr;
		std::time_t t_date = time(nullptr);
		std::tm* date = gmtime(&t_date);
		std::filesystem::path file = s->dir + "." + urls[request.page];

		if (request.SESSION_ID == -1) {
			std::wcout << "New client no " << NUM_CLIENTS();
			newClient.cache = s->temp + "crver_session_" + std::to_string(NUM_CLIENTS()) + ".tmp";
			s->ClientSessions.push_back(newClient);
			CurrentSession.cache = newClient.cache;
		}

		std::cout << "Requested page : " << request.page << " method : " << (request.method == GET ? "GET" : (request.method == POST ? "POST" : "none")) << " Client: " << CurrentSession.id;

		if (request.method == none) {
			status = 0;
			break;
		}

		if (!request.page.compare(""))
			return 0;
		// 404 ?
		if (std::filesystem::exists(file)) 
		{	// is compiled?
			if (endpoints[request.page].compare("throw") == 0) {
				if (file.extension().string().compare(".exe") == 0) {
					responseBuffer = executeAndDump(file.string(), &CurrentSession, &request);
				}
				else {
					if (extentions.find(file.extension().string()) != extentions.end()) {
						std::string page = readFile(file.string());
						responseBuffer = HTTPResponse(date, page, page.size(), CurrentSession.id, extentions[file.extension().string()]);
					}
				}
			}
			else {
				std::string page = readFile(file.string());
				responseBuffer = (char*)(std::string("HTTP/1.1 200 OK\nDate : ") + timeToString(date) + \
					"\nServer: c_rver / 2.0.0 (Windows)\n"								\
					"Content-Disposition: attachment; filename=\"" + file.filename().string() + "\"\n"\
					"Content-Length:" + std::to_string(page.size()) + "\n"						\
					"Content-Transfer-Encoding: binary"									\
					"Connection: keep-alive\n"											\
					"Referrer-Policy: strict-origin-when-cross-origin\n"				\
					"X-Content-Type-Options: nosniff\n"									\
					"Set-Cookie: Session-ID=" + std::to_string(CurrentSession.id) + "\n"		\
					"Feature-Policy: accelerometer 'none'; camera 'none'; geolocation 'none'; gyroscope 'none'; magnetometer 'none'; microphone 'none'; payment 'none'; usb 'none'\n" \
					+ std::string(page) + "\r\n").c_str();
			}
		}
		else 
		{
			if (!file.extension().string().compare(".exe") ||
				!file.extension().string().compare(".html")) {
				std::cout << "404";
				responseBuffer = HTTP404(date);
			}
			else {
				responseBuffer = (char*)(std::string("HTTP/1.1 404 Not Found\nDate : ") + timeToString(date) +
					"\nServer: c_rver / 2.0.0 (Windows)\n"
					"Connection: keep-alive\n"
					"Referrer-Policy: strict-origin-when-cross-origin\r\n"
					"Content-Type: text/html\n"
					"Content-Length: 0\n"
					"X-Content-Type-Options: nosniff\r\n\r\n").c_str();
			}
		}

		delete[] buffer.buf;
		buffer = { (unsigned long)sResponse_buf.size()+2, (CHAR*)sResponse_buf.c_str() };

		WSASend(CurrentSession, &buffer, 1, &bytesSent, MSG_DONTROUTE, NULL, NULL);
	};

	std::cout << "Listener closed\n";

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

	if (getaddrinfo(NULL, port, &hints, &result) != 0) {
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

	hints.ai_addr = result->ai_addr;
	hints.ai_addrlen = result->ai_addrlen;

	freeaddrinfo(result);

	std::thread AcceptThread = std::thread(&connexionListener, this);
	AcceptThread.join();

	return 0;
}

void Server::terminate(void) {
	WSACleanup();
	return;
}