import unittest
import PIL
import PIL.Image
import zipfile
import numpy as np
import dareblopy as db


class BasicFileOps(unittest.TestCase):
    def test_file_exist(self):
        fs = db.FileSystem()
        self.assertTrue(fs.exists('README.rst'))

    def test_rename(self):
        fs = db.FileSystem()
        self.assertFalse(fs.exists('README2.rst'))
        fs.rename('README.rst', 'README2.rst')
        self.assertTrue(fs.exists('README2.rst'))
        fs.rename('README2.rst', 'README.rst')

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

    def test_reading_to_bytes_from_zip(self):
        archive = zipfile.ZipFile("test_utils/test_image_archive.zip", 'r')
        s = archive.open('0.jpg')
        b1 = s.read()

        archive = db.open_zip_archive("test_utils/test_image_archive.zip")
        b2 = archive.open_as_bytes('0.jpg')

        b3 = archive.open('0.jpg').read()

        self.assertEqual(b1, b2)
        self.assertEqual(b1, b3)


if __name__ == '__main__':
    unittest.main()
