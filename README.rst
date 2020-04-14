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
    <a href="https://pepy.tech/project/dareblopy"><img src="https://pepy.tech/badge/dareblopy"></a>
    <a href="https://opensource.org/licenses/Apache-2.0"><img src="https://img.shields.io/badge/License-Apache%202.0-blue.svg"></a>
    <a href="https://api.travis-ci.com/podgorskiy/bimpy.svg?branch=master"><img src="https://travis-ci.org/podgorskiy/DareBlopy.svg?branch=master"></a>
  </p>


**Da**\ ta **Re**\ ading **Blo**\ cks for **Py**\ thon is a python module that provides collection of C++ backed data reading primitives.
It targets deep-learning needs, but it is framework agnostic.

Features:

* Direct reading of data from **ZIP** archives to bytes/numpy arrays.

* You can select jpeg backend: **libjpeg** or **libjpeg-turbo**, at runtime.

* Reading from tfrecords without tensorflow.

* Mounting several zip archive to virtual file system.

* Implemented fully in C++ for optimal performance.
