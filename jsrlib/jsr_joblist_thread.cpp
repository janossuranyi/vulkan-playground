#include "jsr_joblist_thread.h"
#include "jsr_joblist.h"

namespace jsrlib {

	joblist_thread::joblist_thread() :
		m_jobnum(0),
		m_read_index(0),
		m_write_index(0),
		m_threadId(0),
		m_joblists()
	{
	}

	joblist_thread::~joblist_thread()
	{
		stop(true);
	}

	void joblist_thread::start_thread(int id)
	{
		m_threadId = id;
		char tmp[100];
		snprintf(tmp, 100, "jsrJobThread-%d", id);
		start(tmp);
	}

	void joblist_thread::add_joblist(joblist* joblist)
	{
		std::unique_lock<std::mutex> lck(m_mutex);
		while(m_write_index - m_read_index >= MAX_JOBLISTS)
		{
			std::this_thread::yield();
		}

		m_joblists[m_write_index & (MAX_JOBLISTS - 1)].joblist = joblist;
		m_joblists[m_write_index & (MAX_JOBLISTS - 1)].version = joblist->version();
		++m_write_index;
	}

	int joblist_thread::run()
	{
		joblist_threadstate localList[MAX_JOBLISTS];

		int numJoblists = 0;
		int lastStalled = -1;
		while (!terminating())
		{
			if (numJoblists < MAX_JOBLISTS && m_read_index < m_write_index)
			{
				localList[numJoblists].joblist = m_joblists[m_read_index & (MAX_JOBLISTS - 1)].joblist;
				localList[numJoblists].version = m_joblists[m_read_index & (MAX_JOBLISTS - 1)].version;
				localList[numJoblists].nextjob = -1;
				++numJoblists;
				++m_read_index;
			}
			if (numJoblists == 0)
			{
				break;
			}

			int currentJobList = 0;
			joblist_priority priority = joblist_priority::None;

			if (lastStalled < 0)
			{
				for (int i = 0; i < numJoblists; ++i)
				{
					if (localList[i].joblist->priority() > priority)
					{
						priority = localList[i].joblist->priority();
						currentJobList = i;
					}
				}
			}
			else
			{
				// try to hide the stall with a job from a list that has equal or higher priority
				currentJobList = lastStalled;
				priority = localList[lastStalled].joblist->priority();
				for (int i = 0; i < numJoblists; i++)
				{
					if (i != lastStalled && localList[i].joblist->priority() >= priority)
					{
						priority = localList[i].joblist->priority();
						currentJobList = i;
					}
				}

			}

			// if the priority is high then try to run through the whole list to reduce the overhead
			// otherwise run a single job and re-evaluate priorities for the next job
			bool single = localList[currentJobList].joblist->priority() == joblist_priority::High ? false : true;
			auto result = localList[currentJobList].joblist->runjobs(m_threadId, localList[currentJobList], single);

			++m_jobnum;

			if (result == joblist_state::Done)
			{
				for (int i = currentJobList; i < numJoblists - 1; ++i)
				{
					localList[i] = localList[i + 1];
				}
				--numJoblists;
			}
			else if (result == joblist_state::Stalled)
			{
				if (currentJobList == lastStalled)
				{
					std::this_thread::yield();
				}
				lastStalled = currentJobList;
			}
			else
			{
				lastStalled = -1;
			}
		}
		return 0;
	}

}
