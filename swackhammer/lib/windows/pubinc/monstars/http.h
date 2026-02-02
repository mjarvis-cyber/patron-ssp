#ifndef _MONSTARS_HTTP_H_
#define _MONSTARS_HTTP_H_

#include <Windows.h>
#include <winhttp.h>

// enable TLS/HTTPS
#ifndef TLS_ENABLED
    #define TLS_ENABLED 1
#endif
#if !TLS_ENABLED
    #pragma message("*** WARNING: TLS/HTTPS is disabled for this build ***")
#endif

namespace monstars
{

struct InternetHandle
{
public:
    InternetHandle(HINTERNET handle = nullptr);
    ~InternetHandle();

    void Reset(HINTERNET other = nullptr);
    HINTERNET Release();

    operator HINTERNET() const { return m_handle; };
    operator bool() const { return m_handle != nullptr; };
    void operator= (HINTERNET h) { Reset(h); };

private:
    HINTERNET m_handle;
};

class HttpClient
{
public:
    HttpClient(const wchar_t* hostname);
    
    bool PostRequest(const wchar_t* endpoint, const char* data, size_t dataLen,
        const wchar_t* contentType = L"application/octet-stream");

    bool Forbidden() const { return m_forbidden; };

    operator bool() const { return m_connected; };

private:
    const wchar_t* m_hostname = nullptr;

    bool m_connected = false;
    bool m_forbidden = false;

    InternetHandle m_session;
    InternetHandle m_connection;
};
    
}  // namespace monstars

#endif  // _MONSTARS_HTTP_H_