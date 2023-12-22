#include "jsr_memblock.h"

namespace jsrlib {
	Memory::Memory()
	{
	}
	Memory::Memory(const void* data, size_t size)
	{
		m_storage.resize(size);
		memcpy(m_storage.data(), data, size);
	}
	void Memory::copyTo(void* dest)
	{
		memcpy(dest, m_storage.data(), m_storage.size());
	}
	const uint8_t* Memory::get() const { return m_storage.empty() ? nullptr : m_storage.data(); }
}