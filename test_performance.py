import os
import sys
import PIL
import PIL.Image
import io
import zipfile
import numpy as np
import matplotlib.pyplot as plt
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


##################################################################
# Benchmarking different record reading strategies
##################################################################
filenames = ['test_utils/test-large-r00.tfrecords',
             'test_utils/test-large-r01.tfrecords',
             'test_utils/test-large-r02.tfrecords',
             'test_utils/test-large-r03.tfrecords',
             'test_utils/test-large-r00.tfrecords',
             'test_utils/test-large-r01.tfrecords',
             'test_utils/test-large-r02.tfrecords',
             'test_utils/test-large-r03.tfrecords']

if not all(os.path.exists(x) for x in filenames):
    raise RuntimeError('Could not find tfrecords. You need to run test_utils/make_tfrecords.py')


results = []


@benchmark.timeit
def simple_reading_of_records():
    records = []
    for filename in filenames:
        rr = db.RecordReader(filename)
        records += list(rr)


results.append((simple_reading_of_records(), "Reading records with\nRecordReader\nNo parsing"))


@benchmark.timeit
def test_yielder_basic():
    record_yielder = db.RecordYielderBasic(filenames)
    records = []
    while True:
        try:
            records += record_yielder.next_n(32)
        except StopIteration:
            break


results.append((test_yielder_basic(), "Reading records with\nRecordYielderBasic\nNo parsing"))


@benchmark.timeit
def test_yielder_randomized():
    features = {
        #'shape': db.FixedLenFeature([3], db.int64),
        'data': db.FixedLenFeature([3, 256, 256], db.uint8)
    }
    parser = db.RecordParser(features, False)
    record_yielder = db.ParsedRecordYielderRandomized(parser, filenames, 64, 1, 0)
    records = []
    while True:
        try:
            records += record_yielder.next_n(32)
        except StopIteration:
            break


results.append((test_yielder_randomized(), "Reading records with\nParsedRecordYielderRandomized\nHas parsing"))


@benchmark.timeit
def test_yielder_randomized_parallel():
    features = {
        #'shape': db.FixedLenFeature([3], db.int64),
        'data': db.FixedLenFeature([3, 256, 256], db.uint8)
    }
    parser = db.RecordParser(features, True)
    record_yielder = db.ParsedRecordYielderRandomized(parser, filenames, 64, 1, 0)
    records = []
    while True:
        try:
            records += record_yielder.next_n(32)
        except StopIteration:
            break


results.append((test_yielder_randomized_parallel(), "Reading records with\nParsedRecordYielderRandomized\n +parallel parsing\nHas parsing"))


@benchmark.timeit
def test_ParsedTFRecordsDatasetIterator():
    features = {
        #'shape': db.FixedLenFeature([3], db.int64),
        'data': db.FixedLenFeature([3, 256, 256], db.uint8)
    }
    iterator = db.ParsedTFRecordsDatasetIterator(filenames, features, 32, 64)
    records = []
    for batch in iterator:
        records += batch


results.append((test_ParsedTFRecordsDatasetIterator(), "Reading records with\nParsedTFRecordsDatasetIterator\n +parallel parsing\nHas parsing"))


@benchmark.timeit
def test_ParsedTFRecordsDatasetIterator_and_dataloader():
    features = {
        #'shape': db.FixedLenFeature([3], db.int64),
        'data': db.FixedLenFeature([3, 256, 256], db.uint8)
    }
    iterator = db.data_loader(db.ParsedTFRecordsDatasetIterator(filenames, features, 32, 64), worker_count=6)
    records = []
    for batch in iterator:
        records += batch


results.append((test_ParsedTFRecordsDatasetIterator_and_dataloader(), "Reading records with\nParsedTFRecordsDatasetIterator\n+multy worker dataloader\nHas parsing"))


fig, ax = plt.subplots(figsize=(16, 6), dpi=120, facecolor='w', edgecolor='k')
x = np.arange(len(results))
width = 0.2
rects1 = ax.bar(x, [x[0] for x in results], width)
# Add some text for labels, title and custom x-axis tick labels, etc.
ax.set_ylabel('Running time, [ms]. Lower is better')
ax.set_title('Reading tfrecords')
ax.set_xticks(x)
ax.set_xticklabels([x[1] for x in results])


def autolabel(rects, show=None):
    if show is None:
        show = np.ones(len(rects))
    for rect, s in zip(rects, show):
        if s:
            height = rect.get_height()
            ax.annotate('{:.2f}'.format(height),
                        xy=(rect.get_x() + rect.get_width() / 2, height),
                        xytext=(0, 3),
                        textcoords="offset points",
                        ha='center', va='bottom')


autolabel(rects1)
fig.tight_layout()

fig.savefig('test_utils/benchmark_reading_tfrecords.png')


import tensorflow as tf


filenames = ['test_utils/test-large-r00.tfrecords',
             'test_utils/test-large-r01.tfrecords',
             'test_utils/test-large-r02.tfrecords',
             'test_utils/test-large-r03.tfrecords',
             'test_utils/test-large-r00.tfrecords',
             'test_utils/test-large-r01.tfrecords',
             'test_utils/test-large-r02.tfrecords',
             'test_utils/test-large-r03.tfrecords']

results = []



@benchmark.timeit
def reading_tf_records_from_tensorflow_withoutdecoding():
    raw_dataset = tf.data.TFRecordDataset(filenames)

    feature_description = {
        'data': tf.io.FixedLenFeature([], tf.string)
    }

    records = []
    for batch in raw_dataset.batch(32, drop_remainder=True):
        s = tf.io.parse_example(batch, feature_description)['data']
        records.append(s)



results.append((reading_tf_records_from_tensorflow_withoutdecoding(), "Reading rfrecords with\nTensorflow\nwithout decoding"))


@benchmark.timeit
def reading_tf_records_from_tensorflow():
    raw_dataset = tf.data.TFRecordDataset(filenames)

    feature_description = {
        'data': tf.io.FixedLenFeature([], tf.string)
    }

    records = []
    for batch in raw_dataset.batch(32, drop_remainder=True):
        s = tf.io.parse_example(batch, feature_description)['data']
        data = tf.reshape(tf.io.decode_raw(s, tf.uint8), [-1, 3, 256, 256])
        records.append(data)



results.append((reading_tf_records_from_tensorflow(), "Reading rfrecords with\nTensorflow"))


@benchmark.timeit
def reading_tf_records_from_dareblopy():

    features = {
        'data': db.FixedLenFeature([3, 256, 256], db.uint8)
    }
    iterator = db.data_loader(db.ParsedTFRecordsDatasetIterator(filenames, features, 32, 64), worker_count=6)
    records = []
    for batch in iterator:
        records += batch


results.append((reading_tf_records_from_dareblopy(), "Reading rfrecords with\nDareBlopy"))
