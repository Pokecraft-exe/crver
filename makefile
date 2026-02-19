########################################
# 1. Root / sudo detection
########################################
SUDO := $(shell if [ "$$(id -u)" -eq 0 ]; then echo ""; else echo "sudo"; fi)

########################################
# 2. Debian / apt check
########################################
APT_EXISTS := $(shell command -v apt-get 2>/dev/null)

ifeq ($(APT_EXISTS),)
$(error "Error: apt-get not found. This Makefile only supports Debian-based distributions.")
endif

########################################
# 3. Variables
########################################
CXX      := g++
CXXFLAGS := -I./include -I./msquic/src/inc/ -O3
LDLIBS   := -lmsquic
TARGET   := crver

SOURCES  := $(wildcard *.cpp)
OBJS     := $(SOURCES:.cpp=.o)
HEADERS  := $(wildcard include/*.hpp)

########################################
# 4. Build Rules
########################################
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDLIBS)

%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

########################################
# 5. Install
########################################
install: $(TARGET)
	@echo "Installing $(TARGET)..."

	$(SUDO) cp $(TARGET) /usr/local/bin/
	$(SUDO) chmod +x /usr/local/bin/$(TARGET)

	# Allow binding to ports <1024 without root
	$(SUDO) setcap 'cap_net_bind_service=+ep' /usr/local/bin/$(TARGET)

	$(SUDO) mkdir -p /etc/crver/
	$(SUDO) mkdir -p /var/cwww/src
	$(SUDO) mkdir -p /var/cwww/build
	$(SUDO) mkdir -p /var/cwww

	$(SUDO) cp examples/linux/config.cfg /etc/crver/
	$(SUDO) cp examples/linux/index.cpp /var/cwww/src/
	$(SUDO) cp -R sites-includes/ /var/cwww/

	# Build shared object
	$(SUDO) g++ -fPIC /var/cwww/src/index.cpp \
		-o /var/cwww/build/index.so \
		-I/var/cwww/sites-includes \
		-shared

	# Install systemd service
	$(SUDO) cp crver.service /etc/systemd/system/
	$(SUDO) systemctl daemon-reload
	$(SUDO) systemctl enable crver

	@echo ""
	@echo "Installation complete."
	@echo "Run: $(SUDO) systemctl start crver"

########################################
# 6. Clean
########################################
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all install clean
