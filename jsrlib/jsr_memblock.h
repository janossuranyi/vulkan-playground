#pragma once

#include <cinttypes>
#include <vector>

namespace jsrlib {

	struct Pointer {
		void* data;
		unsigned size;
	};

	class Memory {
	public:
		Memory();

		Memory(const void* data, size_t size);

		template<class T>
		Memory(const T* data, size_t size);

		template<class T>
		Memory(const std::vector<T>& data);
		
		void copyTo(void* dest);

		const uint8_t* get() const;
		
		template<class T>
		const T* ptr() const;

	private:
		std::vector<uint8_t> m_storage;
	};

	template<class T>
	inline Memory::Memory(const T* data, size_t size) : Memory(data, sizeof(T) * size)
	{
	}

	template<class T>
	inline Memory::Memory(const std::vector<T>& data) : Memory(data.data(), sizeof(T) * data.size())
	{
	}
	template<class T>
	inline const T* Memory::ptr() const
	{
		return reinterpret_cast<T*>(m_storage.data());
	}
}