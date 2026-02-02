#include <Windows.h>

#include <monstars/util.h>


namespace monstars
{

Cooldown::Cooldown(uint32_t milliseconds)
{
    Reset(milliseconds);
}

bool Cooldown::Elapsed(bool reset)
{
    GetSystemTimeAsFileTime(&m_current);

    if (m_current.dwLowDateTime > m_next.dwLowDateTime)
    {
        if (reset) Reset();

        return true;
    }

    return false;
}

void Cooldown::Reset(uint32_t milliseconds)
{
    GetSystemTimeAsFileTime(&m_current);

    if (milliseconds)
    {
        m_interval = milliseconds * c_msToFileTime;
    }

    m_next = m_current;
    m_next.dwLowDateTime += m_interval;
}

void Cooldown::Wait(bool reset)
{
    GetSystemTimeAsFileTime(&m_current);

    monstars::Finally([this, reset]() noexcept { if (reset) this->Reset(); });

    if (m_current.dwLowDateTime > m_next.dwLowDateTime) return;

    uint32_t remaining = (m_next.dwLowDateTime - m_current.dwLowDateTime) / 1000 / 10;
    ::Sleep(remaining);
    return;
}


}  // namespace monstars