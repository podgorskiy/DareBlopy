import zipfile
import tqdm
import imageio
import numpy as np
import random
import pickle
import os
import tensorflow as tf
import PIL.Image


def prepare_tfrecords():
    directory = os.path.dirname(os.path.abspath(__file__))

    os.makedirs(directory, exist_ok=True)

    archive = zipfile.ZipFile(os.path.join(directory, 'test_image_archive.zip'), 'r')

    names = archive.namelist()

    names = [x for x in names if x[-4:] == '.jpg']

    count = len(names)
    print("Count: %d" % count)

    random.seed(1)
    random.shuffle(names)

    folds_count = 4
    folds = [[] for _ in range(folds_count)]

    count_per_fold = count // len(folds)
    for i in range(len(folds)):
            folds[i] += names[i * count_per_fold: (i + 1) * count_per_fold]

    for i in range(len(folds)):
        images_small = []
        images_large = []
        for x in tqdm.tqdm(folds[i]):
            imgfile = archive.open(x)
            image = imageio.imread(imgfile.read())
            if image.ndim == 2:
                image = np.stack([image] * 3, axis=-1)
            image_small = np.asarray(PIL.Image.fromarray(image).resize((32, 32)))
            images_small.append(image_small.transpose((2, 0, 1)))
            images_large.append(image.transpose((2, 0, 1)))

        tfr_opt = tf.python_io.TFRecordOptions(tf.python_io.TFRecordCompressionType.NONE)

        part_path_large = "test-large-r%02d.tfrecords" % i
        part_path_small = "test-small-r%02d.tfrecords" % i
        part_path_images_pickle = "test-small-images-r%02d.pth" % i
        part_path_records_pickle = "test-small-records-r%02d.pth" % i

        tfr_writer = tf.python_io.TFRecordWriter(part_path_large, tfr_opt)
        for image in images_large * 6:
            ex = tf.train.Example(features=tf.train.Features(feature={
                'shape': tf.train.Feature(int64_list=tf.train.Int64List(value=image.shape)),
                'data': tf.train.Feature(bytes_list=tf.train.BytesList(value=[image.tostring()]))}))
            record = ex.SerializeToString()
            tfr_writer.write(record)
        tfr_writer.close()

        tfr_writer = tf.python_io.TFRecordWriter(part_path_small, tfr_opt)
        records = []
        for image in images_small:
            ex = tf.train.Example(features=tf.train.Features(feature={
                'shape': tf.train.Feature(int64_list=tf.train.Int64List(value=image.shape)),
                'data': tf.train.Feature(bytes_list=tf.train.BytesList(value=[image.tostring()]))}))
            record = ex.SerializeToString()
            records.append(record)
            tfr_writer.write(record)
        tfr_writer.close()

        # Dump source images as target for comparison for testing
        with open(part_path_images_pickle, 'wb') as f:
            pickle.dump(images_small, f)

        # Dump records as target for comparison for testing
        with open(part_path_records_pickle, 'wb') as f:
            pickle.dump(records, f)


if __name__ == '__main__':
    prepare_tfrecords()
