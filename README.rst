.. raw:: html

  <h1 align="center">
    <br>
    <img src="https://podgorskiy.com/static/dareblopylogo.svg">
    <br>
  </h1>
  <h4 align="center">Framework agnostic, faster data reading for DeepLearning.</h4>
  <h4 align="center">A native extension for Python built with C++ and <a href="https://github.com/pybind/pybind11" target="_blank">pybind11</a>.</h4>

  <p align="center">
    <a href="https://badge.fury.io/py/dareblopy"><img src="https://badge.fury.io/py/dareblopy.svg" alt="PyPI version" height="18"></a>
    <img src="https://img.shields.io/pypi/pyversions/dareblopy">
    <img src="https://travis-ci.org/podgorskiy/DareBlopy.svg?branch=master">
  </p>


**Da**\ ta **Re**\ ading **Blo**\ cks for **Py**\ thon is a python module that provides collection of C++ backed data reading primitives.
It targets deep-learning needs, but it is framework agnostic.

Features:

* Direct reading of data from **ZIP** archives to bytes/numpy arrays.

* You can select jpeg backend: **libjpeg** or **libjpeg-turbo**, at runtime.

* Reading from tfrecords without tensorflow.

* Mounting several zip archive to virtual file system.

* Implemented fully in C++ for optimal performance.
