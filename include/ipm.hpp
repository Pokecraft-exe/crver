#pragma once
#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <tchar.h>
#include <string>
#include <vector>
#pragma comment(lib, "user32.lib")

/*
*
*
*
* Simplify MS's Memory Mapped Files
*
*
*
*/

constexpr int IPM_FAILED = 0b0;
constexpr int IPM_SUCCESS = 0b1;
constexpr int IPM_OPEN = IPM_SUCCESS;
constexpr int IPM_FAILED_SIZE_OVERFLOW = 0b10;
constexpr int IPM_CLOSED = 0b100;

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
    HANDLE hMapFile;
    interProcessMemory<T>* buffer;
    int status;
    std::wstring fileName;
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
    };
    interProcessMemory(IN const std::wstring name) {
        this->buffer = NULL;
        Open(name);
    }
    ~interProcessMemory() {
        Close();
    }
    int Open(IN const std::wstring name) {
        this->fileName = name;
        this->fileSize = sizeof(interProcessMemory<T>);

        hMapFile = CreateFileMappingW(
            INVALID_HANDLE_VALUE,
            NULL,
            PAGE_READWRITE,
            0,
            this->fileSize,
            this->fileName.c_str());

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
    int Close() {
        if (this->status != IPM_SUCCESS) return IPM_CLOSED;
        if (buffer != NULL) {
            UnmapViewOfFile(buffer);
            buffer = NULL;
        }
        if (hMapFile != NULL) {
            CloseHandle(hMapFile);
            hMapFile = NULL;
        }
        this->status = IPM_CLOSED;
        return IPM_CLOSED;
    }
    int inline Ready() {
        return this->status;
    }
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
        HANDLE hMap = local->hMapFile;
        interProcessMemory<T>* local_backup = local;
        CopyMemory(local, local->buffer, local->fileSize);

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
        CopyMemory(local->buffer, local, local->fileSize);
        return IPM_SUCCESS;
    }
};
