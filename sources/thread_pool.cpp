#include "thread_pool.h"


void ThreadPool::Worker()
{
	while (!m_terminating)
	{
		threadIdx task = PopTaskFromQueue();

		if (m_terminating)
		{
			if (task != -1)
			{
				--m_activeWorkers;
			}
			assert(m_activeWorkers >= 0);
			m_emptyQueue.notify_all();
			return;
		}

		(*m_kernel)(task, m_blockDim);

		--m_activeWorkers;
		if (m_activeWorkers == 0 && m_tasksWaiting == m_blockDim)
		{
			m_emptyQueue.notify_all();
		}
	}
}

ThreadPool::threadIdx ThreadPool::PopTaskFromQueue()
{
	std::unique_lock<std::mutex> locker(m_queueLock);

	if (m_tasksWaiting == m_blockDim)
	{
		if (m_terminating)
		{
			return -1;
		}

		m_queuecheck.wait(locker);
	}

	++m_activeWorkers;

	threadIdx task = m_tasksWaiting;
	++m_tasksWaiting;
	return task;
}

ThreadPool::ThreadPool(int max_worker_count): m_terminating(false), m_activeWorkers(0), m_blockDim(0), m_tasksWaiting(0)
{
	int count = std::thread::hardware_concurrency() - 1;
	count = count == 0 ? 1 : count;
	SetWorkerCount(std::min(count, max_worker_count));
	m_tasksWaiting = 0;
	m_blockDim = 0;
}

ThreadPool::~ThreadPool()
{
	TerminateAll();
}

void ThreadPool::TerminateAll()
{
	if (m_workers.empty())
	{
		return;
	}
	{
		std::unique_lock<std::mutex> locker(m_queueLock);
		m_tasksWaiting = m_blockDim = 0;
		m_terminating = true;
		m_queuecheck.notify_all();
	}
	for (auto it = m_workers.begin(); it != m_workers.end(); ++it)
	{
		m_queuecheck.notify_all();
		(*it)->join();
		delete (*it);
	}
	m_workers.clear();
	m_terminating = false;
}

void ThreadPool::ParallelFor(const Kernel* kernel, blockDim dim)
{
	if (m_terminating)
	{
		return;
	}
	{
		std::unique_lock<std::mutex> lock(m_queueLock);
		m_kernel = kernel;
		m_blockDim = dim;
		m_tasksWaiting = 0;
		m_queuecheck.notify_all();
	}
	Wait();
}

void ThreadPool::Wait()
{
	std::unique_lock<std::mutex> locker(m_queueLock);

	while (m_tasksWaiting != m_blockDim || m_activeWorkers != 0)
	{
		if (m_terminating)
		{
			return;
		}
		m_emptyQueue.wait(locker);
	}
}

void ThreadPool::SetWorkerCount(int count)
{
	if (m_activeWorkers != 0)
	{
		Wait();
	}
	TerminateAll();

	for (int i = 0; i < count; ++i)
	{
		m_workers.push_back(new std::thread(&ThreadPool::Worker, this));
	}

	m_activeWorkers = 0;
}
