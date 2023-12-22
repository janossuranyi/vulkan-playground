#pragma once

#include <vector>
#include <array>
#include <functional>
#include <thread>
#include "jsr_joblist.h"
#include "jsr_joblist_thread.h"

namespace jsrlib {

	const int JOBLIST_PARALLELISM_DEFAULT = -1;
	const int JOBLIST_PARALLELISM_MAX_THREADS = -2;
	const int JOBLIST_PARALLELISM_MAX_CORES = -3;

	class jobsystem
	{
	public:
		void init();
		void quit();
		joblist* create_joblist(joblist_priority prio, int id);
		joblist* get_joblist(int id);
		void submit(joblist* list, int parallelism = JOBLIST_PARALLELISM_DEFAULT);
		static jobsystem* singleton();
		static void singleton_quit();
	private:
		jobsystem();
		~jobsystem();
		std::vector<joblist*> m_joblists;
		std::vector<joblist_thread> m_joblistThreads{ 32 };
		static jobsystem* m_instance;
		static std::mutex m_mutex;
	};

}