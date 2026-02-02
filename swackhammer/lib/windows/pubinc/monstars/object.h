#ifndef _MONSTARS_OBJECT_H_
#define _MONSTARS_OBJECT_H_

#include <Windows.h>


namespace monstars
{

struct ObjectHandle
{
public:
    ObjectHandle(HANDLE handle = nullptr);
    ~ObjectHandle();

    void Reset(HANDLE other = nullptr);
    HANDLE Release();

    const HANDLE* const Ref() const { return &m_handle; };

    operator HANDLE() const { return m_handle; };
    operator bool() const { return m_handle != nullptr; };
    void operator= (HANDLE h) { Reset(h); };

private:
    HANDLE m_handle;
};
    
}  // namespace monstars


#endif  // _MONSTARS_OBJECT_H_