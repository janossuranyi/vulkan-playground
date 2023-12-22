#pragma once

#include <vector>
#include <atomic>

namespace jsrlib {

	inline constexpr size_t CACHE_LINE_ALIGNMENT = 64;

	class transient_buffer
	{
	public:
		transient_buffer() {}
		transient_buffer(size_t size_);
		size_t			size() const;
		size_t			bytes_allocated() const;
		bool			valid() const;
		bool			empty() const;
		bool			resize(size_t size_);
		void*			allocate(const size_t numBytes, const void* initial = nullptr);
		template<class T>
		T*				allocate(const T&& src);
		template<class T, class...Args>
		T*				allocate(Args&&...);
		void			reset();
	private:
		std::vector<unsigned char>		m_buffer;
		std::atomic_int					m_bytesAlloced{ 0 };
	};

	template<class T>
	inline T* transient_buffer::allocate(const T&& src)
	{
		T* ptr = new (allocate(sizeof(T))) T(src);

		return ptr;
	}

	template<class T, class ...Args>
	inline T* transient_buffer::allocate(Args&& ...args)
	{
		T* ptr = new (allocate(sizeof(T))) T(std::forward<Args>(args)...);

		return ptr;
	}

}