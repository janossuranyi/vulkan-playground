#include "jsr_jobsystem2.h"
#include "jsr_logger.h"

namespace jsrlib {

	JobSystem::JobSystem(int threadCount, int maxPendingJobs) : m_index(0),m_count(0),m_threadCount(threadCount)
	{
		m_joblist.resize(maxPendingJobs);
		m_jobCounters.resize(threadCount);

		m_running = true;
		for (int i = 0; i < threadCount; ++i) {
			m_threads.emplace_back([i, this]()
				{
					jobfunc_t job;
					while (request_job(&job))
					{
						job(i);
						++m_jobCounters[i];
					}
				});
		}
	}

	JobSystem::JobSystem() : JobSystem(2, 16)
	{
	}

	JobSystem::~JobSystem()
	{
		m_running = false;
		{
			std::unique_lock<std::mutex> lock(m_mutex);
			m_workerSignal.notify_all();
		}
		for (auto& it : m_threads) {
			it.join();
		}

		uint32_t total = 0;
		for (const auto& n : m_jobCounters) {
			total += n;
		}

		Info("JobSystem2: total processed jobs: %d", total);

		int j = 0;
		for (const auto& n : m_jobCounters) {
			Info("JobSystem2 Thread-%d total processed jobs: %d", j, n);
			++j;
		}

	}

	void JobSystem::submitJob(jobfunc_t fn, counting_semaphore* counter)
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_clientSignal.wait(lock, [this] {return m_count < m_joblist.size(); });
		
		if (counter)
		{
			counter->lock();
			m_joblist[(m_index + m_count) % m_joblist.size()] = [counter, fn](int id)
			{
				fn(id);
				counter->release();
			};
		}
		else
		{
			m_joblist[(m_index + m_count) % m_joblist.size()] = fn;
		}
		++m_count;
		m_workerSignal.notify_one();
	}

	int JobSystem::getWorkerCount() const
	{
		return m_threadCount;
	}

	bool JobSystem::request_job(jobfunc_t* job)
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_workerSignal.wait(lock, [this] { return m_count > 0 || m_running == false; });
		
		if (m_running) {
			*job = m_joblist[m_index];
			m_index = (m_index + 1) % m_joblist.size();
			m_count--;
			m_clientSignal.notify_one();
			return true;
		}

		return false;
	}

}
