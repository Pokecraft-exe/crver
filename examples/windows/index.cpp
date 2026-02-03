#define VER "1.1"
#include <crver.hpp>

WEBMAIN WebMain(crver::HTTPRequest* Request) {
  crver::metaCharset("utf8");
  //crver::link("stylesheet", "text/css", "style.css");

  crver::title((Request->GET.find("title") != Request->GET.end() ? Request->GET["title"] : "index"));
  HTML(
	  <body>
	      Hello from C++ page
	      Ready to try the POST method?
	      <form action="posttest" method="post">
	          firstname:
	          <input name="firstname">
	          lastname:
	          <input name="lastname">
	  		  <input type="submit">
	      </form>
	      Ready to try the GET method?
	      <form action="gettest" method="get">
	          firstname:
	          <input name="firstname">
	          lastname:
	          <input name="lastname">
	          <input type="submit">
	      </form>
	      <table style="border-collapse: collapse;border: 1 black;">
	          <thead>
	              <tr>
	                  <th>
	                      Name
	                  </th>
	                  <th>
	                      Value
	                  </th>
	              </tr>
	          </thead>
	);
	for (auto i : Request->GET) {
		crver::out << "<tr><td>" << i.first << "</td><td>" << i.second << "</td></tr>";
	}
	HTML(
	      </table>
	      <table>
                <thead>
                  <tr>
                    <th>Cookie name</th>
                    <th>Cookie value</th>
                  </tr>
                </thead>
                <tbody>);
	for (auto i : Request->COOKIES) {
                crver::out << "<tr><td>" << i.first << "</td><td>" << i.second << "</td></tr>";
        }
        HTML(</tbody>
              </table>
	  </body>
	);


  return crver::HTTPResponse(200, "text/html", Request);
}
