#ifndef _MONSTARS_LOGGING_H_
#define _MONSTARS_LOGGING_H_

#include <Windows.h>


#ifdef _DEBUG
    #define DEBUG_PRINTW(fmt, ...)              \
    do {                                        \
        wchar_t printbuf[MAX_PATH];             \
        wsprintfW(printbuf, fmt, __VA_ARGS__);  \
        OutputDebugStringW(printbuf);           \
    } while (0);
#else
    #define DEBUG_PRINTW(...)
#endif

#define LOG_LINE_ERROR                                                           \
do {                                                                             \
    DEBUG_PRINTW(L"%S:%d - error 0x%lx\n", __FILE__, __LINE__, GetLastError());  \
} while (0);

#endif  // _MONSTARS_LOGGING_H_