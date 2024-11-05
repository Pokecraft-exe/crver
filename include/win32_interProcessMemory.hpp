#pragma once
#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <tchar.h>
#include <string>

#define BUF_SIZE 256

namespace interProcessMemory {
    bool send(
        IN wchar_t wsName[],
        IN char* wsMsg,
        IN size_t sLen);
    bool recieve(
        IN wchar_t wsName[], 
        OUT char* &wsMsg);
};

/*
* 
* 
* 
* Simplify MS's Memory Mapped Files
* 
* 
* 
*/

#ifndef _WIN32_INTERPROCESSMEMORY_
#define _WIN32_INTERPROCESSMEMORY_
#pragma comment(lib, "user32.lib")
bool interProcessMemory::send(
    IN wchar_t wsName[],
    IN char* wsMsg,
    IN size_t sLen)
{
    HANDLE hMapFile;
    LPCTSTR pBuf;

    hMapFile = CreateFileMapping(
        INVALID_HANDLE_VALUE,
        NULL,
        PAGE_READWRITE,
        0,
        BUF_SIZE,
        wsName);

    if (hMapFile == NULL)
        return 0;

    pBuf = (LPTSTR)MapViewOfFile(hMapFile,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        BUF_SIZE);

    if (pBuf == NULL)
    {
        CloseHandle(hMapFile);
        return 1;
    }

    std::string sSize = std::to_string(sLen) + " ";
    
    strncpy((char*)pBuf, sSize.c_str(), sSize.size());
    CopyMemory((PVOID)(pBuf + sSize.size()), wsMsg, sLen);

    _getch();
    UnmapViewOfFile(pBuf);
    CloseHandle(hMapFile);
}

bool interProcessMemory::recieve(
    IN wchar_t wsName[],
    OUT char* &wsMsg) {
    HANDLE hMapFile;
    LPCTSTR pBuf;

    hMapFile = OpenFileMapping(
        FILE_MAP_ALL_ACCESS,   // read/write access
        FALSE,                 // do not inherit the name
        wsName);               // name of mapping object

    if (hMapFile == NULL)
    {
        _tprintf(TEXT("Could not open file mapping object (%d).\n"),
            GetLastError());
        return 1;
    }

    pBuf = (LPTSTR)MapViewOfFile(hMapFile, // handle to map object
        FILE_MAP_ALL_ACCESS,  // read/write permission
        0,
        0,
        BUF_SIZE);

    if (pBuf == NULL)
    {
        _tprintf(TEXT("Could not map view of file (%d).\n"),
            GetLastError());

        CloseHandle(hMapFile);

        return 1;
    }

    MessageBox(NULL, pBuf, TEXT("Process2"), MB_OK);

    UnmapViewOfFile(pBuf);

    CloseHandle(hMapFile);
}

#endif // !_WIN32_INTERPROCESSMEMORY_
