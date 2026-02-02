#include <Windows.h>

#include <monstars/logging.h>
#include <monstars/http.h>

namespace monstars
{
InternetHandle::InternetHandle(HINTERNET handle) : m_handle(handle) {}

InternetHandle::~InternetHandle()
{
    Reset();
}

void InternetHandle::Reset(HINTERNET other)
{
    if (m_handle)
    {
        WinHttpCloseHandle(m_handle);
    }
    m_handle = other;
}

HINTERNET InternetHandle::Release()
{
    HINTERNET handle = m_handle;
    m_handle = nullptr;
    return handle;
}

HttpClient::HttpClient(const wchar_t* hostname)
    : m_hostname(hostname)
{
    m_session =
        WinHttpOpen(nullptr, WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (m_session)
    {
        m_connection = WinHttpConnect(m_session, hostname, INTERNET_DEFAULT_PORT, 0);
        if (m_connection)
        {
            m_connected = true;
        }
    }
}

bool HttpClient::PostRequest(const wchar_t* endpoint, const char* data, size_t dataLen, const wchar_t* contentType)
{
    if (!m_connected)
    {
        DEBUG_PRINTW(L"error: not connected\n");
        return false;
    }
    m_forbidden = false;

#if TLS_ENABLED
    DWORD req_flags = WINHTTP_FLAG_SECURE;
#else
    DWORD req_flags = 0;
#endif
    InternetHandle request = WinHttpOpenRequest(m_connection, L"POST", endpoint, nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, req_flags);
    if (request)
    {
#if TLS_ENABLED
        DWORD sec_flags = SECURITY_FLAG_IGNORE_ALL_CERT_ERRORS;
        if (WinHttpSetOption(request, WINHTTP_OPTION_SECURITY_FLAGS, &sec_flags, sizeof(sec_flags)))
#endif
        {
#pragma warning(suppress: 6385)
            if (WinHttpSendRequest(request, contentType, -1L, const_cast<char*>(data), static_cast<DWORD>(dataLen),
                static_cast<DWORD>(dataLen), NULL))
            {
                if (WinHttpReceiveResponse(request, nullptr))
                {
                    // only interested in status code
                    DWORD status = 0;
                    DWORD size = sizeof(status);
                    if (WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX))
                    {
                        DEBUG_PRINTW(L"HTTP server response: %lu\n", status);
                        if (status == HTTP_STATUS_FORBIDDEN)
                        {
                            m_forbidden = true;
                        }
                        return status == HTTP_STATUS_OK;
                    }
                }
            }
        }
    }

    LOG_LINE_ERROR;
    return false;
}

}  // namespace monstars