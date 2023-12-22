#include "jsr_worker.h"

#include <iostream>

jsrlib::worker_thread::worker_thread() :
	m_done(false),
	m_gotwork(false),
	m_running(false),
	m_terminating(false),
	m_worker(false)
{
}

jsrlib::worker_thread::~worker_thread()
{
	stop(false);
	if (m_thread.joinable())
	{
		m_thread.join();
	}
}

std::string jsrlib::worker_thread::name() const
{
	return m_name;
}

bool jsrlib::worker_thread::running() const
{
	return m_running;
}

bool jsrlib::worker_thread::terminating() const
{
	return m_terminating;
}

bool jsrlib::worker_thread::start(const std::string& name)
{
	if (running())
	{
		return false;
	}

	m_name = name;
	m_terminating = false;
	m_worker = true;
	m_thread = std::thread([&] {
		int ret{};
		try {

			if (m_worker)
			{
				while (true)
				{
					std::unique_lock<std::mutex> lck(m_mutex);
					if (m_gotwork)
					{
						m_gotwork = false;
						m_done = false;
						lck.unlock();
					}
					else
					{
						m_done = true;
						m_gotwork = false;
						m_done_cond.notify_one();
						m_gotwork_cond.wait(lck, [&] {return m_gotwork; });
						continue;
					}

					if (terminating())
					{
						break;
					}

					ret = run();
				}
				m_done = true;
				m_done_cond.notify_one();
			}
			else
			{
				ret = run();
			}
			m_result = ret;
		}
		catch (std::runtime_error err) {
			std::cerr << "Fatal error in WorkerThread:" << err.what() << std::endl;
		}
		
		m_running = false;

		});

	m_running = true;
	return true;
}

void jsrlib::worker_thread::stop(bool wait)
{
	if (m_worker && m_running)
	{
		std::unique_lock<std::mutex> lck(m_mutex);
		m_terminating = true;
		m_gotwork = true;
		m_gotwork_cond.notify_one();

		if (wait)
		{
			m_done_cond.wait(lck, [&] {return m_done; });
		}
	}
	else
	{
		m_terminating = true;
	}
}

bool jsrlib::worker_thread::work_done()
{
	std::unique_lock<std::mutex> lck(m_mutex);
	return m_done;
}

void jsrlib::worker_thread::wait()
{
	if (m_worker)
	{
		std::unique_lock<std::mutex> lck(m_mutex);
		m_done_cond.wait(lck, [&] {return m_done; });
	}
	else if (m_running && m_terminating)
	{
		if (m_thread.joinable()) m_thread.join();
	}
}

void jsrlib::worker_thread::signal()
{
	if (m_worker)
	{
		std::unique_lock<std::mutex> lck(m_mutex);
		m_gotwork = true;
		m_done = false;
		m_gotwork_cond.notify_one();
	}
	else
	{
		run();
	}
}

int jsrlib::worker_thread::result() const
{
	return m_result;
}

int jsrlib::worker_thread::run()
{
	return 0;
}
