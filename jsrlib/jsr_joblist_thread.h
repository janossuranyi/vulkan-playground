#pragma once

#include <mutex>
#include "jsr_worker.h"
#include "jsr_joblist.h"

namespace jsrlib {


	class joblist_thread : public worker_thread
	{
	public:
		static const int MAX_JOBLISTS = 32;
		joblist_thread();
		~joblist_thread();
		void start_thread(int id);
		void add_joblist(joblist* joblist);
	private:
		struct workerjob {
			joblist* joblist;
			int version;
		};
		
		workerjob m_joblists[MAX_JOBLISTS];
		unsigned int m_read_index;
		unsigned int m_write_index;
		unsigned int m_threadId;
		unsigned int m_jobnum;
		std::mutex m_mutex;

		int run() override;
	};
}