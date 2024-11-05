#define _CRT_SECURE_NO_WARNINGS
#include "win32socket.hpp"
#include "http.hpp"
#include "compress.hpp"
#include "win32_interProcessMemory.hpp"
#include <codecvt>

#define StrToW(x) MultiByteToWideChar(x)

std::string WtoStr(wchar_t* ws) {
	char buf[255] = { 0 };
	WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, ws, wcslen(ws), buf, 255, "@ù", nullptr);
	return std::string(buf);
}

Session::Session(SOCKET sock) { connexion = sock; dest_len = sizeof(sockaddr); }
Session::Session(const Session& sess) { memcpy(this, &sess, sizeof(Session)); }
Session::operator SOCKET() { return connexion; }

Server::Server(void) {};

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

std::string exec(std::string cmd, Session* session, HTTPRequest* request) {
	FILE* fp;
	std::array<char, 0x1000> buffer;
	std::string result;

	cmd += " " + WtoStr(session->server_to_page) + " " + WtoStr(session->page_to_server);

	// Open the command for reading
	fp = _popen(cmd.c_str(), "r");
	if (fp == NULL) {
		printf("Failed to run command\n");
	}

	// Read the output a line at a time - output it
	while (fgets(buffer.data(), 0x5000 - 1, fp) != NULL) {
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
				s->ClientSessions[socketNumber].listener = std::thread(&listener, s, socketNumber);
				s->ClientSessions[socketNumber].listener.detach();
			}
			else {
				std::wcout << "New client no " << NUM_CLIENTS() << " destination: " << s->ws_addr << '\n';

				newClient.server_to_page = (wchar_t*)(L"GLOBAL/" + std::to_wstring(newClient.connexion) + L"PTS").c_str();
				newClient.page_to_server = (wchar_t*)(L"GLOBAL/" + std::to_wstring(newClient.connexion) + L"PTS").c_str();

				s->ClientSessions.push_back(newClient);

				interProcessMemory::send(newClient.server_to_page, (char*)"test", 4);
				interProcessMemory::send(newClient.page_to_server, (char*)"test", 4);
				s->ClientSessions[NUM_CLIENTS() - 1].listener = std::thread(&listener, s, NUM_CLIENTS() - 1);
				s->ClientSessions[NUM_CLIENTS() - 1].listener.join();
			}
		}
	}
	return;
}

int Server::listener(Server* s, int i_currentSession) {
	int status = 0;
	Session CurrentSession = s->ClientSessions[i_currentSession];
	WSABUF buffer;
	DWORD bytesSent = 0;

	std::cout << "Listener[" << i_currentSession << "] engaged\n";

	do {
		buffer.buf = new char[0x5000]; // 5KB buffer for recv
		buffer.len = 0x5000;

		status = recv(CurrentSession, buffer.buf, buffer.len, 0);
		if (status > 0) 
		{
			HTTPRequest request = HTTP(buffer, status);

			if (request.SESSION_ID) return;

			std::stringstream response_buf;
			std::time_t t_date = time(nullptr);
			std::tm* date = gmtime(&t_date);
			std::string page = "";

			//std::cout << "Listener[" << i_currentSession << "] requested page : " << request.page << " method : " << (request.method == GET ? "GET" : "POST") << " Client: " << CurrentSession.id;

			std::filesystem::path file = s->dir + "." + request.page;

			if (!std::filesystem::exists(file)) 
			{
				if (file.extension().string().compare("") || file.extension().string().compare(""))
					page = exec(file.string(), &CurrentSession, &request);
				else 
					page = readFile(file.string());

				response_buf << HTTPResponse(date, page, page.size(), CurrentSession.id);
			}
			else 
			{
				std::cout << "404";
				response_buf << HTTP404(date);
			}

			std::string s_response_buf = response_buf.str();

			delete[] buffer.buf;
			buffer.buf = (CHAR*)s_response_buf.c_str();
			buffer.len = response_buf.str().length();

			WSASend(CurrentSession, &buffer, 1, &bytesSent, MSG_DONTROUTE, NULL, NULL);
		}
		else if (status < 0)
			wprintf(L"\trecv failed with error: %d\n", WSAGetLastError());
	} while (status > 0);


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