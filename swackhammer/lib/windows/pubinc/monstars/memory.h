#ifndef _MONSTARS_MEMORY_H_
#define _MONSTARS_MEMORY_H_

#include <Windows.h>

namespace monstars
{

void __cdecl memcpy(void* dest, size_t dest_size, void* src, size_t num);

void __cdecl memset(void* ptr, int value, size_t num);


class HeapBuffer
{
public:
    HeapBuffer(size_t size = 0);
    HeapBuffer(void* alloc, size_t size);
    HeapBuffer(void* alloc);

    ~HeapBuffer();

    // no copy constructor
    HeapBuffer(const HeapBuffer& other) noexcept = delete;

    // move constructor
    HeapBuffer(HeapBuffer&& other) noexcept;

    void Free();
    void Resize(size_t size);

    template <typename T = void>
    T* Release();

    template <typename T = char>
    T* Get() const;

    size_t Size() const { return m_size; };

    operator bool() const { return m_alloc != nullptr; };

private:
    static HANDLE s_heap;

    void* m_alloc = nullptr;
    size_t m_size = 0;
};


}  // namespace monstars

#include "../src/memory.ipp"

#endif  // _MONSTARS_MEMORY_H_