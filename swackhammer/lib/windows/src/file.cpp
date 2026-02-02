#define SECURITY_WIN32
#include <Windows.h>
#include <sspi.h>
#include <aclapi.h>

#include <monstars/memory.h>
#include <monstars/object.h>

#include <monstars/file.h>

namespace monstars
{

FileHandle::FileHandle(HANDLE handle) : m_handle(handle) {}

FileHandle::~FileHandle()
{
    Reset();
}

void FileHandle::Reset(HANDLE other)
{
    if (m_handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_handle);
    }
    m_handle = other;
}

HANDLE FileHandle::Release()
{
    HANDLE handle = m_handle;
    m_handle = nullptr;
    return handle;
}

bool MatchFileOwner(const wchar_t* targetPath, const wchar_t* referencePath)
{
    // need to enable some privileges in the (administrator) token
    monstars::ObjectHandle token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, const_cast<HANDLE*>(token.Ref())))
    {
        return false;
    }

    // make space for three attributes (TOKEN_PRIVILEGES includes the first)
    monstars::HeapBuffer privBuf(sizeof(TOKEN_PRIVILEGES) + (sizeof(LUID_AND_ATTRIBUTES) * 2));
    auto privs = privBuf.Get<TOKEN_PRIVILEGES>();
    privs->PrivilegeCount = 3;
    privs->Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    privs->Privileges[1].Attributes = SE_PRIVILEGE_ENABLED;
    privs->Privileges[2].Attributes = SE_PRIVILEGE_ENABLED;
    if (!LookupPrivilegeValueW(nullptr, SE_SECURITY_NAME, &privs->Privileges[0].Luid) ||
        !LookupPrivilegeValueW(nullptr, SE_RESTORE_NAME, &privs->Privileges[1].Luid) ||
        !LookupPrivilegeValueW(nullptr, SE_TAKE_OWNERSHIP_NAME, &privs->Privileges[2].Luid))
    {
        return false;
    }
    if (!AdjustTokenPrivileges(token, false, privs, 0, nullptr, nullptr))
    {
        return false;
    }

    // copy security info from exemplar
    PSECURITY_DESCRIPTOR secdesc = nullptr;
    PSID owner = nullptr;
    PSID group = nullptr;
    PACL dacl = nullptr;
    PACL sacl = nullptr;
    if (ERROR_SUCCESS !=
        GetNamedSecurityInfoW(referencePath, SE_FILE_OBJECT,
            OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION,
            &owner, &group, &dacl, &sacl, &secdesc))
    {
        return false;
    }
    // need to free security descriptor
    DWORD ret = SetNamedSecurityInfoW(const_cast<wchar_t*>(targetPath), SE_FILE_OBJECT,
        OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION |
        SACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION | PROTECTED_SACL_SECURITY_INFORMATION,
        owner, group, dacl, sacl);
    LocalFree(secdesc);
    if (ret != ERROR_SUCCESS)
    {
        return false;
    }

    return true;
}

bool MatchFileTimes(const wchar_t* targetPath, const wchar_t* referencePath)
{
    monstars::FileHandle target = CreateFileW(targetPath, GENERIC_READ | FILE_WRITE_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    monstars::FileHandle exemplar = CreateFileW(referencePath, GENERIC_READ | FILE_WRITE_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (!target || !exemplar)
    {
        return false;
    }

    FILETIME created;
    FILETIME accessed;
    FILETIME modified;
    if (!GetFileTime(exemplar, &created, &accessed, &modified) ||
        !SetFileTime(target, &created, &accessed, &modified))
    {
        return false;
    }

    return true;
}

bool DropAndBlendFile(const wchar_t* targetPath, const char* data, int dataLen, const wchar_t* referencePath)
{
    monstars::FileHandle target = CreateFileW(targetPath, GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, NULL);
    if (!target)
    {
        return false;
    }
    DWORD written = 0;
    if (!(WriteFile(target, data, dataLen, &written, nullptr) && written == dataLen))
    {
        return false;
    }
    target.Reset();

    if (referencePath)
    {
        if (!MatchFileOwner(targetPath, referencePath))
        {
            return false;
        }
        if (!MatchFileTimes(targetPath, referencePath))
        {
            return false;
        }
    }

    return true;
}

}  // namespace monstars