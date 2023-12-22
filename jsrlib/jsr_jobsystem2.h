#pragma once
#include <functional>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <tuple>
#include "jsrlib/jsr_semaphore.h"

namespace jsrlib {

	class JobSystem {
	public:
		typedef std::function<void(int)> jobfunc_t;

		JobSystem(int threadCount, int maxPendingJobs);
		JobSystem();
		~JobSystem();
		void submitJob(jobfunc_t fn, counting_semaphore* counter);
		int getWorkerCount() const;
	private:

		bool request_job(jobfunc_t* job);

		int m_threadCount;
		unsigned m_index, m_count;
		std::atomic_bool m_running;
		std::mutex m_mutex;
		std::condition_variable m_workerSignal;
		std::condition_variable m_clientSignal;
		std::vector<jobfunc_t> m_joblist;
		std::vector<std::thread> m_threads;
		std::vector<uint64_t> m_jobCounters;
	};
}