#ifdef _WIN64 || _WIN32

#include "win32socket.hpp"
#pragma comment(lib, "Ws2_32.lib")
#define DEFAULT_CONFFILE "C:/c_rver/conf.ini"

#else

#error "made for windows"

#endif

/*
POST method
conf.ini
*/

int main(int argc, char** argv) {
	Server* s = new Server();
	std::filesystem::path confFile;

	confFile = DEFAULT_CONFFILE;

	if (std::filesystem::exists(confFile)) {
		std::cout << "not supported yet!\n";
		return 1;
	}
	else {
		s->port = (char*)"80";
		s->dir = "C:/testwww/";
		s->temp = "T:/";
		std::cout << "configuration default\nport: 80\ndirectory: " << s->dir << "\ntemporary directory (ramdisk preferred): " << s->temp << std::endl;
	}

	if (!s->start()) {
		s->terminate();
		return 1;
	}

	s->terminate();
}