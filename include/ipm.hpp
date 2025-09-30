#pragma once

// Platform detection
#ifdef _WIN32
#define IPM_WINDOWS
#elif defined(__linux__) || defined(__unix__)
#define IPM_LINUX
#endif

#define IN
#define OUT

// Platform-specific includes
#ifdef IPM_WINDOWS
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <tchar.h>
#pragma comment(lib, "user32.lib")
#endif

#ifdef IPM_LINUX
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <stdio.h>
#endif

#include <string>
#include <vector>

/*
*
*
*
* Cross-Platform Inter-Process Memory using Memory Mapped Files
*
*
*
*/

constexpr int IPM_FAILED = 0b0;
constexpr int IPM_SUCCESS = 0b1;
constexpr int IPM_OPEN = IPM_SUCCESS;
constexpr int IPM_FAILED_SIZE_OVERFLOW = 0b10;
constexpr int IPM_CLOSED = 0b100;

// Platform-specific type definitions
#ifdef IPM_WINDOWS
typedef HANDLE IPM_HANDLE;
typedef const wchar_t IPM_STRING;
#define IPM_STRINGIFY(X) L##X
#define IPM_INVALID_HANDLE INVALID_HANDLE_VALUE
#define IPM_NULL_HANDLE NULL
#endif

#ifdef IPM_LINUX
typedef int IPM_HANDLE;
typedef const char IPM_STRING;
#define IPM_STRINGIFY(X) X
#define IPM_INVALID_HANDLE -1
#define IPM_NULL_HANDLE -1
#endif

template<class T> class IPM_Global;
template<class T> class IPM_Local;

template<class T>
class interProcessMemory : public T {
private:
    friend class IPM_Local<T>;
    friend class IPM_Global<T>;
protected:
    IPM_Local<T> local;
    IPM_Global<T> global;
    size_t fileSize;
    IPM_HANDLE hMapFile;
    interProcessMemory<T>* buffer;
    int status;
    IPM_STRING* fileName; // store pointer to wide/char literal

#ifdef IPM_LINUX
    IPM_HANDLE fd; // File descriptor for Linux
#endif

public:
    IPM_Local<T> inline Local() { return this->local; };
    IPM_Global<T> inline Global() { return this->global; };
    inline char* Data() { return reinterpret_cast<char*>(this->buffer); };
    inline interProcessMemory<T>* Direct() { return this->buffer; };

    interProcessMemory() {
        this->buffer = NULL;
        this->local = IPM_Local<T>(this);
        this->global = IPM_Global<T>(this);
        this->status = IPM_CLOSED;
        this->hMapFile = IPM_NULL_HANDLE;
#ifdef IPM_LINUX
        this->fd = IPM_INVALID_HANDLE;
#endif
    };

#ifdef IPM_WINDOWS
    interProcessMemory(IN const std::wstring name) {
        this->buffer = NULL;
        this->hMapFile = IPM_NULL_HANDLE;
        Open(name);
    }
#endif

#ifdef IPM_LINUX
    interProcessMemory(IN const std::string name) {
        this->buffer = NULL;
        this->hMapFile = IPM_NULL_HANDLE;
        this->fd = IPM_INVALID_HANDLE;
        Open(name);
    }
#endif

    ~interProcessMemory() {
        Close();
    }

#ifdef IPM_WINDOWS
    int Open(IN const std::wstring name) {
        this->fileName = name.c_str();
        this->fileSize = sizeof(interProcessMemory<T>);

        hMapFile = CreateFileMappingW(
            INVALID_HANDLE_VALUE,
            NULL,
            PAGE_READWRITE,
            0,
            (DWORD)this->fileSize,
            this->fileName);

        if (hMapFile == NULL) {
            this->status = IPM_FAILED;
            return IPM_FAILED;
        }

        this->buffer = reinterpret_cast<interProcessMemory<T>*>(MapViewOfFile(hMapFile,
            FILE_MAP_ALL_ACCESS,
            0,
            0,
            this->fileSize));

        if (this->buffer == NULL)
        {
            CloseHandle(hMapFile);
            this->status = IPM_FAILED;
            return IPM_FAILED;
        }

        this->local = IPM_Local<T>(this);
        this->global = IPM_Global<T>(this);
        this->status = IPM_SUCCESS;
        return IPM_OPEN;
    }
#endif

#ifdef IPM_LINUX
    int Open(IN const std::string name) {
        this->fileName = name.c_str();
        this->fileSize = sizeof(interProcessMemory<T>);

        // Create shared memory object
        std::string shm_name = "/" + name; // POSIX shared memory names must start with '/'
        fd = shm_open(shm_name.c_str(), O_CREAT | O_RDWR, 0666);

        if (fd == -1) {
            this->status = IPM_FAILED;
            return IPM_FAILED;
        }

        // Set the size of the shared memory object
        if (ftruncate(fd, this->fileSize) == -1) {
            close(fd);
            fd = IPM_INVALID_HANDLE;
            this->status = IPM_FAILED;
            return IPM_FAILED;
        }

        // Map the shared memory object
        this->buffer = reinterpret_cast<interProcessMemory<T>*>(mmap(NULL, this->fileSize,
            PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));

        if (this->buffer == MAP_FAILED) {
            close(fd);
            fd = IPM_INVALID_HANDLE;
            this->buffer = NULL;
            this->status = IPM_FAILED;
            return IPM_FAILED;
        }

        this->hMapFile = fd; // Store fd in hMapFile for consistency
        this->local = IPM_Local<T>(this);
        this->global = IPM_Global<T>(this);
        this->status = IPM_SUCCESS;
        return IPM_OPEN;
    }
#endif

    int Close() {
        if (this->status != IPM_SUCCESS) return IPM_CLOSED;

#ifdef IPM_WINDOWS
        if (buffer != NULL) {
            UnmapViewOfFile(buffer);
            buffer = NULL;
        }
        if (hMapFile != NULL) {
            CloseHandle(hMapFile);
            hMapFile = NULL;
        }
#endif

#ifdef IPM_LINUX
        if (buffer != NULL) {
            munmap(buffer, fileSize);
            buffer = NULL;
        }
        if (fd != IPM_INVALID_HANDLE) {
            close(fd);
            fd = IPM_INVALID_HANDLE;
        }
        hMapFile = IPM_NULL_HANDLE;
#endif

        this->status = IPM_CLOSED;
        return IPM_CLOSED;
    }

    int inline Ready() {
        return this->status;
    }

    interProcessMemory<T>& operator=(const interProcessMemory<T>& obj) = delete; // prevent accidental recursion/copy
    interProcessMemory<T>& operator=(const T& obj) {
        static_cast<T&>(*this) = obj; // Assign the values properly
        return *this;
    }
};

template<class T>
class IPM_Local {
private:
    interProcessMemory<T>* local;
public:
    IPM_Local() {
        this->local = nullptr;
    }
    IPM_Local(interProcessMemory<T>* ipm) {
        this->local = ipm;
    }
    int update() {
        if (local == NULL) {
            return IPM_FAILED;
        }
        if (local->buffer == NULL) {
            return IPM_FAILED;
        }
        interProcessMemory<T>* buffer = local->buffer;
        IPM_HANDLE hMap = local->hMapFile;
        interProcessMemory<T>* local_backup = local;

#ifdef IPM_WINDOWS
        CopyMemory(local, local->buffer, local->fileSize);
#endif

#ifdef IPM_LINUX
        memcpy(local, local->buffer, local->fileSize);
#endif

        // ensure the communication data is not changed
        local->buffer = buffer;
        local->hMapFile = hMap;
        local->local = IPM_Local(local_backup);
        local->global = IPM_Global(local_backup);
        return IPM_SUCCESS;
    }
};

template<class T>
class IPM_Global {
private:
    interProcessMemory<T>* local;
public:
    IPM_Global() {
        this->local = nullptr;
    }
    IPM_Global(interProcessMemory<T>* ipm) {
        this->local = ipm;
    }
    int update() {
        if (local == NULL) {
            return IPM_FAILED;
        }
        if (local->buffer == NULL) {
            return IPM_FAILED;
        }

#ifdef IPM_WINDOWS
        CopyMemory(local->buffer, local, local->fileSize);
#endif

#ifdef IPM_LINUX
        memcpy(local->buffer, local, local->fileSize);
#endif

        return IPM_SUCCESS;
    }
};
