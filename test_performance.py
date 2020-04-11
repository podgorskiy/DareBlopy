import os
import sys
import PIL
import PIL.Image
import io
import zipfile
import numpy as np
import dareblopy as db
from test_utils import benchmark


bm = benchmark.Benchmark()


##################################################################
# Reading a file to bytes object
##################################################################
def read_to_bytes_native():
    for i in range(20000):
        f = open('test_utils/test_image.jpg', 'rb')
        b = f.read()


def read_to_bytes_db():
    for i in range(20000):
        b = db.open_as_bytes('test_utils/test_image.jpg')


bm.add('reading file to bytes',
       baseline=read_to_bytes_native,
       dareblopy=read_to_bytes_db)


##################################################################
# Reading a jpeg image to numpy array
##################################################################
def read_jpg_to_numpy_pil():
    for i in range(600):
        image = PIL.Image.open('test_utils/test_image.jpg')
        ndarray = np.array(image)


def read_jpg_to_numpy_db():
    for i in range(600):
        ndarray = db.read_jpg_as_numpy('test_utils/test_image.jpg')


def read_jpg_to_numpy_db_turbo():
    for i in range(600):
        ndarray = db.read_jpg_as_numpy('test_utils/test_image.jpg', True)


bm.add('reading jpeg image to numpy',
       baseline=read_to_bytes_native,
       dareblopy=read_to_bytes_db,
       dareblopy_turbo=read_jpg_to_numpy_db_turbo)


##################################################################
# Reading files to bytes object from zip archive
##################################################################
def read_jpg_bytes_from_zip_native():
    archive = zipfile.ZipFile("test_utils/test_image_archive.zip", 'r')

    for i in range(2000):
        s = archive.open('%d.jpg' % (i % 200))
        b = s.read()
        # picture_stream = io.BytesIO(b)
        # picture = PIL.Image.open(picture_stream)
        # picture.show()


def read_jpg_bytes_from_zip_db():
    archive = db.open_zip_archive("test_utils/test_image_archive.zip")

    for i in range(2000):
        b = archive.open_as_bytes('%d.jpg' % (i % 200))
        # picture_stream = io.BytesIO(b)
        # picture = PIL.Image.open(picture_stream)
        # picture.show()


bm.add('reading files to bytes from zip',
       baseline=read_to_bytes_native,
       dareblopy=read_jpg_bytes_from_zip_db,
       prehit=lambda: (db.open_zip_archive("test_utils/test_image_archive.zip"),
                       zipfile.ZipFile("test_utils/test_image_archive.zip", 'r')))


##################################################################
# Reading jpeg images to numpy array from zip archive
##################################################################
def read_jpg_to_numpy_from_zip_native():
    archive = zipfile.ZipFile("test_utils/test_image_archive.zip", 'r')

    for i in range(200):
        s = archive.open('%d.jpg' % i)
        image = PIL.Image.open(s)
        ndarray = np.array(image)


def read_jpg_to_numpy_from_zip_db():
    archive = db.open_zip_archive("test_utils/test_image_archive.zip")
    for i in range(200):
        ndarray = archive.read_jpg_as_numpy('%d.jpg' % i)


def read_jpg_to_numpy_from_zip_db_turbo():
    archive = db.open_zip_archive("test_utils/test_image_archive.zip")
    for i in range(200):
        ndarray = archive.read_jpg_as_numpy('%d.jpg' % i, True)


bm.add('reading jpeg to numpy from zip',
       baseline=read_jpg_to_numpy_from_zip_native,
       dareblopy=read_jpg_to_numpy_from_zip_db,
       dareblopy_turbo=read_jpg_to_numpy_from_zip_db_turbo,
       prehit=lambda: (db.open_zip_archive("test_utils/test_image_archive.zip"),
                       zipfile.ZipFile("test_utils/test_image_archive.zip", 'r')))


# Run everything and save plot
bm.run()


filenames = ['test_utils/test-r00.tfrecords',
             'test_utils/test-r01.tfrecords',
             'test_utils/test-r02.tfrecords',
             'test_utils/test-r03.tfrecords']

if not all(os.path.exists(x) for x in filenames):
    raise RuntimeError('Could not find tfrecords. You need to run test_utils/make_tfrecords.py')


@benchmark.timeit
def simple_reading_of_records():
    for filename in filenames:
        rr = db.RecordReader(filename)
        records = list(rr)


simple_reading_of_records()


@benchmark.timeit
def test_yielder_basic():
    record_yielder = db.RecordYielderBasic(filenames)
    while True:
        try:
            batch = record_yielder.next_n(32)
        except StopIteration:
            break


test_yielder_basic()


@benchmark.timeit
def test_yielder_randomized():
    features = {
        #'shape': db.FixedLenFeature([3], db.int64),
        'data': db.FixedLenFeature([3, 256, 256], db.uint8)
    }
    parser = db.RecordParser(features, False)
    record_yielder = db.ParsedRecordYielderRandomized(parser, filenames, 64, 1, 0)
    while True:
        try:
            batch = record_yielder.next_n(32)
        except StopIteration:
            break


test_yielder_randomized()


@benchmark.timeit
def test_yielder_randomized_parallel():
    features = {
        #'shape': db.FixedLenFeature([3], db.int64),
        'data': db.FixedLenFeature([3, 256, 256], db.uint8)
    }
    parser = db.RecordParser(features, True)
    record_yielder = db.ParsedRecordYielderRandomized(parser, filenames, 64, 1, 0)
    while True:
        try:
            batch = record_yielder.next_n(32)
        except StopIteration:
            break


test_yielder_randomized_parallel()


@benchmark.timeit
def test_ParsedTFRecordsDatasetIterator():
    features = {
        #'shape': db.FixedLenFeature([3], db.int64),
        'data': db.FixedLenFeature([3, 256, 256], db.uint8)
    }
    dl = db.data_loader(db.ParsedTFRecordsDatasetIterator(filenames, features, 32, 128))
    b = list(dl)


test_ParsedTFRecordsDatasetIterator()


rr = db.RecordReader('test_utils/test-small-r00.tfrecords')
records = list(rr)
record = records[0]

features = {
    'shape': db.FixedLenFeature([3], db.int64),
    'data': db.FixedLenFeature([], db.string)}

parser = db.RecordParser(features)

shape = np.zeros(3, dtype=np.int64)
data = np.asarray([bytes()], dtype=object)

parser.parse_single_example_inplace(record, [shape, data], 0)

data = np.frombuffer(data[0], dtype=np.uint8).reshape(shape)

print(shape)
print(data)


features = {
    'data': db.FixedLenFeature([3, 32, 32], db.uint8)}

parser = db.RecordParser(features)

data = np.zeros([3, 32, 32], dtype=np.uint8)

parser.parse_single_example_inplace(record, [data], 0)

print(data)

features = {
    'shape': db.FixedLenFeature([3], db.int64),
    'data': db.FixedLenFeature([], db.string)}

parser = db.RecordParser(features)

shape, data = parser.parse_single_example(record)

print(shape)
print(data)


features = {
    'shape': db.FixedLenFeature([3], db.int64),
    'data': db.FixedLenFeature([3, 32, 32], db.uint8)
}
    #'data': _vfsdl.FixedLenFeature([], _vfsdl.string)}

parser = db.RecordParser(features, False)

shape, data = parser.parse_example(records)

print(shape)
print(data)


data_gold = data

features={
    'shape': db.FixedLenFeature([3], db.int64),
    'data': db.FixedLenFeature([3, 32, 32], db.uint8)
}

parser = db.RecordParser(features, True)

shape, data = parser.parse_example(records)

print(shape.shape)
print(data.shape)

print(np.all(data_gold == data))

