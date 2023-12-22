#include <stdexcept>
#include "jsr_transient_buffer.h"

namespace jsrlib {
    transient_buffer::transient_buffer(size_t size_)
    {
        resize(size_);
    }
    size_t transient_buffer::size() const
    {
        return m_buffer.size();
    }
    size_t transient_buffer::bytes_allocated() const
    {
        return m_bytesAlloced;
    }
    bool transient_buffer::valid() const
    {
        return size() > 0;
    }
    bool transient_buffer::empty() const
    {
        return m_bytesAlloced < CACHE_LINE_ALIGNMENT;
    }
    bool transient_buffer::resize(size_t size_)
    {
        m_buffer.resize(size_);

        reset();

        return true;
    }
    void* transient_buffer::allocate(const size_t numBytes, const void* initial)
    {
        const size_t realBytes = (numBytes + CACHE_LINE_ALIGNMENT - 1) & ~(CACHE_LINE_ALIGNMENT - 1);
        const size_t offset = m_bytesAlloced.fetch_add(static_cast<int>(realBytes));

        if (offset >= m_buffer.size()) {
            throw std::overflow_error("TransientBuffer::allocate: memory overflow error !");
        }

        void* result = m_buffer.data() + offset;
        if (initial)
        {
            memcpy(result, initial, numBytes);
        }

        return result;
    }
    void transient_buffer::reset()
    {
        const auto bufferAligment = (uintptr_t)m_buffer.data() & (CACHE_LINE_ALIGNMENT - 1);
        m_bytesAlloced = static_cast<int>((bufferAligment == 0 ? 0 : CACHE_LINE_ALIGNMENT - bufferAligment));
    }
}