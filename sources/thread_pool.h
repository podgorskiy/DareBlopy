#pragma once
#include <list>
#include <vector>
#include <functional>
#include <atomic>
#include <mutex>
#include <assert.h>
#include <thread>
#include <condition_variable>


class ThreadPool
{
public:
	typedef int threadIdx;
	typedef int blockDim;
	typedef std::function<void(threadIdx, blockDim)> Kernel;

	ThreadPool(int max_worker_count = 64);
	~ThreadPool();

	void ParallelFor(const Kernel* kernel, blockDim dim);

private:
	void Wait();
	void Worker();
	void SetWorkerCount(int count);
	void TerminateAll();

	threadIdx PopTaskFromQueue(int local_size);

	bool m_terminating;
	std::atomic<int> m_activeWorkers;
	const Kernel* m_kernel;
	blockDim m_blockDim;
	threadIdx m_tasksWaiting;

	std::vector<std::thread*> m_workers;
	std::mutex m_queueLock;
	std::condition_variable m_queuecheck;
	std::condition_variable m_emptyQueue;
};
