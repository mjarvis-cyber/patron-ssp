#include <Windows.h>

#include <monstars/object.h>

namespace monstars
{

ObjectHandle::ObjectHandle(HANDLE handle) : m_handle(handle) {}

ObjectHandle::~ObjectHandle()
{
    Reset();
}

void ObjectHandle::Reset(HANDLE other)
{
    if (m_handle)
    {
        CloseHandle(m_handle);
    }
    m_handle = other;
}

HANDLE ObjectHandle::Release()
{
    HANDLE handle = m_handle;
    m_handle = nullptr;
    return handle;
}

}  // namespace monstars