#ifndef _MONSTARS_UTIL_H_
#define _MONSTARS_UTIL_H_

#include <Windows.h>

#include <utility>

namespace monstars
{

// ----- cooldown helper ----- //

constexpr int c_msToFileTime = 1000 * 10;

class Cooldown
{
public:
    Cooldown(uint32_t milliseconds);

    bool Elapsed(bool reset = true);
    void Reset(uint32_t milliseconds = 0);
    void Wait(bool reset = true);

    explicit operator bool() { return Elapsed(true); };

private:
    FILETIME m_current;
    FILETIME m_next;
    uint32_t m_interval;
};


// ----- Finally() ----- //

template <typename T>
struct _FinallyHelper
{
    _FinallyHelper(T fin) : _fin(std::move(fin)) {};

    ~_FinallyHelper() noexcept
    {
        static_assert(noexcept(_fin()), "_fin must be noexept");
        _fin();
    };

    _FinallyHelper(_FinallyHelper&&) = delete;
    _FinallyHelper(const _FinallyHelper&) = delete;

    T _fin;
};

template <typename T>
_FinallyHelper<T> Finally(T fin)
{
    return { std::move(fin) };
};

}  // namespace monstars

#endif  // _MONSTARS_UTIL_H_