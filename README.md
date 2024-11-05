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

Windows provides MMF (Memory Mapped Files) that is a file stored in RAM sharable between processes. This is what i (will) use to share infos between the page and the server.
