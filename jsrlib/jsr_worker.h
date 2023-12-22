#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <string>

namespace jsrlib {

	class worker_thread
	{
	public:
		worker_thread();
		virtual	~worker_thread();
		std::string			name() const;
		bool				running() const;
		bool				terminating() const;
		bool				start(const std::string& name);
		void				stop(bool wait);
		bool				work_done();
		void				wait();
		virtual void		signal();
		int					result() const;
	protected:
		virtual int			run();
	private:
		int					m_result;
		std::string			m_name;
		std::thread			m_thread;
		bool				m_worker;
		bool				m_running;
		bool				m_terminating;
		bool				m_gotwork;
		bool				m_done;
		std::mutex			m_mutex;
		std::condition_variable m_done_cond;
		std::condition_variable m_gotwork_cond;
	};
}