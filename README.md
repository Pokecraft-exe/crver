# crver

**crver** is a proof-of-concept custom HTTP server designed to serve and request compiled dynamic pages.  
It works cross-platform on Windows and Linux and provides a lightweight C++ API for page-server interaction.

---

## Features

- Serves compiled dynamic pages efficiently.
- Cross-platform support:
  - Windows default WWW folder: `C:\testwww\`
  - Linux default WWW folder: `/var/cwww/build/`
- Minimal dependencies; built with `g++`/`clang` and `CMake`/`Ninja`.
- Designed for high-performance workloads with optional CPU pinning and worker pools.

---

## Requirements

- Linux or Windows
- C++17 compatible compiler (`g++` or `clang`)
- CMake (≥ 3.20 recommended)
- Ninja (installed with ./setup.sh)
- Standard build tools and libraries (`libssl-dev`, `pkg-config`, etc.)

---

## Installation (Linux)

```bash
# Clone the repository
git clone https://github.com/Pokecraft-exe/crver.git
cd crver

# Build and install dependencies
./setup.sh

# Build the server
make

# Install binary and systemd service
sudo make install

# Start the service
sudo systemctl start crver
```

Note: The service binds by default to port 80. The binary drops root privileges after binding.

## Usage

Run manually for testing:
```
sudo -u crver /usr/local/bin/crver
```

Service management:
```
sudo systemctl start crver
sudo systemctl stop crver
sudo systemctl status crver
sudo journalctl -u crver -f
```

## Project Structure

```
crver/
├── include/          # Header files for server compilaiton
├── examples/         # Example usage of crver
├── sites-includes/   # Headers for endpoint developpment
├── main.cpp          # Server entry point
├── Session.cpp       # Socket abstraction
├── Socket.cpp        # Binding, listeners, basic http logic
├── affinity.cpp      # Server CPU pin affinity
├── Worker.cpp        # Worker pool code
├── makefile          # Build instructions
├── crver.service     # Systemd service file
└── setup.sh          # Dependency installation script
```

## CRVER API

- Provides C++ abstractions to create dynamic pages and interact with the server.

- Uses shared memory via IPM for communication between server and page code.

- See examples/ for usage patterns.

# License

This project and its forks/releases are released under the Creative Commons License and affiliated to the original developper (pokecraft-exe).

<div style="display:flex;flex-direction:row;">
  <img src="https://mirrors.creativecommons.org/presskit/buttons/88x31/png/by-nc-sa.png" width="200px" style="float:right;"/>
  <img src="https://mirrors.creativecommons.org/presskit/buttons/88x31/png/by-nc-sa.eu.png" width="200px" style="float:left;"/>
</div>

## About

crver is intended as a proof-of-concept and experimental project for custom high-performance HTTP server development.
