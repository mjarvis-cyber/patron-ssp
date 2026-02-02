#ifndef _MONSTARS_FILES_H_
#define _MONSTARS_FILES_H_

#include <Windows.h>

namespace monstars
{

struct FileHandle
{
public:
    FileHandle(HANDLE handle = nullptr);
    ~FileHandle();

    void Reset(HANDLE other = nullptr);
    HANDLE Release();

    const HANDLE* const Ref() const { return &m_handle; };

    operator HANDLE() const { return m_handle; };
    operator bool() const { return m_handle != INVALID_HANDLE_VALUE; };
    void operator= (HANDLE h) { Reset(h); };

private:
    HANDLE m_handle;
};


bool MatchFileOwner(const wchar_t* targetPath, const wchar_t* referencePath);

bool MatchFileTimes(const wchar_t* targetPath, const wchar_t* referencePath);

bool DropAndBlendFile(const wchar_t* targetPath, const char* data, int dataLen, const wchar_t* referencePath = nullptr);

}


#endif  // _MONSTARS_FILES_H_