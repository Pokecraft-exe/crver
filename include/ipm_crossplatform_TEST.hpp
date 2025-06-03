#pragma once

#include <type_traits>
#include <string>
#include <vector>
#include <map>

// Platform detection
#if defined(_WIN32)
    #define IPM_WINDOWS
    #include <windows.h>
    #include <tchar.h>
#elif defined(__linux__)
    #define IPM_LINUX
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <cstring>
#else
    #error "Unsupported platform. Only Windows and Linux are supported."
#endif

constexpr int IPM_FAILED = 0b0;
constexpr int IPM_SUCCESS = 0b1;
constexpr int IPM_OPEN = IPM_SUCCESS;
constexpr int IPM_FAILED_SIZE_OVERFLOW = 0b10;
constexpr int IPM_CLOSED = 0b100;

// Type checking: forbid certain STL types
template<typename T>
struct is_disallowed_type : std::false_type {};

template<>
struct is_disallowed_type<std::string> : std::true_type {};

template<typename T>
struct is_disallowed_type<std::vector<T>> : std::true_type {};

template<typename K, typename V>
struct is_disallowed_type<std::map<K, V>> : std::true_type {};


template<class T>
class interProcessMemory {
    static_assert(!is_disallowed_type<T>::value, "interProcessMemory cannot be instantiated with disallowed STL types (e.g. string, vector, map)");

#ifdef IPM_WINDOWS
private:
    HANDLE hMapFile;
    void* buffer;
    std::wstring fileName;
    size_t fileSize;
    int status;

public:
    interProcessMemory(const std::wstring& name, size_t size) : fileName(name), fileSize(size), hMapFile(nullptr), buffer(nullptr), status(IPM_FAILED) {
        hMapFile = CreateFileMappingW(
            INVALID_HANDLE_VALUE,
            nullptr,
            PAGE_READWRITE,
            0,
            static_cast<DWORD>(fileSize),
            fileName.c_str()
        );

        if (hMapFile == nullptr) {
            status = IPM_FAILED;
            return;
        }

        buffer = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, fileSize);

        status = buffer ? IPM_SUCCESS : IPM_FAILED;
    }

    ~interProcessMemory() {
        if (buffer) UnmapViewOfFile(buffer);
        if (hMapFile) CloseHandle(hMapFile);
    }

    inline char* Data() { return reinterpret_cast<char*>(buffer); }
    inline int Status() const { return status; }
#endif

#ifdef IPM_LINUX
private:
    int shm_fd;
    void* buffer;
    std::string fileName;
    size_t fileSize;
    int status;

public:
    interProcessMemory(const std::string& name, size_t size) : fileName("/" + name), fileSize(size), shm_fd(-1), buffer(nullptr), status(IPM_FAILED) {
        shm_fd = shm_open(fileName.c_str(), O_CREAT | O_RDWR, 0666);
        if (shm_fd == -1) return;

        if (ftruncate(shm_fd, fileSize) == -1) return;

        buffer = mmap(0, fileSize, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        if (buffer == MAP_FAILED) {
            buffer = nullptr;
            return;
        }

        status = IPM_SUCCESS;
    }

    ~interProcessMemory() {
        if (buffer) munmap(buffer, fileSize);
        if (shm_fd != -1) close(shm_fd);
        shm_unlink(fileName.c_str());
    }

    inline char* Data() { return reinterpret_cast<char*>(buffer); }
    inline int Status() const { return status; }
#endif
};
