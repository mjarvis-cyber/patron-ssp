#include <Windows.h>

#include <monstars/mutex.h>

namespace monstars
{

MutexLock::MutexLock(const HANDLE mutex, DWORD timeout) : m_mutex(mutex)
{
    if (WAIT_OBJECT_0 == WaitForSingleObject(m_mutex, timeout))
    {
        m_held = true;
    }
}

MutexLock::~MutexLock()
{
    if (m_mutex && m_held)
    {
        ReleaseMutex(m_mutex);
    }
}

}  // namespace monstars