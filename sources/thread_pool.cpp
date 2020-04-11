//   Copyright 2019-2020 Stanislav Pidhorskyi
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.

#include "thread_pool.h"


void ThreadPool::Worker()
{
	int local_size = 8;

	while (!m_terminating)
	{
		threadIdx task = PopTaskFromQueue(local_size);

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

		for (int i = 0; i < local_size && task + i < m_blockDim; ++i)
		{
			(*m_kernel)(task + i, m_blockDim);
		}

		--m_activeWorkers;
		if (m_activeWorkers == 0 && m_tasksWaiting == m_blockDim)
		{
			m_emptyQueue.notify_all();
		}
	}
}

ThreadPool::threadIdx ThreadPool::PopTaskFromQueue(int local_size)
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
	m_tasksWaiting = std::min(m_tasksWaiting + local_size, m_blockDim);
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
