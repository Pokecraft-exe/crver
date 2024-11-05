#ifdef _WIN64 || _WIN32
#include "win32socket.hpp"
#pragma comment(lib, "Ws2_32.lib")
#else
#error "made for windows"
#endif

/*
POST method
conf.ini
*/

int main(int argc, char** argv) {
	//if (argc != 2) std::cout << "`crver get help' to prompt help\n";

	Server* s = new Server();
	std::filesystem::path confFile;

#ifdef _WIN64 || _WIN32
	confFile = "C:/c_rver/conf.ini";
#else
	confFile = "/var/c_rver/conf.ini";
#endif

	if (std::filesystem::exists(confFile))
		std::cout << "not supported yet!\n";
	else {
		s->port = (char*)"80";
		s->dir = "C:/testwww/";
		std::cout << "configuration default\nport: 80\ndirectory: " << s->dir << std::endl;
	}

	if (!s->start()) {
		s->terminate();
		return 1;
	}

	s->terminate();
}