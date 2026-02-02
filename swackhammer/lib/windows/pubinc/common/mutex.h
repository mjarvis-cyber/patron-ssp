#ifndef _COMMON_MUTEX_H_
#define _COMMON_MUTEX_H_

#include <Windows.h>


namespace common
{

class MutexLock
{
public:
    MutexLock(const HANDLE mutex, DWORD timeout = INFINITE);
    ~MutexLock();

    operator bool() const { return m_held; };

private:
    const HANDLE m_mutex;
    bool m_held = false;
};


}  // namespace common


#endif  // _COMMON_MUTEX_H_