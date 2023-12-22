#include "jsr_semaphore.h"

namespace jsrlib {
	counting_semaphore::counting_semaphore() : counting_semaphore(0)
	{
	}

	counting_semaphore::counting_semaphore(int initial) : m_counter(initial)
	{
	}

	void counting_semaphore::lock()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		++m_counter;
	}

	void counting_semaphore::release()
	{
		std::unique_lock<std::mutex> lock(m_mutex);

		--m_counter;

		if (m_counter == 0) {
			m_zeroCond.notify_all();
		}
	}
	
	void counting_semaphore::wait()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		while (m_counter > 0)
		{
			m_zeroCond.wait(lock);
		}
		m_counter = 0;
	}

	bool counting_semaphore::locked()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		const bool res = m_counter > 0;

		return res;
	}
}
