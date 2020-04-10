cmake_minimum_required(VERSION 3.6)

project(DareBlopy)

option(USE_MSVC_RUNTIME_LIBRARY_DLL OFF)
option(PNG_SHARED "Build shared lib" OFF)
option(PNG_STATIC "Build static lib" ON)
option(PNG_TESTS  "Build libpng tests" OFF)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
set(REQUIRE_SIMD ON "Generate a fatal error if SIMD extensions are not available for this platform (default is to fall back to a non-SIMD build)" FORCE)
set(WITH_ARITH_DEC ON "Include arithmetic decoding support when emulating the libjpeg v6b API/ABI" FORCE)
set(WITH_ARITH_ENC ON "Include arithmetic encoding support when emulating the libjpeg v6b API/ABI" FORCE)
set(WITH_JPEG8 ON "Emulate libjpeg v8 API/ABI (this makes ${CMAKE_PROJECT_NAME} backward-incompatible with libjpeg v6b)" FORCE)
set(WITH_SIMD ON "Include SIMD extensions, if available for this platform" FORCE)
set(ENABLE_STATIC ON "Build static libraries" FORCE)
set(ENABLE_SHARED FALSE "Build shared libraries" FORCE)

set(WITH_JPEG8 FORCE)

math(EXPR BITS "${CMAKE_SIZEOF_VOID_P} * 8")
string(TOLOWER ${CMAKE_SYSTEM_PROCESSOR} CMAKE_SYSTEM_PROCESSOR_LC)
if(CMAKE_SYSTEM_PROCESSOR_LC MATCHES "x86_64" OR
        CMAKE_SYSTEM_PROCESSOR_LC MATCHES "amd64" OR
        CMAKE_SYSTEM_PROCESSOR_LC MATCHES "i[0-9]86" OR
        CMAKE_SYSTEM_PROCESSOR_LC MATCHES "x86" OR
        CMAKE_SYSTEM_PROCESSOR_LC MATCHES "ia32")
    if(BITS EQUAL 64)
        set(CPU_TYPE x86_64)
    else()
        set(CPU_TYPE i386)
    endif()
    if(NOT CMAKE_SYSTEM_PROCESSOR STREQUAL ${CPU_TYPE})
        set(CMAKE_SYSTEM_PROCESSOR ${CPU_TYPE})
    endif()
elseif(CMAKE_SYSTEM_PROCESSOR_LC STREQUAL "aarch64" OR
        CMAKE_SYSTEM_PROCESSOR_LC MATCHES "arm*64*")
    set(CPU_TYPE arm64)
elseif(CMAKE_SYSTEM_PROCESSOR_LC MATCHES "arm*")
    set(CPU_TYPE arm)
elseif(CMAKE_SYSTEM_PROCESSOR_LC MATCHES "ppc*" OR
        CMAKE_SYSTEM_PROCESSOR_LC MATCHES "powerpc*")
    set(CPU_TYPE powerpc)
else()
    set(CPU_TYPE ${CMAKE_SYSTEM_PROCESSOR_LC})
endif()
message(STATUS "${BITS}-bit build (${CPU_TYPE})")

include(CheckTypeSize)
include(CheckFunctionExists)
include(CheckIncludeFile)

FUNCTION(PREPEND var prefix)
    SET(listVar "")
    FOREACH(f ${ARGN})
        LIST(APPEND listVar "${prefix}/${f}")
    ENDFOREACH(f)
    SET(${var} "${listVar}" PARENT_SCOPE)
ENDFUNCTION(PREPEND)

find_package (Python 3 COMPONENTS Interpreter Development REQUIRED)
find_package(PythonLibs 3 REQUIRED)

message("Python_FOUND:${Python_FOUND}")
message("Python_VERSION:${Python_VERSION}")
message("Python_Development_FOUND:${Python_Development_FOUND}")
message("Python_LIBRARIES:${Python_LIBRARIES}")
message("PYTHON_LIBRARY_DIR:${PYTHON_LIBRARY_DIR}")

include_directories(${Python_INCLUDE_DIRS} )


##############################################################
# Setup compiler options and configurations
##############################################################

if(MSVC)
    set(COMMON_OPTIONS /W3 /wd4996 /we4239 /we4002 /we4700 /we4305 /EHsc /MP)
    set(DEBUG_OPTIONS /MTd /Od /Zi)
    set(RELEASE_OPTIONS /MT /Ox /fp:fast /GL /GR-)

    add_definitions(-D_SCL_SECURE)
    add_definitions(-D_SECURE_SCL)
    add_definitions(-D_ALLOW_KEYWORD_MACROS)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
    add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)
    add_definitions(-DWINVER=0x0602)
    add_definitions(-D_WIN32_WINNT=0x0602)
    add_definitions(-DNTDDI_VERSION=0x06020000)

elseif(APPLE)
    set(COMMON_OPTIONS -DHAVE_PTHREAD -DGUID_CFUUID -lstdc++fs)
    set(DEBUG_OPTIONS -g)
    set(RELEASE_OPTIONS -Ofast )
else()
    set(COMMON_OPTIONS -DHAVE_PTHREAD  -lstdc++fs)
    set(DEBUG_OPTIONS -g)
    set(RELEASE_OPTIONS -O2)
endif()

set(DEBUG_OPTIONS ${DEBUG_OPTIONS} ${COMMON_OPTIONS})
set(RELEASE_OPTIONS ${RELEASE_OPTIONS} ${COMMON_OPTIONS})

add_compile_options(
        "$<$<CONFIG:RELEASE>:${RELEASE_OPTIONS}>"
        "$<$<CONFIG:DEBUG>:${DEBUG_OPTIONS}>")

if(MSVC)
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /DEBUG")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG /INCREMENTAL:NO")
endif()

add_definitions(-D__STDC_LIMIT_MACROS)
add_definitions(-D__STDC_FORMAT_MACROS)
add_definitions(-D__STDC_CONSTANT_MACROS)

add_definitions(-DHAVE_SSE42)

add_definitions(-D_HAS_EXCEPTIONS=0)

set (CMAKE_CXX_STANDARD 14)

set (CMAKE_C_FLAGS "-std=c99 ${CMAKE_C_FLAGS}")
set (CMAKE_C_FLAGS "-fPIC -g  ${CMAKE_C_FLAGS}")
set (CMAKE_CXX_FLAGS "-fPIC -g -fno-strict-aliasing -msse2 -msse3 -msse4 -mpopcnt -funsafe-math-optimizations ${CMAKE_CXX_FLAGS}")

include_directories(configs)

#####################################################################
# zlib
#####################################################################
file(GLOB ZLIB_SOURCES zlib/*.c zlib/*.h)
add_library(zlib_static STATIC ${ZLIB_SOURCES})
#####################################################################

#####################################################################
# libpng
#####################################################################
# file(GLOB LPNG_SOURCES libpng/*.c libpng/*.h)
# add_library(libpng STATIC ${LPNG_SOURCES})
add_subdirectory (libpng)
#####################################################################

#####################################################################
# libjpeg-turbo
#####################################################################
#
set(JPEG_TURBO_SOURCES jcapimin.c jcapistd.c jccoefct.c jccolor.c jcdctmgr.c jchuff.c
        jcicc.c jcinit.c jcmainct.c jcmarker.c jcmaster.c jcomapi.c jcparam.c
        jcphuff.c jcprepct.c jcsample.c jctrans.c jdapimin.c jdapistd.c jdatadst.c
        jdatasrc.c jdcoefct.c jdcolor.c jddctmgr.c jdhuff.c jdicc.c jdinput.c
        jdmainct.c jdmarker.c jdmaster.c jdmerge.c jdphuff.c jdpostct.c jdsample.c
        jdtrans.c jerror.c jfdctflt.c jfdctfst.c jfdctint.c jidctflt.c jidctfst.c
        jidctint.c jidctred.c jquant1.c jquant2.c jutils.c jmemmgr.c jmemnobs.c
        jaricom.c jcarith.c jdarith.c)

PREPEND(JPEG_TURBO_SOURCES libjpeg-turbo/ ${JPEG_TURBO_SOURCES})
add_subdirectory(libjpeg-turbo/simd)

add_library(jpeg-turbo STATIC ${JPEG_TURBO_SOURCES} $<TARGET_OBJECTS:simd> ${SIMD_OBJS})
target_compile_options(jpeg-turbo PRIVATE -DTURBO -Ofast)
target_compile_options(simd PRIVATE -DTURBO)
#####################################################################


#####################################################################
# libjpeg
#####################################################################

set(JPEG_SOURCES jmemnobs.c jaricom.c jcapimin.c jcapistd.c jcarith.c jccoefct.c jccolor.c
        jcdctmgr.c jchuff.c jcinit.c jcmainct.c jcmarker.c jcmaster.c jcomapi.c jcparam.c
        jcprepct.c jcsample.c jctrans.c jdapimin.c jdapistd.c jdarith.c jdatadst.c jdatasrc.c
        jdcoefct.c jdcolor.c jddctmgr.c jdhuff.c jdinput.c jdmainct.c jdmarker.c jdmaster.c
        jdmerge.c jdpostct.c jdsample.c jdtrans.c jerror.c jfdctflt.c jfdctfst.c jfdctint.c
        jidctflt.c jidctfst.c jidctint.c jquant1.c jquant2.c jutils.c jmemmgr.c cderror.h
        cdjpeg.h jdct.h jinclude.h jmemsys.h jpegint.h jversion.h transupp.h )

PREPEND(JPEG_SOURCES libjpeg/ ${JPEG_SOURCES})
add_library(jpeg STATIC ${JPEG_SOURCES})
target_compile_options(jpeg PRIVATE -DVANILA -Ofast)
#####################################################################


#####################################################################
# protobuf
#####################################################################
set(PROTOBUF any_lite.cc arena.cc extension_set.cc generated_enum_util.cc
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
        wire_format.cc  wrappers.pb.cc)

PREPEND(PROTOBUF protobuf/src/google/protobuf/ ${PROTOBUF})

add_library(protobuf STATIC ${PROTOBUF})
#####################################################################

#####################################################################
# lz4
#####################################################################
set(LZ4_DIR lz4/lib/)
set(SOURCES_LZ4 ${LZ4_DIR}lz4.c ${LZ4_DIR}lz4hc.c ${LZ4_DIR}lz4.h ${LZ4_DIR}lz4hc.h ${LZ4_DIR}xxhash.c ${LZ4_DIR}xxhash.h)
add_library(lz4 ${SOURCES_LZ4})
#####################################################################

#####################################################################
# crc32c
#####################################################################
file(GLOB CRC32C_SOURCES crc32c/src/*.cc crc32c/src/*.h)
add_library(crc32c STATIC ${CRC32C_SOURCES})
#####################################################################


#####################################################################
# fsal
#####################################################################
file(GLOB FSAL_SOURCES fsal/sources/*.cpp fsal/sources/*.h)
add_library(fsal STATIC ${FSAL_SOURCES})
#####################################################################

##############################################################
# Includes
##############################################################
include_directories(libs/lz4/lib)
include_directories(zlib)
include_directories(fsal/sources)
include_directories(libpng)
include_directories(pybind11/include)
include_directories(${CMAKE_BINARY_DIR}/libpng)
include_directories(protobuf/src)
include_directories(crc32c/include)
include_directories(${CMAKE_BINARY_DIR}/libjpeg-turbo)
include_directories(${PYTHON_INCLUDE_DIR})
include_directories(sources)
include_directories("${PROJECT_BINARY_DIR}")

link_directories("${PYTHON_LIBRARY_DIR}")

# Main sources
##############################################################
file(GLOB_RECURSE SOURCES sources/*.cpp sources/*.h sources/*.c sources/*.cc)
list(REMOVE_ITEM SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/sources/libjpeg-turbo_CU.cpp)

##############################################################
# Targets
##############################################################
add_library(dareblopy SHARED ${SOURCES})

##############################################################
# Linkage
##############################################################
set(LIBRARIES rt m  stdc++fs fsal jpeg jpeg-turbo png_static zlib_static protobuf crc32c ${PYTHON_LIBRARY})

target_link_libraries(dareblopy ${LIBRARIES})
target_link_libraries(fsal stdc++fs)
SET_TARGET_PROPERTIES(dareblopy PROPERTIES PREFIX "_")
