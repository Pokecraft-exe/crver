# crver
(PROOF OF CONCEPT) a custom http server to send compiled dynamic pages

the default port is 80
the incoming request accept permissions are total (loopback, local, www)
the default www folder is C:\testwww\ for windows and /var/cwww/build/ for linux

## link between server and the pages

The new crver API uses IPM (an abstract of Memory Mapped Files) to share info between the pages and the server.

## CRVER API

See the examples for the use of crver's c++ api

![image](https://github.com/user-attachments/assets/1d8a0dbf-7f66-4c87-bf95-2c0991cf3e72)

## License

The project, past and future releases, forks and codes, is under the CreativeCommons License:

<div style="display:flex;flex-direction:row;">
  <img src="https://mirrors.creativecommons.org/presskit/buttons/88x31/png/by-nc-sa.png" width="200px" style="float:right;"/>
  <img src="https://mirrors.creativecommons.org/presskit/buttons/88x31/png/by-nc-sa.eu.png" width="200px" style="float:left;"/>
</div>
