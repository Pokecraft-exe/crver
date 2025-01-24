# crver
(PROOF OF CONCEPT) a custom http server to send/request compiled dynamic pages

## MADE FOR WINDOWS

For now, it only works with windows.
the main file is made so the switching from and to one and another system is easy with #define declarations.
if you want to contribute, you can adapt the windows code to linux. The files without the win32_ prefix are compatible for all systems.

the default port is 80
the incoming request accept permissions are total (loopback, local, www)
the default www folder is C:\testwww\

## SECURITY ISSUES

a request like
``` {server}/../{...} ```
is (for now) valid and can leak all your private data!

## link between server and the pages

The new crver API uses ramdisk and RAM filesystems to share info between the page and the server.

## CRVER API

c++ example using crver.hpp file
```c++
#include "includes/crver.hpp"
int main(int argc, crver::ENV argv) {
    MakeEnvironnemnt(env);
    using std::cout, crver::endl;

    cout << 
"<html>" <<
    "<body>" <<
        "Hello from C++ dynamic page! method: " << (env.request.method == crver::GET ? "GET" : "POST") << 
        " Bye Socket " << env.session.connexion << "!" << endl << 
        env.request.GET.size() << endl;

        for (auto i : env.request.GET) {
            cout << i.first << " = " << i.second << endl;
        }

cout <<
    "</body>"
"</html>";

    return crver::HTTPResponse();
}
```
![image](https://github.com/user-attachments/assets/1d8a0dbf-7f66-4c87-bf95-2c0991cf3e72)
