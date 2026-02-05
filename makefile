
# 1. Variables
CXX      := g++
CXXFLAGS := -I./include -I./msquic/src/inc/ -O3
LDLIBS  := -lmsquic
TARGET   := crver

# 2. Wildcards: Automatically find all .cpp files
SOURCES  := $(wildcard *.cpp)
# Create a list of object files (.o) from the .cpp files
OBJS     := $(SOURCES:.cpp=.o)
# Track all .hpp files in the include directory
HEADERS  := $(wildcard include/*.hpp)

# 3. Main Rule
all: $(TARGET)

# 4. Linking: Depends on all object files
$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

# 5. Compilation: Each .o depends on its .cpp AND the headers
# This ensures that if any .hpp changes, the .cpp files recompile
%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 6. Installation (as defined previously)
install:
	sudo cp $(TARGET) /usr/local/bin/
	sudo chmod +x /usr/local/bin/$(TARGET)
	sudo setcap 'cap_net_bind_service=+ep' /usr/local/bin/$(TARGET)
	sudo mkdir -p /etc/crver/
	sudo cp examples/linux/config.cfg /etc/crver/
	sudo cp examples/linux/index.cpp /var/cwww/src/
	sudo cp -R sites-includes/ /var/cwww/
	sudo g++ -fPIC /var/cwww/src/index.cpp -o /var/cwww/build/index.so -I/var/cwww/sites-includes -shared
	sudo cp crver.service /etc/systemd/system/
	sudo systemctl daemon-reload
	sudo systemctl enable crver
	@echo "Installation complete. Run 'sudo systemctl start crver'"

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all install clean
