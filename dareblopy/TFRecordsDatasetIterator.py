import dareblopy as db
import time
import numpy as np


class TFRecordsDatasetIterator:
    def __init__(self, filenames, batch_size, buffer_size=1000, seed=np.uint64(time.time() * 1000), epoch=0):
        self.record_yielder = db.RecordYielderRandomized(filenames, buffer_size, seed, epoch)
        self.batch_size = batch_size

    def __next__(self):
        return self.record_yielder.next_n(self.batch_size)


class ParsedTFRecordsDatasetIterator:
    def __init__(self, filenames, features, batch_size, buffer_size=1000, seed=np.uint64(time.time() * 1000), epoch=0):
        self.parser = db.RecordParser(features, False)
        self.record_yielder = db.ParsedRecordYielderRandomized(self.parser, filenames, buffer_size, seed, epoch)
        self.batch_size = batch_size

    def __next__(self):
        return self.record_yielder.next_n(self.batch_size)
