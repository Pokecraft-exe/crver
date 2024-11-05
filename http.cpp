#include <vector>
#include <string>
#include "win32socket.hpp"
#include "sstream"
#include "http.hpp"

HTTPRequest HTTP(WSABUF buffer, int len) {
	HTTPRequest req;
	if (buffer.buf[0] == 'G') {
		req.method = GET;
		req.GET = std::make_shared<std::map<std::string, std::string > >();
		req.page = "";

		std::stringstream request(buffer.buf);

		std::string get("");

		//std::getline(request, get);

		request >> get;

		/*int find = request.find("?");
		if (find != std::string::npos) {
			req.page = request.substr(0, find);

			request = request.substr(req.page.length(), request.length() - 4);

			size_t aFindEqual = 0;
			while ((aFindEqual = request.find('=')) != std::string::npos) {

			}

			auto aFindAmpersand = request.find('&');
			while ((aFindEqual = request.find('=')) != std::string::npos ||
				(aFindAmpersand = request.find('&')) != std::string::npos) {
				if ()
			}
		}
		else {
			req.page = request.substr(0, request.length() - 4);
		}*/
	}
	return req;
}