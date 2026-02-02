#include <Windows.h>

#include <monstars/memory.h>

namespace monstars
{

// when building the release configuration, MSVC really wants to replace these for-loops
//   with the "real" memcpy/memset... but we're building with /NODEFAULTLIB
#ifndef _DEBUG
#pragma optimize("", off)
#endif

void __cdecl memcpy(void* dest, size_t dest_size, void* src, size_t num)
{
    for (size_t i = 0; i < num && i < dest_size; i++)
    {
        reinterpret_cast<char*>(dest)[i] = reinterpret_cast<char*>(src)[i];
    }
}

void __cdecl memset(void* ptr, int value, size_t num)
{
    for (size_t i = 0; i < num; i++)
    {
        reinterpret_cast<char*>(ptr)[i] = value;
    }
}

#ifndef _DEBUG
#pragma optimize("", on)
#endif

HANDLE HeapBuffer::s_heap;

HeapBuffer::HeapBuffer(size_t size)
{
    if (!s_heap)
    {
        s_heap = GetProcessHeap();
    }
    if (size)
    {
        m_alloc = HeapAlloc(s_heap, HEAP_ZERO_MEMORY, size);
        if (m_alloc)
        {
            m_size = size;
        }
    }
}

HeapBuffer::HeapBuffer(void* alloc, size_t size)
{
    if (s_heap)
    {
        m_alloc = alloc;
        m_size = size;
    }
}

HeapBuffer::HeapBuffer(void* alloc)
{
    if (s_heap)
    {
        m_alloc = alloc;
        m_size = HeapSize(s_heap, 0, alloc);
    }
}

HeapBuffer::~HeapBuffer()
{
    Free();
}

HeapBuffer::HeapBuffer(HeapBuffer&& other) noexcept : m_alloc(other.m_alloc), m_size(other.m_size)
{
    other.m_alloc = nullptr;
    other.m_size = 0;
}

void HeapBuffer::Free()
{
    if (m_alloc)
    {
        if (HeapFree(s_heap, 0, m_alloc))
        {
            m_alloc = nullptr;
            m_size = 0;
        }
    }
}

void HeapBuffer::Resize(size_t size)
{
    if (!s_heap)
    {
        s_heap = GetProcessHeap();
    }
    if (!size)
    {
        Free();
    }
    else if (m_alloc)
    {
        void* newalloc = HeapReAlloc(s_heap, HEAP_ZERO_MEMORY, m_alloc, size);
        if (newalloc)
        {
            m_alloc = newalloc;
            m_size = size;
        }
    }
    else
    {
        m_alloc = HeapAlloc(s_heap, HEAP_ZERO_MEMORY, size);
        if (m_alloc)
        {
            m_size = size;
        }
    }
}

}  // namespace monstars