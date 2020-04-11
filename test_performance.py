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
