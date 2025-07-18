#include <json.hpp>
#include <map>
#include <algorithm>
#include <cctype>
#include <functional>
#include <queue>
#include "ipm.hpp"
#include "Socket.hpp"
#define DEFAULT_CONFFILE "config.cfg"

#ifdef _WIN64 || _WIN32

#define LIBSSH_STATIC
#include <windows.h>
#pragma comment(lib, "Ws2_32.lib")

#else

#define _popen(x, y) popen(x, y)

#endif

/*
POST method
*/

const QUIC_API_TABLE* MsQuic;
HTTP_Server* s = nullptr;
const char* ws = " \t\n\r\f\v";

#ifdef _WIN64 || _WIN32

/**
 * Remap the console events to perform a cleanup
 */
BOOL WINAPI ConsoleEventHandler(DWORD event) {
	switch (event) {
	case CTRL_C_EVENT:
		s->terminate();
		std::cout << "Ctrl+C pressed. Cleaning up...\n";
		return TRUE; // Prevent default behavior
	case CTRL_CLOSE_EVENT:
		s->terminate();
		std::cout << "Console is closing. Performing cleanup...\n";
		return TRUE; // Prevent default behavior
	case CTRL_BREAK_EVENT:
		s->terminate();
		std::cout << "Ctrl+Break pressed. Handling...\n";
		return TRUE;
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		s->terminate();
		std::cout << "System is logging off or shutting down. Handling...\n";
		return TRUE;
	default:
		return FALSE; // Let the system handle other events
	}
}

#endif

class pjson : public nlohmann::json {
public:
	pjson(nlohmann::json j) : nlohmann::json(j) {
	}

	template<typename T>
	T tryGetJson(std::string key) {
		T value;
		try {
			value = this->operator[](key).get<T>();
		}
		catch (exception e) {
			std::cout << "Element " << key << " was not found in the config file.";
			std::terminate();
		}
		return value;
	}
};

// trim from end of string (right)
inline std::string& rtrim(std::string& s, const char* t = ws)
{
	s.erase(s.find_last_not_of(t) + 1);
	return s;
}

// trim from beginning of string (left)
inline std::string& ltrim(std::string& s, const char* t = ws)
{
	s.erase(0, s.find_first_not_of(t));
	return s;
}

// trim from both ends of string (right then left)
inline std::string& trim(std::string& s, const char* t = ws)
{
	return ltrim(rtrim(s, t), t);
}

std::map<std::string, std::string> urls;
std::map<std::string, std::string> endpoints;
std::map<std::string, std::string> extentions;
std::map<std::string, std::string> cache;
std::map<std::string, interProcessMemory<WebIPM>> ipms;
std::map<std::string, std::queue<std::thread::id>> queues;

int main(int argc, char** argv) {
	s = new HTTP_Server();
	std::filesystem::path confFile;

	QUIC_STATUS Status;
	if (QUIC_FAILED(Status = MsQuicOpen2(&MsQuic))) {
		printf("MsQuicOpen2 failed, 0x%x!\n", Status);
		return 0;
	}

#ifdef _WIN64 || _WIN32
	// Set the console control handler
	if (!SetConsoleCtrlHandler(ConsoleEventHandler, TRUE)) {
		std::cerr << "Error: Could not set control handler.\n";
		return 1;
	}
#endif

	confFile = DEFAULT_CONFFILE;

	if (std::filesystem::exists(confFile)) {
		std::ifstream file(confFile);
		pjson j = nlohmann::json::parse(file);
		std::string port = j.tryGetJson<std::string>("port");
		std::string dir = j.tryGetJson<std::string>("directory");
		std::string temp = j.tryGetJson<std::string>("temporary");
		std::string key = j.tryGetJson<std::string>("key");
		std::string cert = j.tryGetJson<std::string>("cert");

		trim(port);
		s->port = port;
		s->dir = dir;
		s->temp = temp;
		s->key = dir + key;
		s->cert = dir + cert;

		// verify port is an integer
		if (!std::all_of(port.begin(), port.end(), ::isdigit) || port.empty()) {
			std::cerr << "Error: Port must be a valid number" << std::endl;
			return 1;
		}

		std::cout << "Certificate file: " << s->cert << '\n';
		std::cout << "Key file: " << s->key << '\n';

        for (const auto& i : j["urls"].items()) { // Add reference (&) to avoid copying each item
			std::string key = i.key();
			std::string path;

			try {
				path = i.value()["ipm"].get<std::string>();

				if (path.compare(".") == 0) path = "IPM_" + key.substr(1, key.length() - 1);
				ipms[path].Open(IPM_STRING(path.begin(), path.end()).c_str());
				interProcessMemory<WebIPM>* ipm = &ipms[path];

				ipm->busy = false;
				ipm->requested = false;
				ipm->partial = false;
				ipm->reached = false;
				ipm->Global().update();

				if (ipms[path].Ready() != IPM_SUCCESS) {
					std::cerr << "Couldn't open IPM " << key << std::endl;
					return 1;
				}
			}
			catch (std::exception e) {
				try {
					path = i.value()["path"].get<std::string>();

					if (path.compare(".") == 0) path = key;
				}
				catch (std::exception ee) {
					std::cerr << "JSON is invalid, can't find `path' or `ipm' at `" << key << "'" << std::endl;
					return 1;
				}
			}
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

	// start the pages
	for (auto& i : ipms) {
		std::filesystem::path path = s->dir + i.first.substr(4, i.first.length() - 4);
		_popen(path.string().c_str(), "r");
		std::cout << "Starting page: " << path.string() << std::endl;
	}

	if (!s->start()) {
		s->terminate();
		delete s;
		return 1;
	}

	s->terminate();
	delete s;
	return 0;
}
