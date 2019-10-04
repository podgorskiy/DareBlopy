try:
    from Queue import Queue, Empty
except ImportError:
    from queue import Queue, Empty
from threading import Thread, Lock, Event


def data_loader(yielder, collator=None, iteration_count=None, worker_count=1, queue_size=16):
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
