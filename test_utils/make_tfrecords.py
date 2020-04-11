import zipfile
import tqdm
import imageio
import numpy as np
import random
import os
import tensorflow as tf
import PIL.Image


def prepare_tfrecords():
    im_size = 256
    directory = os.path.dirname(os.path.abspath(__file__))

    os.makedirs(directory, exist_ok=True)

    archive = zipfile.ZipFile(os.path.join(directory, 'test_image_archive.zip'), 'r')

    names = archive.namelist()

    names = [x for x in names if x[-4:] == '.jpg'] * 6

    count = len(names)
    print("Count: %d" % count)

    random.shuffle(names)

    folds_count = 4
    folds = [[] for _ in range(folds_count)]

    count_per_fold = count // len(folds)
    for i in range(len(folds)):
            folds[i] += names[i * count_per_fold: (i + 1) * count_per_fold]

    for i in range(len(folds)):
        images = []
        for x in tqdm.tqdm(folds[i]):
            imgfile = archive.open(x)
            image = imageio.imread(imgfile.read())
            if image.ndim == 2:
                image = np.stack([image] * 3, axis=-1)
            # image = np.asarray(PIL.Image.fromarray(image).resize((32, 32)))
            images.append(image.transpose((2, 0, 1)))

        tfr_opt = tf.python_io.TFRecordOptions(tf.python_io.TFRecordCompressionType.NONE)

        part_path = "test-r%02d.tfrecords" % i

        tfr_writer = tf.python_io.TFRecordWriter(part_path, tfr_opt)

        for image in images:
            ex = tf.train.Example(features=tf.train.Features(feature={
                'shape': tf.train.Feature(int64_list=tf.train.Int64List(value=image.shape)),
                'data': tf.train.Feature(bytes_list=tf.train.BytesList(value=[image.tostring()]))}))
            tfr_writer.write(ex.SerializeToString())
        tfr_writer.close()


if __name__ == '__main__':
    prepare_tfrecords()
