#ifdef _WIN64 || _WIN32

#include "win32socket.hpp"
#pragma comment(lib, "Ws2_32.lib")
#define DEFAULT_CONFFILE "config.cfg"

#else

#error "made for windows"

#endif

#include <json.hpp>
#include <map>
/*
POST method
*/

std::map<std::string, std::string> urls;
std::map<std::string, std::string> endpoints;
std::map<std::string, std::string> extentions;

int main(int argc, char** argv) {
	Server* s = new Server();
	std::filesystem::path confFile;

	confFile = DEFAULT_CONFFILE;

	if (std::filesystem::exists(confFile)) {
		std::ifstream file(confFile);
		nlohmann::json j = nlohmann::json::parse(file);
		std::string port = j["port"].get<std::string>();
		std::string dir = j["directory"].get<std::string>();
		std::string temp = j["temporary"].get<std::string>();

		s->port = (char*)port.c_str();
		s->dir = dir;
		s->temp = temp;

        for (const auto& i : j["urls"].items()) { // Add reference (&) to avoid copying each item
           std::string key = i.key();
           std::string path = i.value()["path"].get<std::string>();
           if (path.compare(".") == 0) path = key;

           std::string method = i.value()["treat"].get<std::string>();

           urls[key] = path;
           endpoints[key] = method;
           std::cout << "url: " << key << " path: " << path << " treat: " << method << std::endl;
        }

		for (const auto& i : j["extentions"].items()) {
			std::string key = i.key();
			std::string method = i.value().get<std::string>();
			extentions[key] = method;
			std::cout << "extention: " << key << " method: " << method << std::endl;
		}

		file.close();

		std::cout << "configuration loaded\nport: " << s->port << "\ndirectory: " << s->dir << "\ntemporary directory (ramdisk preferred): " << s->temp << std::endl;

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