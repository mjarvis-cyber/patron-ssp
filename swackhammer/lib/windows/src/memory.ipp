namespace monstars
{

template <typename T>
T* HeapBuffer::Release()
{
    auto ptr = static_cast<T*>(m_alloc);
    m_alloc = nullptr;
    m_size = 0;

    return ptr;
}

template <typename T>
T* HeapBuffer::Get() const
{
    return static_cast<T*>(m_alloc);
}

}  // namespace monstars