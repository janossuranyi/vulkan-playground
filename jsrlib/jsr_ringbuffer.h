#pragma once
#include <cassert>
#include <array>
#include <mutex>
#include <chrono>
#include <utility>
#include <condition_variable>

namespace jsrlib {

	template<class T>
	class ringbuffer {
	public:
		ringbuffer(size_t capacity)
		{
			m_elemCount = 0;
			m_frontIndex = 0;
			m_buffer.resize(capacity);
			std::fill(m_buffer.begin(), m_buffer.end(), T{});
		}

		ringbuffer() : ringbuffer(default_capacity) {}

		size_t size() const { return m_buffer.size(); }

		size_t count() const { return m_elemCount; }

		bool empty() const { return m_elemCount == 0; }
		
		bool full() const { return m_elemCount == size(); }

		void push_back(const T& elem)
		{
			if ( full() )
			{
				pop_front();
			}
			store(elem);
		}

		T pop_front()
		{
			if (!empty())
			{
				return load();
			}
			else
			{
				return m_buffer[m_frontIndex];
			}
		}

		T operator[](size_t index) const
		{
			const size_t _index = (m_frontIndex + index) % size();
			return m_buffer[_index];
		}
		static const size_t default_capacity = 16;

	private:
		std::vector<T> m_buffer;
		size_t m_elemCount;





		size_t m_frontIndex;



		void store(const T& elem)
		{
			const size_t index = (m_frontIndex + m_elemCount) % size();
			m_buffer[index] = std::move(elem);
			m_elemCount++;
		}

		T load()
		{
			assert(m_elemCount > 0);
			T& val = m_buffer[m_frontIndex];
			m_frontIndex = (m_frontIndex + 1) % size();
			m_elemCount--;

			return std::move(val);
		}
	};

	template<class T>
	class threadsafe_ringbuffer {
	public:
		threadsafe_ringbuffer(size_t capacity);
		threadsafe_ringbuffer();

		template<class Rep, class Period>
		bool push_back(const T& elem, const std::chrono::duration<Rep, Period> wait = std::chrono::nanoseconds(0));

		bool push_back(const T& elem);

		bool try_push_back(const T& elem);
		T pop_front();
		bool pop_front(T& elem, const std::chrono::nanoseconds wait = std::chrono::nanoseconds(0));
		bool try_pop_front(T& elem);
		bool empty() const;
		bool full() const;

	private:
		ringbuffer<T> m_buffer;
		std::mutex m_mutex;
		std::condition_variable m_fullCond;
		std::condition_variable m_emptyCond;
	};

	template<class T>
	inline threadsafe_ringbuffer<T>::threadsafe_ringbuffer(size_t capacity) :
		m_buffer(capacity) {}

	template<class T>
	inline threadsafe_ringbuffer<T>::threadsafe_ringbuffer() : threadsafe_ringbuffer(ringbuffer::default_capacity) {}

	template<class T>
	template<class Rep, class Period>
	inline bool threadsafe_ringbuffer<T>::push_back(const T& elem, const std::chrono::duration<Rep, Period> wait)
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		std::cv_status cvstat = std::cv_status::no_timeout;
		while (m_buffer.full() && cvstat == std::cv_status::no_timeout) {

			if (wait.count() > 0) {
				cvstat = m_fullCond.wait_for(lock, wait);
			}
			else m_fullCond.wait(lock);
		}
		
		if (cvstat == std::cv_status::timeout) return false;

		m_buffer.push_back(elem);
		m_emptyCond.notify_one();

		return true;
	}

	template<class T>
	inline bool threadsafe_ringbuffer<T>::push_back(const T& elem)
	{
		return push_back(elem, std::chrono::nanoseconds(0));
	}

	template<class T>
	inline bool threadsafe_ringbuffer<T>::try_push_back(const T& elem)
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		if (m_buffer.full()) {
			return false;
		}

		m_buffer.push_back(elem);
		m_emptyCond.notify_one();

		return true;
	}

	template<class T>
	inline T threadsafe_ringbuffer<T>::pop_front()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		while (m_buffer.empty()) {
			m_emptyCond.wait(lock);
		}
		
		auto val = m_buffer.pop_front();
		m_fullCond.notify_one();

		return val;
	}

	template<class T>
	inline bool threadsafe_ringbuffer<T>::pop_front(T& elem, const std::chrono::nanoseconds wait)
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		std::cv_status cvstat = std::cv_status::no_timeout;
		while (m_buffer.empty() && cvstat == std::cv_status::no_timeout) {
			if (wait.count() > 0) {
				cvstat = m_emptyCond.wait_for(lock, wait);
			}
			else m_emptyCond.wait(lock);
		}

		if (cvstat == std::cv_status::timeout) return false;

		elem = m_buffer.pop_front();
		m_fullCond.notify_one();

		return true;
	}

	template<class T>
	inline bool threadsafe_ringbuffer<T>::try_pop_front(T& elem)
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		if (m_buffer.empty()) {
			return false;
		}

		elem = m_buffer.pop_front();
		return true;
	}

	template<class T>
	inline bool threadsafe_ringbuffer<T>::empty() const
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		return m_buffer.empty();
	}

	template<class T>
	inline bool threadsafe_ringbuffer<T>::full() const
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		return m_buffer.full();
	}
}