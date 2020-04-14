import unittest
import PIL
import PIL.Image
import zipfile
import numpy as np
import pickle
import dareblopy as db


class BasicFileOps(unittest.TestCase):
    def test_file_exist(self):
        fs = db.FileSystem()
        self.assertTrue(fs.exists('README.md'))

    def test_rename(self):
        fs = db.FileSystem()
        self.assertFalse(fs.exists('README2.md'))
        fs.rename('README.md', 'README2.md')
        self.assertTrue(fs.exists('README2.md'))
        fs.rename('README2.md', 'README.md')

    def test_seek_tell(self):
        fs = db.FileSystem()
        file = fs.open("test_utils/test_archive.zip")
        self.assertTrue(file)

        self.assertEqual(file.tell(), 0)
        file.seek(0, 2)
        size = file.tell()
        self.assertEqual(file.size(), size)

        file.seek(-1, 2)
        self.assertEqual(file.tell(), size - 1)
        file.seek(-100, 1)
        self.assertEqual(file.tell(), size - 101)
        file.seek(2, 1)
        self.assertEqual(file.tell(), size - 101 + 2)
        file.seek(0)
        self.assertEqual(file.tell(), 0)

    def test_zip_mounting(self):
        fs = db.FileSystem()
        zip = fs.open("test_utils/test_archive.zip", lockable=True)
        self.assertTrue(zip)

        fs.mount_archive(db.open_zip_archive(zip))

        s = fs.open('test.txt')

        self.assertEqual(s.read().decode('utf-8'), "asdasdasd")


class FileAndImageReadingOps(unittest.TestCase):
    def test_reading_to_bytes(self):
        f = open("test_utils/test_image.jpg", 'rb')
        b1 = f.read()
        f.close()

        b2 = db.open_as_bytes("test_utils/test_image.jpg")
        self.assertEqual(b1, b2)

    def test_reading_to_numpy(self):
        image = PIL.Image.open("test_utils/test_image.jpg")
        ndarray1 = np.array(image)

        ndarray2 = db.read_jpg_as_numpy("test_utils/test_image.jpg")

        ndarray3 = db.read_jpg_as_numpy("test_utils/test_image.jpg", True)

        PIL.Image.fromarray(ndarray3).save("test_image2.png")

        self.assertTrue(np.all(ndarray1 == ndarray2))

        mean_error = np.abs(ndarray1.astype(int) - ndarray3.astype(int)).mean()

        PIL.Image.fromarray(np.abs(ndarray1.astype(int) - ndarray3.astype(int)).astype(np.uint8)).save("test_image_diff.png")

        self.assertTrue(mean_error < 0.5)

    def test_reading_to_bytes_from_zip(self):
        archive = zipfile.ZipFile("test_utils/test_image_archive.zip", 'r')
        s = archive.open('0.jpg')
        b1 = s.read()

        archive = db.open_zip_archive("test_utils/test_image_archive.zip")
        b2 = archive.open_as_bytes('0.jpg')

        b3 = archive.open('0.jpg').read()

        self.assertEqual(b1, b2)
        self.assertEqual(b1, b3)

    def test_reading_to_numpy_from_zip(self):
        archive = zipfile.ZipFile("test_utils/test_image_archive.zip", 'r')
        s = archive.open('0.jpg')
        image = PIL.Image.open(s)
        ndarray1 = np.array(image)

        archive = db.open_zip_archive("test_utils/test_image_archive.zip")
        ndarray2 = archive.read_jpg_as_numpy('0.jpg')

        self.assertTrue(np.all(ndarray1 == ndarray2))


class TFRecordsReading(unittest.TestCase):
    def test_reading_record(self):
        rr = db.RecordReader('test_utils/test-small-r00.tfrecords')
        self.assertIsNotNone(rr)

        file_size, data_size, entries = rr.get_metadata()
        self.assertEqual(entries, 50)

        records = list(rr)

        self.assertEqual(len(records), 50)

        # reading ground truth records to confirm reading container was correct
        with open('test_utils/test-small-records-r00.pth', 'rb') as f:
            records_gt = pickle.load(f)

        self.assertEqual(records_gt, records)

    def test_record_yielder(self):
        record_yielder = db.RecordYielderBasic(['test_utils/test-small-r00.tfrecords',
                                                'test_utils/test-small-r01.tfrecords',
                                                'test_utils/test-small-r02.tfrecords',
                                                'test_utils/test-small-r03.tfrecords'])

        self.assertIsNotNone(record_yielder)
        records = []

        while True:
            try:
                batch = record_yielder.next_n(32)
                records += batch
            except StopIteration:
                break

        # reading ground truth records to confirm reading the container was correct
        records_gt = []
        for file in ['test_utils/test-small-records-r00.pth',
                     'test_utils/test-small-records-r01.pth',
                     'test_utils/test-small-records-r02.pth',
                     'test_utils/test-small-records-r03.pth']:
            with open(file, 'rb') as f:
                records_gt += pickle.load(f)

        self.assertEqual(records_gt, records)

    def test_record_yielder_randomized(self):
        record_yielder = db.RecordYielderRandomized(['test_utils/test-small-r00.tfrecords',
                                                     'test_utils/test-small-r01.tfrecords',
                                                     'test_utils/test-small-r02.tfrecords',
                                                     'test_utils/test-small-r03.tfrecords'],
                                                    buffer_size=16,
                                                    seed=0,
                                                    epoch=0)

        self.assertIsNotNone(record_yielder)
        records = []

        while True:
            try:
                batch = record_yielder.next_n(32)
                records += batch
            except StopIteration:
                break

        # reading ground truth records to confirm reading the container was correct
        records_gt = []
        for file in ['test_utils/test-small-records-r00.pth',
                     'test_utils/test-small-records-r01.pth',
                     'test_utils/test-small-records-r02.pth',
                     'test_utils/test-small-records-r03.pth']:
            with open(file, 'rb') as f:
                records_gt += pickle.load(f)

        self.assertNotEqual(records_gt, records)

        # Check that all records present, and they are unique.
        for record in records:
            self.assertIn(record, records_gt)
        for record_gt in records_gt:
            self.assertIn(record_gt, records)

        index = []

        for record in records:
            index.append(records_gt.index(record))

        # TODO: Check if sequence is random? For small `buffer_size` it's going to be random only at local scale.
        print(index)


class TFRecordsParsing(unittest.TestCase):
    def setUp(self):
        # reading records
        self.records = []
        for file in ['test_utils/test-small-records-r00.pth',
                     'test_utils/test-small-records-r01.pth',
                     'test_utils/test-small-records-r02.pth',
                     'test_utils/test-small-records-r03.pth']:
            with open(file, 'rb') as f:
                self.records += pickle.load(f)

        # reading ground-truth data
        self.images_gt = []
        for file in ['test_utils/test-small-images-r00.pth',
                     'test_utils/test-small-images-r01.pth',
                     'test_utils/test-small-images-r02.pth',
                     'test_utils/test-small-images-r03.pth']:
            with open(file, 'rb') as f:
                self.images_gt += pickle.load(f)

    def test_parsing_single_record(self):
        features = {
            'shape': db.FixedLenFeature([3], db.int64),
            'data': db.FixedLenFeature([], db.string)
        }

        parser = db.RecordParser(features)
        self.assertIsNotNone(parser)

        for record, image_gt in zip(self.records, self.images_gt):
            shape, data = parser.parse_single_example(record)

            self.assertTrue(np.all(shape == [3, 32, 32]))
            self.assertTrue(np.all(
                np.frombuffer(data[0], dtype=np.uint8).reshape(3, 32, 32) == image_gt
            ))

    def test_parsing_single_record_with_uint8_alternative_to_string(self):
        features_alternative = {
            'shape': db.FixedLenFeature([3], db.int64),
            'data': db.FixedLenFeature([3, 32, 32], db.uint8)
        }

        parser = db.RecordParser(features_alternative)
        self.assertIsNotNone(parser)

        for record, image_gt in zip(self.records, self.images_gt):
            shape, data = parser.parse_single_example(record)

            self.assertTrue(np.all(shape == [3, 32, 32]))
            self.assertTrue(np.all(data == image_gt))

    def test_parsing_records_in_batch(self):
        features_alternative = {
            'data': db.FixedLenFeature([3, 32, 32], db.uint8)
        }

        parser = db.RecordParser(features_alternative, False)
        self.assertIsNotNone(parser)

        data = parser.parse_example(self.records)[0]
        self.assertTrue(np.all(data == self.images_gt))

    def test_parsing_single_record_inplace(self):
        features = {
            'shape': db.FixedLenFeature([3], db.int64),
            'data': db.FixedLenFeature([], db.string)}

        parser = db.RecordParser(features)
        self.assertIsNotNone(parser)

        shape = np.zeros(3, dtype=np.int64)
        data = np.asarray([bytes()], dtype=object)

        for record, image_gt in zip(self.records, self.images_gt):
            parser.parse_single_example_inplace(record, [shape, data], 0)
            image = np.frombuffer(data[0], dtype=np.uint8).reshape(shape)
            self.assertTrue(np.all(image == image_gt))

    def test_parsing_single_record_inplace_with_uint8(self):
        features_alternative = {
            'data': db.FixedLenFeature([3, 32, 32], db.uint8)
        }

        parser = db.RecordParser(features_alternative, False)
        self.assertIsNotNone(parser)

        data = np.zeros([3, 32, 32], dtype=np.uint8)

        for record, image_gt in zip(self.records, self.images_gt):
            parser.parse_single_example_inplace(record, [data], 0)
            self.assertTrue(np.all(data == image_gt))


class DatasetIterator(unittest.TestCase):
    def setUp(self):
        # reading ground-truth data
        self.images_gt = []
        for file in ['test_utils/test-small-images-r00.pth']:
            with open(file, 'rb') as f:
                self.images_gt += pickle.load(f)

    def test_dataset_iterator(self):
        features = {
            'data': db.FixedLenFeature([3, 32, 32], db.uint8)
        }
        iterator = db.ParsedTFRecordsDatasetIterator(['test_utils/test-small-r00.tfrecords'],
                                                     features, 32, buffer_size=1)

        images = np.concatenate([x[0] for x in iterator], axis=0)
        self.assertTrue(np.all(images == self.images_gt))


if __name__ == '__main__':
    unittest.main()
