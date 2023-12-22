#include "jsr_joblist.h"

#include <cassert>
#include <thread>

namespace jsrlib {

	joblist::joblist() :
		m_jobcount(0),
		m_currentJob(0),
		m_threadCount(0),
		m_version(0),
		m_prio(joblist_priority::Low),
		m_lock(0),
		m_id(0)
	{
	}
	void joblist::set_id(int id)
	{
		m_id = id;
	}
	int joblist::id() const
	{
		return m_id;
	}
	void joblist::wait()
	{
		if (!m_joblist.empty())
		{
			while (m_jobcount > 0)
			{
				std::this_thread::yield();
			}

			++m_version;

			while (m_threadCount > 0)
			{
				std::this_thread::yield();
			}

			m_joblist.clear();
			m_jobcount = 0;
			m_currentJob = 0;
		}
	}
	int joblist::version() const
	{
		return m_version;
	}
	joblist_priority joblist::priority() const
	{
		return m_prio;
	}
	void joblist::push_back(const job& job)
	{
		m_joblist.push_back(job);
		++m_jobcount;
	}
	void joblist::submit()
	{
		if (m_joblist.empty()) return;
		m_currentJob = 0;
		joblist_threadstate state(version());
		state.joblist = this;
		runjobs(0, state, false);
	}
	void joblist::set_priority(joblist_priority p)
	{
		m_prio = p;
	}
	joblist_state joblist::runjobs(int threadNum, joblist_threadstate& state, bool oneshot)
	{
		++m_threadCount;
		auto result = runjobs_internal(threadNum, state, oneshot);
		--m_threadCount;

		return result;
	}
	joblist_state joblist::runjobs_internal(int threadNum, joblist_threadstate& state, bool oneshot)
	{
		assert(this == state.joblist);

		if (state.version != version()) {
			return joblist_state::Done;
		}

		joblist_state result{};

		do {
			state.nextjob = m_currentJob++;
/*
			if (++m_lock == 1)
			{
				state.nextjob = m_currentJob++;
				--m_lock;
			}
			else
			{
				--m_lock;
				return joblist_state::Stalled;
			}
*/
			if (state.nextjob >= m_joblist.size())
			{
				return joblist_state::Done;
			}

			m_joblist[state.nextjob](threadNum);
			--m_jobcount;

			result = joblist_state::Progress;

		} while (!oneshot);

		return result;
	}
}