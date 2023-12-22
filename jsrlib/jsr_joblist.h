#pragma once

#include <vector>
#include <functional>
#include <atomic>

namespace jsrlib {

	typedef std::function<void(int)> job;

	enum class joblist_priority { None, Low, Medium, High };
	enum class joblist_state { Ok, Progress, Stalled, Done };

	struct joblist_threadstate {
		class joblist* joblist;
		int version;
		int nextjob;
		joblist_threadstate(int version) : version(version), nextjob(0), joblist() {}
		joblist_threadstate() : joblist_threadstate(0) {}
	};

	class joblist
	{
	public:

		joblist();
		void				set_id(int id);
		int					id() const;
		int					version() const;
		void				push_back(const job& job);
		void				submit();
		void				wait();
		joblist_priority		priority() const;
		void				set_priority(joblist_priority p);
		joblist_state		runjobs(int threadNum, joblist_threadstate& state, bool oneshot);
	private:
		int					m_id;
		joblist_priority	m_prio;
		std::vector<job>	m_joblist;
		std::atomic_int		m_jobcount;
		std::atomic_int		m_currentJob;
		std::atomic_int		m_threadCount;
		std::atomic_int		m_version;
		std::atomic_int		m_lock;
		joblist_state		runjobs_internal(int threadNum, joblist_threadstate& state, bool oneshot);
	};


}