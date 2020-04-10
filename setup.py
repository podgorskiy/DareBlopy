# Copyright 2017-2020 Stanislav Pidhorskyi. All rights reserved.
# License: https://raw.githubusercontent.com/podgorskiy/bimpy/master/LICENSE.txt
#

from setuptools import setup, Extension, find_packages
from distutils.errors import *
from distutils.dep_util import newer_group
from distutils import log
from distutils.command.build_ext import build_ext

from codecs import open
import os
import sys
import platform
import re
import glob

target_os = 'none'

if sys.platform == 'darwin':
    target_os = 'darwin'
elif os.name == 'posix':
    target_os = 'posix'
elif platform.system() == 'Windows':
    target_os = 'win32'

here = os.path.abspath(os.path.dirname(__file__))

with open(os.path.join(here, 'README.rst'), encoding='utf-8') as f:
    long_description = f.read()


def filter_sources(sources):
    """Filters sources into c, cpp, objc and asm"""
    cpp_ext_match = re.compile(r'.*[.](cpp|cxx|cc)\Z', re.I).match
    c_ext_match = re.compile(r'.*[.](c|C)\Z', re.I).match
    objc_ext_match = re.compile(r'.*[.]m\Z', re.I).match
    asm_ext_match = re.compile(r'.*[.](asm|s|S)\Z', re.I).match

    c_sources = []
    cpp_sources = []
    objc_sources = []
    asm_sources = []
    other_sources = []
    for source in sources:
        if c_ext_match(source):
            c_sources.append(source)
        elif cpp_ext_match(source):
            cpp_sources.append(source)
        elif objc_ext_match(source):
            objc_sources.append(source)
        elif asm_ext_match(source):
            asm_sources.append(source)
        else:
            other_sources.append(source)
    return c_sources, cpp_sources, objc_sources, asm_sources, other_sources


def build_extension(self, ext):
    """Modified version of build_extension method from distutils.
       Can handle compiler args for different files"""

    sources = ext.sources
    if sources is None or not isinstance(sources, (list, tuple)):
        raise DistutilsSetupError(
              "in 'ext_modules' option (extension '%s'), "
              "'sources' must be present and must be "
              "a list of source filenames" % ext.name)

    sources = list(sources)
    ext_path = self.get_ext_fullpath(ext.name)
    depends = sources + ext.depends
    if not (self.force or newer_group(depends, ext_path, 'newer')):
        log.debug("skipping '%s' extension (up-to-date)", ext.name)
        return
    else:
        log.info("building '%s' extension", ext.name)

    sources = self.swig_sources(sources, ext)

    extra_args = ext.extra_compile_args or []
    extra_c_args = getattr(ext, "extra_compile_c_args", [])
    extra_cpp_args = getattr(ext, "extra_compile_cpp_args", [])
    extra_objc_args = getattr(ext, "extra_compile_objc_args", [])
    extra_asm_args = getattr(ext, "extra_compile_asm_args", [])
    file_specific_definitions = getattr(ext, "file_specific_definitions", {})
    asm_include = getattr(ext, "asm_include", [])

    macros = ext.define_macros[:]
    for undef in ext.undef_macros:
        macros.append((undef,))

    c_sources, cpp_sources, objc_sources, asm_sources, other_sources = filter_sources(sources)

    self.compiler.src_extensions += ['.asm']

    self.compiler.set_executable('assembler', ['nasm'])

    def _compile(src, args):
        obj = []
        for s in src:
            additional_macros = []
            if s in file_specific_definitions.keys():
                additional_macros += file_specific_definitions[s]
            obj += self.compiler.compile([s],
                                         output_dir=self.build_temp,
                                         macros=macros + additional_macros,
                                         include_dirs=ext.include_dirs,
                                         debug=self.debug,
                                         extra_postargs=extra_args + args,
                                         depends=ext.depends)
        return obj

    def _compile_asm(src):
        obj = []
        for s in src:
            additional_macros = []
            if s in file_specific_definitions.keys():
                additional_macros += file_specific_definitions[s]
            macros_, objects, extra_postargs, asm_args, build = \
                self.compiler._setup_compile(self.build_temp, macros + additional_macros, asm_include, [s],
                                             depends, extra_asm_args)

            for o in objects:
                try:
                    src, ext = build[o]
                except KeyError:
                    continue
                try:
                    self.spawn(self.compiler.assembler + extra_postargs + asm_args + ['-o', o, src])
                except DistutilsExecError as msg:
                    raise CompileError(msg)
            obj += objects

        return obj

    objects = []
    objects += _compile_asm(asm_sources)
    objects += _compile(c_sources, extra_c_args)
    objects += _compile(cpp_sources, extra_cpp_args)
    objects += _compile(objc_sources, extra_objc_args)
    objects += _compile(other_sources, [])

    self._built_objects = objects[:]
    if ext.extra_objects:
        objects.extend(ext.extra_objects)

    extra_args = ext.extra_link_args or []

    language = ext.language or self.compiler.detect_language(sources)
    self.compiler.link_shared_object(
        objects, ext_path,
        libraries=self.get_libraries(ext),
        library_dirs=ext.library_dirs,
        runtime_library_dirs=ext.runtime_library_dirs,
        extra_postargs=extra_args,
        export_symbols=self.get_export_symbols(ext),
        debug=self.debug,
        build_temp=self.build_temp,
        target_lang=language)

# patching
build_ext.build_extension = build_extension

fsal = list(glob.glob('fsal/sources/*.cpp'))
crc32c = list(glob.glob('crc32c/src/*.cc'))
zlib = list(glob.glob('zlib/*.c'))
lz4 = list(glob.glob('lz4/lib/*.c'))
dareblopy = list(glob.glob('sources/*.c*')) + list(glob.glob('sources/protobuf/*.c*'))

protobuf = """any_lite.cc arena.cc extension_set.cc generated_enum_util.cc
        generated_message_table_driven_lite.cc generated_message_util.cc implicit_weak_message.cc
        io/coded_stream.cc io/io_win32.cc io/strtod.cc io/zero_copy_stream.cc io/zero_copy_stream_impl.cc
        io/zero_copy_stream_impl_lite.cc message_lite.cc parse_context.cc repeated_field.cc stubs/bytestream.cc
        stubs/common.cc stubs/int128.cc stubs/status.cc stubs/statusor.cc stubs/stringpiece.cc stubs/stringprintf.cc
        stubs/structurally_valid.cc stubs/strutil.cc stubs/time.cc wire_format_lite.cc
        any.cc  any.pb.cc  api.pb.cc  compiler/importer.cc  compiler/parser.cc  descriptor.cc  descriptor.pb.cc
        descriptor_database.cc  duration.pb.cc  dynamic_message.cc  empty.pb.cc  extension_set_heavy.cc
        field_mask.pb.cc  generated_message_reflection.cc  generated_message_table_driven.cc  io/gzip_stream.cc
        io/printer.cc  io/tokenizer.cc  map_field.cc  message.cc  reflection_ops.cc  service.cc  source_context.pb.cc
        struct.pb.cc  stubs/mathlimits.cc  stubs/substitute.cc  text_format.cc  timestamp.pb.cc  type.pb.cc
        unknown_field_set.cc  util/delimited_message_util.cc  util/field_comparator.cc  util/field_mask_util.cc
        util/internal/datapiece.cc  util/internal/default_value_objectwriter.cc  util/internal/error_listener.cc
        util/internal/field_mask_utility.cc  util/internal/json_escaping.cc  util/internal/json_objectwriter.cc
        util/internal/json_stream_parser.cc  util/internal/object_writer.cc  util/internal/proto_writer.cc
        util/internal/protostream_objectsource.cc  util/internal/protostream_objectwriter.cc
        util/internal/type_info.cc  util/internal/type_info_test_helper.cc  util/internal/utility.cc
        util/json_util.cc  util/message_differencer.cc  util/time_util.cc  util/type_resolver_util.cc
        wire_format.cc  wrappers.pb.cc"""

protobuf = ['protobuf/src/google/protobuf/' + x for x in protobuf.split()]


jpeg_turbo = """jcapimin.c jcapistd.c jccoefct.c jccolor.c jcdctmgr.c jchuff.c
        jcicc.c jcinit.c jcmainct.c jcmarker.c jcmaster.c jcomapi.c jcparam.c
        jcphuff.c jcprepct.c jcsample.c jctrans.c jdapimin.c jdapistd.c jdatadst.c
        jdatasrc.c jdcoefct.c jdcolor.c jddctmgr.c jdhuff.c jdicc.c jdinput.c
        jdmainct.c jdmarker.c jdmaster.c jdmerge.c jdphuff.c jdpostct.c jdsample.c
        jdtrans.c jerror.c jfdctflt.c jfdctfst.c jfdctint.c jidctflt.c jidctfst.c
        jidctint.c jidctred.c jquant1.c jquant2.c jutils.c jmemmgr.c jmemnobs.c
        jaricom.c jcarith.c jdarith.c"""

jpeg_turbo = ['libjpeg-turbo/' + x for x in jpeg_turbo.split()]


jpeg_turbo_simd = """x86_64/jsimdcpu.asm x86_64/jfdctflt-sse.asm
    x86_64/jccolor-sse2.asm x86_64/jcgray-sse2.asm x86_64/jchuff-sse2.asm
    x86_64/jcphuff-sse2.asm x86_64/jcsample-sse2.asm x86_64/jdcolor-sse2.asm
    x86_64/jdmerge-sse2.asm x86_64/jdsample-sse2.asm x86_64/jfdctfst-sse2.asm
    x86_64/jfdctint-sse2.asm x86_64/jidctflt-sse2.asm x86_64/jidctfst-sse2.asm
    x86_64/jidctint-sse2.asm x86_64/jidctred-sse2.asm x86_64/jquantf-sse2.asm
    x86_64/jquanti-sse2.asm
    x86_64/jccolor-avx2.asm x86_64/jcgray-avx2.asm x86_64/jcsample-avx2.asm
    x86_64/jdcolor-avx2.asm x86_64/jdmerge-avx2.asm x86_64/jdsample-avx2.asm
    x86_64/jfdctint-avx2.asm x86_64/jidctint-avx2.asm x86_64/jquanti-avx2.asm x86_64/jsimd.c"""

jpeg_turbo_simd = ['libjpeg-turbo/simd/' + x for x in jpeg_turbo_simd.split()]


jpeg_vanila = """jmemnobs.c jaricom.c jcapimin.c jcapistd.c jcarith.c jccoefct.c jccolor.c
        jcdctmgr.c jchuff.c jcinit.c jcmainct.c jcmarker.c jcmaster.c jcomapi.c jcparam.c
        jcprepct.c jcsample.c jctrans.c jdapimin.c jdapistd.c jdarith.c jdatadst.c jdatasrc.c
        jdcoefct.c jdcolor.c jddctmgr.c jdhuff.c jdinput.c jdmainct.c jdmarker.c jdmaster.c
        jdmerge.c jdpostct.c jdsample.c jdtrans.c jerror.c jfdctflt.c jfdctfst.c jfdctint.c
        jidctflt.c jidctfst.c jidctint.c jquant1.c jquant2.c jutils.c jmemmgr.c"""

jpeg_vanila = ['libjpeg/' + x for x in jpeg_vanila.split()]


definitions = {
    'darwin': [('HAVE_SSE42', 0), ('HAVE_PTHREAD', 0)],
    'posix': [('HAVE_SSE42', 0), ('HAVE_PTHREAD', 0)],
    'win32': [('HAVE_SSE42', 0)],
}

file_specific_definitions = {}
for file in jpeg_turbo:
    file_specific_definitions[file] = [('TURBO', 0)]
for file in jpeg_turbo_simd:
    file_specific_definitions[file] = [('TURBO', 0)]
for file in jpeg_vanila:
    file_specific_definitions[file] = [('VANILA', 0)]

libs = {
    'darwin': [],
    'posix': ["rt", "m", "stdc++fs"],
    'win32': [],
}

extra_link = {
    'darwin': [],
    'posix': ['-static-libstdc++', '-static-libgcc'],
    'win32': [],
}

extra_compile_args = {
    'darwin': ['-fPIC', '-g', '-msse2', '-msse3', '-msse4', '-mpopcnt', '-funsafe-math-optimizations'],
    'posix': ['-fPIC', '-g', '-msse2', '-msse3', '-msse4', '-mpopcnt', '-funsafe-math-optimizations'],
    'win32': ['/MT', '/fp:fast', '/GL', '/GR-'],
}

extra_compile_cpp_args = {
    'darwin': ['-std=c++14', '-lstdc++fs', '-Ofast'],
    'posix': ['-std=c++14', '-lstdc++fs', '-Ofast'],
    'win32': [],
}

extra_compile_c_args = {
    'darwin': ['-std=c99', '-Ofast'],
    'posix': ['-std=c99', '-Ofast'],
    'win32': [],
}

extra_compile_asm_args = ['-DELF', '-D__x86_64__', '-DPIC', '-DTURBO', '-g', '-f elf64', '-Ox']

extension = Extension("_dareblopy",
                      jpeg_turbo + jpeg_vanila + jpeg_turbo_simd + dareblopy + fsal + crc32c + zlib + protobuf + lz4,
                             define_macros = definitions[target_os],
                             include_dirs=[
                                 "zlib",
                                 "fsal/sources",
                                 "lz4/lib",
                                 "pybind11/include",
                                 "crc32c/include",
                                 "protobuf/src",
                                 "sources",
                                 "configs"
                             ],
                             extra_compile_args=extra_compile_args[target_os],
                             extra_link_args=extra_link[target_os],
                             libraries = libs[target_os])

extension.extra_compile_cpp_args = extra_compile_cpp_args[target_os]
extension.extra_compile_c_args = extra_compile_c_args[target_os]
extension.file_specific_definitions = file_specific_definitions
extension.extra_compile_asm_args = extra_compile_asm_args
extension.asm = 'nasm'
extension.asm_include = ['libjpeg-turbo/simd/nasm/', 'libjpeg-turbo/simd/x86_64/']

setup(
    name='dareblopy',

    version='0.0.1',

    description='dareblopy',
    long_description=long_description,

    url='https://github.com/podgorskiy/dareblopy',

    author='Stanislav Pidhorskyi',
    author_email='stanislav@podgorskiy.com',

    license='MIT',

    classifiers=[
        'Development Status :: 3 - Alpha',
        'License :: OSI Approved :: MIT License',
        'Programming Language :: Python :: 2.7',
        'Programming Language :: Python :: 3.6',
    ],

    keywords='dareblopy',

    packages=['dareblopy'],

    ext_modules=[extension],
)
