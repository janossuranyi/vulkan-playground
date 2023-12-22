#pragma once

#include <mutex>
#include <atomic>
#include <condition_variable>
#include <chrono>

namespace jsrlib {

	class counting_semaphore
	{
	public:
		counting_semaphore();

		counting_semaphore(int initial);

		// increment the counter
		void lock();
		
		// decrement the counter
		void release();
		
		// waits while conunter > 0
		void wait();

		bool locked();
	private:
		int m_counter;
		std::mutex m_mutex;
		std::condition_variable m_zeroCond;
	};

}