# Copyright 2019-2020 Stanislav Pidhorskyi
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#  http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ==============================================================================


try:
    from Queue import Queue, Empty
except ImportError:
    from queue import Queue, Empty
from threading import Thread, Lock, Event


def data_loader(yielder, collator=None, iteration_count=None, worker_count=1, queue_size=16):
    """ Return an iterator that retrieves objects from yielder and passes them through collator.

    Maintains a queue of given size and can run several worker threads. Intended to be used for asynchronous, buffered
    data loading. Uses threads instead of multiprocessing, so tensors can be uploaded to GPU in collator.

    There are many purposes that function :attr:`collator` can be used for, depending on your use case.

    - Reading data from disk or db
    - Data decoding, e.g. from JPEG.
    - Augmenting data, flipping, rotating adding nose, etc.
    - Concatenation of data, stacking to single ndarray, conversion to a tensor, uploading to GPU.
    - Data generation.

    Note:
        Sequential order of batches is guaranteed only if number of workers is 1 (Default), otherwise batches might
        be supplied out of order.

    Args:
        yielder (iterator): Input data, returns batches.
        collator (Callable, optional): Function for processing batches. Receives batch from yielder.
        Can return object of any type. Defaults to None.
        worker_count (int, optional): Number of workers, should be greater or equal to one. To process data in parallel
            and fully load CPU :attr:`worker_count` should be close to the number of CPU cores. Defaults to one.
        queue_size (int, optional): Maximum size of the queue, which is number of batches to buffer. Should be larger
            than :attr:`worker_count`. Typically, one would want this to be as large as possible to amortize all disk
            IO and computational costs. Downside of large value is increased RAM consumption. Defaults to 16.

    Returns:
        Iterator: An object that produces a sequence of batches. :meth:`next()` method of the iterator will return
        object that was produced by :attr:`collator` function


    Raises:
        StopIteration: When all data was iterated through. Stops the for loop.

    """

    class State:
        def __init__(self):
            self.lock = Lock()
            self.iteration_count = iteration_count
            self.quit_event = Event()
            self.queue = Queue(queue_size)
            self.active_workers = 0
            self.collator = collator

    def _worker(state):
        while not state.quit_event.is_set():
            try:
                b = next(yielder)
                if state.collator:
                    b = state.collator(b)
                state.queue.put(b)
            except StopIteration:
                break

        with state.lock:
            state.active_workers -= 1
            if state.active_workers == 0:
                state.queue.put(None)

    class Iterator:
        def __init__(self):
            self.state = State()

            self.workers = []
            self.state.active_workers = worker_count
            for i in range(worker_count):
                worker = Thread(target=_worker, args=(self.state, ))
                worker.daemon = True
                worker.start()
                self.workers.append(worker)

        def __len__(self):
            return self.state.iteration_count

        def __iter__(self):
            return self

        def __next__(self):
            item = self.state.queue.get()
            self.state.queue.task_done()
            if item is None:
                raise StopIteration
            return item

        def __del__(self):
            self.state.quit_event.set()
            while not self.state.queue.empty():
                self.state.queue.get(False)
                self.state.queue.task_done()
            for worker in self.workers:
                worker.join()

    return Iterator()
