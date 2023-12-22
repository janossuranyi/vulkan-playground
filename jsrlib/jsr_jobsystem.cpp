#include <thread>
#include <chrono>
#include <atomic>
#include <iostream>
#include "jsr_jobsystem.h"

namespace jsrlib {

	jobsystem* jobsystem::m_instance;
	std::mutex jobsystem::m_mutex;

	jobsystem::jobsystem()
	{
	}

	jobsystem::~jobsystem()
	{
		quit();
	}

	void jobsystem::init()
	{
		int k = 0;
		for (auto& it : m_joblistThreads)
		{
			it.start_thread(k++);
		}
		std::cout << "[jsrlib::JobSystem::init] " << m_joblistThreads.size() << " worker threads started" << std::endl;
	}

	void jobsystem::quit()
	{
		for (auto* it : m_joblists)
		{
			it->wait();
			delete it;
		}

		for (auto& it : m_joblistThreads)
		{
			it.stop(true);
		}
	}

	joblist* jobsystem::create_joblist(joblist_priority prio, int id)
	{
		joblist* obj = new joblist();
		obj->set_id(id);
		obj->set_priority(prio);
		m_joblists.push_back(obj);

		return obj;
	}

	joblist* jobsystem::get_joblist(int id)
	{
		for (auto* it : m_joblists)
		{
			if (it->id() == id)
			{
				return it;
			}
		}

		return nullptr;
	}

	void jobsystem::submit(joblist* list, int parallelism)
	{
		int numThreads = 1;
		if (parallelism == JOBLIST_PARALLELISM_DEFAULT)
		{
			numThreads = 1;
		}
		else if (parallelism == JOBLIST_PARALLELISM_MAX_THREADS)
		{
			numThreads = static_cast<int>(m_joblistThreads.size());
		}
		else if (parallelism == JOBLIST_PARALLELISM_MAX_CORES)
		{
			numThreads = static_cast<int>(std::min(m_joblistThreads.size(), (size_t)std::thread::hardware_concurrency()));
		}
		else if (parallelism > 0)
		{
			numThreads = static_cast<int>(std::min(m_joblistThreads.size(), (size_t)parallelism));
		}


		for (int i = 0; i < numThreads; ++i)
		{
			m_joblistThreads[i].add_joblist(list);
			m_joblistThreads[i].signal();
		}
	}

	jobsystem* jobsystem::singleton()
	{
		m_mutex.lock();

		if (!m_instance)
		{
			m_instance = new jobsystem();
			m_instance->init();
		}

		m_mutex.unlock();

		return m_instance;
	}

	void jobsystem::singleton_quit()
	{
		if (m_instance)
		{
			delete m_instance;
		}
	}

}