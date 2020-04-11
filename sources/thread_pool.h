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
