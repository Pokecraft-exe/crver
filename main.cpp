#define crver
#include <json.hpp>
#include <map>
#include <algorithm>
#include <cctype>
#include <functional>
#include <queue>
#include "Socket.hpp"
#include "sites-includes/crver.hpp"

extern int affinity(int IN cpu);

#if defined(_WIN64) || defined(_WIN32)

#define DEFAULT_CONFFILE "config.cfg"
#define LIBSSH_STATIC
#include <windows.h>
#pragma comment(lib, "Ws2_32.lib")

#else

#define _popen(x, y) popen(x, y)
#define DEFAULT_CONFFILE "/etc/crver/config.cfg"

#endif

/*
* TODO:
* add all verbs
* add all encodings
* add http version not supported (1.1 and 3.0 only)
* add logging
* add http redirect
*/

const QUIC_API_TABLE* MsQuic;
HTTP_Server* s = nullptr;
const char* ws = " \t\n\r\f\v";

#if defined(_WIN64) || defined(_WIN32)

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

// definition of maps for all necessary
std::map<std::string, std::string> urls;
std::map<std::string, endpoint_type> endpoints;
std::map<std::string, std::string> extentions;
std::map<std::string, std::string> cache;
std::map<std::string, void*> webMains;
std::map<std::string, std::queue<std::thread::id>> queues;

#if defined(__linux__)
extern "C" {
unsigned int MsQuicOpenVersion(uint32_t a, const void** b) {
	return 0;
}
}
#endif

int main(int argc, char** argv) {
	s = new HTTP_Server();
	std::filesystem::path confFile;

	affinity(1); // run on dedicated cpu 1

	QUIC_STATUS Status;
	if (QUIC_FAILED(Status = MsQuicOpen2(&MsQuic))) {
		printf("MsQuicOpen2 failed, 0x%x!\n", Status);
		return 0;
	}


#if defined(_WIN64) || defined(_WIN32) // Set the console control handler
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
			bool isExecutable = false;

			try {
				path = i.value()["dll"].get<std::string>();
				isExecutable = true;

				if (path.compare(".") == 0) key.substr(1, key.length() - 1);

#if defined(_WIN64) || defined(_WIN32)
				std::wstring wpath = L"";

				{
					std::string str = s->dir + path;
					wpath = std::wstring(str.begin(), str.end());
				}

				HMODULE hDll = LoadLibrary(wpath.c_str());
				if (hDll == NULL) {
					std::cerr << "Could not load DLL: " << path << " Error: " << GetLastError() << std::endl;
					return 1;
				}

				webMains[path] = GetProcAddress(hDll, "WebMain");
				if (webMains[path] == NULL) {
					std::cerr << "Could not load process (WebMain): " << path << " Error: " << GetLastError() << std::endl;
					return 1;
				}
#else
#if defined(__linux__)
				void* handle = dlopen((s->dir + path).c_str(), RTLD_LAZY);
				dlerror();
				webMains[path] = (WebMainFunc)dlsym(handle, "WebMain");
				const char* dlsym_error = dlerror();
				if (dlsym_error) {
					std::cerr << "Could not load symbol 'WebMain' from " << path << " Error: " << dlsym_error << std::endl;
					dlclose(handle);
					return 1;
				}
#endif
#endif
				std::cout << "Loaded DLL: " << path << std::endl;
			}
			catch (std::exception e) {
				try {
					path = i.value()["path"].get<std::string>();

					if (path.compare(".") == 0) path = key;
				}
				catch (std::exception ee) {
					std::cerr << "JSON is invalid, can't find `path' or `dll' at `" << key << "'" << std::endl;
					return 1;
				}
			}
			std::string sMethod = i.value()["treat"].get<std::string>();
			endpoint_type method = i.value()["treat"].get<std::string>().compare("throw") == 0 ? endpoint_type::THROW : endpoint_type::UPLOAD;

			urls[key] = path;
			endpoints[key] = (isExecutable && method == endpoint_type::THROW) ? endpoint_type::EXECUTABLE : method;
			std::cout << "url: " << key << " path: " << path << " treat: " << sMethod << std::endl;
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
		#if defined(__linux__)
			s->dir = "/var/cwww/";
			s->temp = "/tmp/";
		#else
			s->dir = "C:/testwww/";
			s->temp = "T:/";
		#endif
		std::cout << "configuration default\nport: 80\ndirectory: " << s->dir << "\ntemporary directory (ramdisk preferred): " << s->temp << std::endl;
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
