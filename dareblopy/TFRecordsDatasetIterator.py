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


import dareblopy as db
import time
import numpy as np


class TFRecordsDatasetIterator:
    def __init__(self, filenames, batch_size, buffer_size=1000, seed=None, epoch=0):
        if seed is None:
            seed = np.uint64(time.time() * 1000)
        self.record_yielder = db.RecordYielderRandomized(filenames, buffer_size, seed, epoch)
        self.batch_size = batch_size

    def __iter__(self):
        return self

    def __next__(self):
        return self.record_yielder.next_n(self.batch_size)


class ParsedTFRecordsDatasetIterator:
    def __init__(self, filenames, features, batch_size, buffer_size=1000, seed=None, epoch=0):
        if seed is None:
            seed = np.uint64(time.time() * 1000)
        self.parser = db.RecordParser(features, True)
        self.record_yielder = db.ParsedRecordYielderRandomized(self.parser, filenames, buffer_size, seed, epoch)
        self.batch_size = batch_size

    def __iter__(self):
        return self

    def __next__(self):
        return self.record_yielder.next_n(self.batch_size)
