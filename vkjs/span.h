#ifndef VKJS_SPAN_H_
#define VKJS_SPAN_H_

namespace vkjs {

	template<class T>
	class span {
	private:
		size_t _size;
		T* _data;
	public:
		span(size_t size, T* data);
		span() : _size(0), _data(nullptr) {}
		span(const span& other);
		span(span&& other);
		span<T>& operator=(const span<T>& other);
		span<T>& operator=(span<T>&& other);
		size_t size() const;
		const T* data() const;
		T* data();
		bool empty() const;
		void swap(span<T>& other);
		T& at(size_t index);
		const T& at(size_t index) const;
		T& operator[](size_t index);
		const T& operator[](size_t index) const;
	};

	template<class T>
	inline size_t span<T>::size() const
	{
		return _size;
	}
	template<class T>
	inline bool span<T>::empty() const
	{
		return _size == 0;
	}
	template<class T>
	inline void span<T>::swap(span<T>& other)
	{
		std::swap(_size, other._size);
		std::swap(_data, other._data);
	}
	template<class T>
	inline T& span<T>::at(size_t index)
	{
		if (index >= size) {
			throw std::runtime_error("Index out of bounds");
		}
		return _data[index];
	}
	template<class T>
	inline const T& span<T>::at(size_t index) const
	{
		if (index >= size) {
			throw std::runtime_error("Index out of bounds");
		}
		return _data[index];
	}
	template<class T>
	inline T& span<T>::operator[](size_t index)
	{
		return _data[index];
	}
	template<class T>
	inline const T& span<T>::operator[](size_t index) const
	{
		return _data[index];
	}
	template<class T>
	inline span<T>::span(size_t size, T* data)
	{
		_size = size;
		_data = data;
	}

	template<class T>
	inline span<T>::span(const span& other)
	{
		_size = other._size;
		_data = other._data;
	}
	template<class T>
	inline span<T>::span(span&& other)
	{
		_size = other._size;
		_data = other._data;
		other._size = 0;
		other._data = nullptr;
	}

	template<class T>
	inline span<T>& span<T>::operator=(const span<T>& other)
	{
		_size = other._size;
		_data = other._data;
		return *this;
	}

	template<class T>
	inline span<T>& span<T>::operator=(span<T>&& other)
	{
		_size = other._size;
		_data = other._data;
		other._size = 0;
		other._data = nullptr;
		return *this;
	}

	template<class T>
	inline T* span<T>::data()
	{
		return _data;
	}

	template<class T>
	inline const T* span<T>::data() const
	{
		return _data;
	}
}
#endif // !VKJS_SPAN_H_
