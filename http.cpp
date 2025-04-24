#include <vector>
#include <string>
#include "win32socket.hpp"
#include "sstream"
#include "http.hpp"

HTTPRequest HTTP(WSABUF buffer, int len) {
	HTTPRequest req;
	std::stringstream request(buffer.buf);
	std::string get("");
	int pos;
	if (buffer.buf[0] == 'G') {
		req.method = GET;
		req.GET = std::map<std::string, std::string >();
		req.page = "";
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

		// retrive the actual page
		request = std::stringstream(get);
		request >> req.page;
		req.page = req.page.substr(0, req.page.find('?'));

		// retrive the actual get map
		while (!request.eof()) {
			std::string one;
			std::string two;

			request >> one;
			request >> two;

			req.GET.insert({ one, two });
		}

		// retrive the session id cookie
		request = std::stringstream(buffer.buf);
		while (std::getline(request, get)) {
			if (get._Starts_with("Cookies")) {
				pos = get.rfind('=');
				get.erase(pos, 1);
				get.insert(pos, " ");

				std::stringstream subRequest(get);

				subRequest >> get >> get;
				if (get.compare("Session-ID")) {
					subRequest >> req.SESSION_ID;
				}
			}
		}
	}
	else if (buffer.buf[0] == 'P') {
		req.method = POST;
		req.POST = std::map<std::string, std::string >();
		req.page = "";
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

		// retrive the actual page
		request = std::stringstream(get);
		request >> req.page;
		req.page = req.page.substr(0, req.page.find('?'));
	}
	else return { none, "", "", {}, {}, 0 };
	return req;
}