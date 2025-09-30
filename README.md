# crver
(PROOF OF CONCEPT) a custom http server to send compiled dynamic pages

## MADE FOR WINDOWS

For now, it only works with windows.
the main file is made so the switching from and to one and another system is easy with #define declarations.
if you want to contribute, you can adapt the windows code to linux. The files without the win32_ prefix are compatible for all systems.

the default port is 80
the incoming request accept permissions are total (loopback, local, www)
the default www folder is C:\testwww\

## link between server and the pages

The new crver API uses IPM (an abstract of Memory Mapped Files) to share info between the pages and the server.

## CRVER API

c++ example using crver.hpp file
```c++
/**
  * The name of the IPM should be the same as the server's
  * I's defined in the config file under "ipm" : "..."
  * you either choose a name of let the server decide with "." (auto)
  * the "auto" mode takes the endpoint and adds IPM_ before
  * 
  * example:
  * 
  * "/index.exe" : {
  *     "ipm" : ".", # => IPM_index.exe
  *     "treat" : "throw"
  * }
  * 
  * for the server, only the "ipm" field defines an executable and a regular file.
  */
#define IPM_NAME L"IPM_index.exe" // widen with L prefix (const wchar_t*)
#include "includes/crver.hpp"
using crver::endl, std::cout;

int WebMain(crver::HTTPRequest Request) {
  crver::metaCharset("utf8");
  crver::link("stylesheet", "text/css", "style.css");

  crver::title((Request.GET.find("title") != Request.GET.end() ? Request.GET["title"] : "index"));
  cout <<
	  "  <body>"
	  "    Hello from C++ page" << endl <<
	  "    Ready to try the POST method?" << endl << 
	  "    <form action=\"posttest.exe\" method=\"post\">"
	  "       firstname:" << endl <<
	  "       <input name=\"firstname\">" << endl <<
	  "       lastname:" << endl <<
	  "       <input name=\"lastname\">"
	  "		  <input type=\"submit\">"
	  "    </form>" << endl <<
	  "    Ready to try the GET method?" << endl <<
	  "    <form action=\"gettest.exe\" method=\"get\">"
	  "       firstname:" << endl <<
	  "       <input name=\"firstname\">" << endl <<
	  "       lastname:" << endl <<
	  "       <input name=\"lastname\">"
	  "		  <input type=\"submit\">"
	  "    </form>"
	  "    <table>"
	  "<thead>"
	  "    <tr>"
	  "    <th>"
	  "      Name"
	  "    </th>"
	  "    <th>"
	  "      Value"
	  "    </th>"
	  "  </tr>"
	  "</thead>";
for (auto i : Request.GET) {
	cout << "<tr><td>" << i.first << "</td><td>" << i.second << "</td></tr>";
}
  cout <<
	  "    </table>"
	  "  </body>";

  crver::HTTPResponse(200, "text/html");
  return 0;
}
```
![image](https://github.com/user-attachments/assets/1d8a0dbf-7f66-4c87-bf95-2c0991cf3e72)

## License

The project, past and future releases, forks and codes, is under the CreativeCommons License:

<div style="display:flex;flex-direction:row;">
  <img src="https://mirrors.creativecommons.org/presskit/buttons/88x31/png/by-nc-sa.png" width="200px" style="float:right;"/>
  <img src="https://mirrors.creativecommons.org/presskit/buttons/88x31/png/by-nc-sa.eu.png" width="200px" style="float:left;"/>
</div>
