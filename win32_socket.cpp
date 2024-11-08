#define _CRT_SECURE_NO_WARNINGS
#include "win32socket.hpp"
#include "http.hpp"
#include "compress.hpp"
#include "win32_interProcessMemory.hpp"
#include <fstream>
#include <codecvt>

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

/*
 * Returns all the content of a file 
 */
std::string readFile(const std::string& filename) {
	std::ifstream file(filename);
	std::string contents;

	// Check if the file was opened successfully
	if (!file.is_open()) {
		std::cerr << "Error opening file: " << filename << std::endl;
		return "";
	}

	// Get the file's size
	file.seekg(0, std::ios::end);
	std::streampos fileSize = file.tellg();
	file.seekg(0, std::ios::beg);

	// Reserve memory for the string to avoid multiple reallocations
	contents.reserve(fileSize);

	// Read the file's contents into the string
	contents.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());

	// Close the file
	file.close();

	return contents;
}

/*
 * Execute a program with the session and the request
 */
std::string exec(std::string cmd, Session* session, HTTPRequest* request) {
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

	return result;
}

int socketBounded(std::vector<Session>& Sessions, sockaddr a) {
	size_t size = Sessions.size();
	for (int i = 0; i < size; i++) {
		if (memcmp(Sessions[i].destination.sa_data, a.sa_data, sizeof(CHAR)) == 0) {
			return i;
		}
	}
	return 0;
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

			int socketNumber = socketBounded(s->ClientSessions, newClient.destination);
			if (socketNumber != 0) {
				listener(s, NUM_CLIENTS() - 1);
			}
			else {
				std::wcout << "New client no " << NUM_CLIENTS() << " destination: " << s->ws_addr << '\n';

				newClient.cache = s->temp + "crver_session_" + std::to_string(NUM_CLIENTS()) + ".tmp";
				s->ClientSessions.push_back(newClient);

				std::thread t = std::thread(listener, s, NUM_CLIENTS() - 1);
				t.detach();
			}
		}
	}
	return;
}

int Server::listener(Server* s, int i_currentSession) {
	int status = 1;
	Session CurrentSession = s->ClientSessions[i_currentSession];
	WSABUF buffer;
	DWORD bytesSent = 0;
	std::string sResponse_buf = "";

	std::cout << "Listener[" << i_currentSession << "] engaged\n";
	while (status > 0) {
		buffer = { 0x1000, new char[0x1000] };

		status = recv(CurrentSession, buffer.buf, buffer.len, 0);
		if (status < 0) {
			wprintf(L"\trecv failed with error: %d\n", WSAGetLastError());
			s->terminate();
			exit(0);
		}
		HTTPRequest request = HTTP(buffer, status);
		std::stringstream response_buf;
		std::time_t t_date = time(nullptr);
		std::tm* date = gmtime(&t_date);
		std::filesystem::path file = s->dir + "." + request.page;

		std::cout << "Listener[" << i_currentSession << "] requested page : " << request.page << " method : " << (request.method == GET ? "GET" : "POST") << " Client: " << CurrentSession.id;

		if (!request.page.compare(""))
			return 0;
		// 404 ?
		if (std::filesystem::exists(file)) 
		{	// is compiled?
			if (!file.extension().string().compare(".exe")) {
				response_buf << exec(file.string(), &CurrentSession, &request);
			}
			else if (!file.extension().string().compare(".css")) {
				std::string page = readFile(file.string());
				response_buf << HTTPResponse(date, page, page.size(), CurrentSession.id, "text/css");
			} else {
				std::string page = readFile(file.string());
				response_buf << HTTPResponse(date, page, page.size(), CurrentSession.id, "text/html");
			}
		}
		else 
		{
			std::cout << "404";
			response_buf << HTTP404(date);
		}

		sResponse_buf = response_buf.str();

		delete[] buffer.buf;
		buffer = { (unsigned long)sResponse_buf.size(), (CHAR*)sResponse_buf.c_str() };

		WSASend(CurrentSession, &buffer, 1, &bytesSent, MSG_DONTROUTE, NULL, NULL);
	};

	std::cout << "Listener[" << i_currentSession << "] closed\n";

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