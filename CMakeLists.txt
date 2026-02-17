cmake_minimum_required(VERSION 3.25)
project(
  "CoopSync_TBB"
  VERSION 0.1.0
  LANGUAGES CXX
  DESCRIPTION "Cooperative synchronization primitives for TBB resumable tasks")

# Common options

option(COOPSYNC_TBB_USE_SYSTEM_LIBS "Use system libraries be default" FALSE)
option(COOPSYNC_TBB_BUILD_TESTING "Build the tests of CoopSync_TBB" TRUE)
option(COOPSYNC_TBB_BUILD_DOCS "Build the documentation of CoopSync_TBB" OFF)

# Sanitizers (used mainly in CI via CMake presets)

set(COOPSYNC_TBB_SANITIZER
  "none"
  CACHE STRING "Enable one sanitizer: none;address;undefined;thread")
set_property(CACHE COOPSYNC_TBB_SANITIZER PROPERTY STRINGS none address undefined thread)

# Common config

include(GNUInstallDirs)

# Set up TBB.

option(COOPSYNC_TBB_SETUP_TBB "Set up the TBB target(s) explicitly" TRUE)
if(COOPSYNC_TBB_USE_SYSTEM_LIBS OR COOPSYNC_TBB_USE_SPACK_LIBS)
  set(COOPSYNC_TBB_USE_SYSTEM_TBB_DEFAULT ON)
else()
  set(COOPSYNC_TBB_USE_SYSTEM_TBB_DEFAULT OFF)
endif()
option(COOPSYNC_TBB_USE_SYSTEM_TBB
       "Pick up an existing installation of TBB from the build environment"
       ${COOPSYNC_TBB_USE_SYSTEM_TBB_DEFAULT})
unset(COOPSYNC_TBB_USE_SYSTEM_TBB_DEFAULT)
if(COOPSYNC_TBB_SETUP_TBB)
  if(COOPSYNC_TBB_USE_SYSTEM_TBB)
    find_package(TBB REQUIRED 2021.8)
  else()
    add_subdirectory(extern/tbb)
  endif()
endif()

# Set up GoogleTest.

include(CTest)
if(BUILD_TESTING AND COOPSYNC_TBB_BUILD_TESTING)
  set(COOPSYNC_TBB_DEFAULT_SETUP_GOOGLETEST TRUE)
endif()
option(COOPSYNC_TBB_SETUP_GOOGLETEST
       "Set up the GoogleTest target(s) explicitly"
       ${COOPSYNC_TBB_DEFAULT_SETUP_GOOGLETEST})
if(COOPSYNC_TBB_USE_SYSTEM_LIBS OR COOPSYNC_TBB_USE_SPACK_LIBS)
  set(COOPSYNC_TBB_USE_SYSTEM_GOOGLETEST_DEFAULT ON)
else()
  set(COOPSYNC_TBB_USE_SYSTEM_GOOGLETEST_DEFAULT OFF)
endif()
option(
  COOPSYNC_TBB_USE_SYSTEM_GOOGLETEST
  "Pick up an existing installation of GoogleTest from the build environment"
  ${COOPSYNC_TBB_USE_SYSTEM_GOOGLETEST_DEFAULT})
unset(COOPSYNC_TBB_USE_SYSTEM_GOOGLETEST_DEFAULT)
if(COOPSYNC_TBB_SETUP_GOOGLETEST)
  if(COOPSYNC_TBB_USE_SYSTEM_GOOGLETEST)
    find_package(GTest REQUIRED)
  else()
    add_subdirectory(extern/googletest)
  endif()
endif()

# Library

add_library(CoopSync_TBB INTERFACE)

if(NOT COOPSYNC_TBB_SANITIZER STREQUAL "none")
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "^(Clang|GNU)$")
    message(
      FATAL_ERROR
        "COOPSYNC_TBB_SANITIZER requires Clang or GCC (got '${CMAKE_CXX_COMPILER_ID}')."
    )
  endif()

  if(COOPSYNC_TBB_SANITIZER STREQUAL "address")
    set(_coopsync_tbb_sanitize_flag "address")
  elseif(COOPSYNC_TBB_SANITIZER STREQUAL "undefined")
    set(_coopsync_tbb_sanitize_flag "undefined")
  elseif(COOPSYNC_TBB_SANITIZER STREQUAL "thread")
    set(_coopsync_tbb_sanitize_flag "thread")
  else()
    message(
      FATAL_ERROR
        "Invalid COOPSYNC_TBB_SANITIZER='${COOPSYNC_TBB_SANITIZER}'. Use one of: none, address, undefined, thread."
    )
  endif()

  target_compile_options(CoopSync_TBB INTERFACE -fsanitize=${_coopsync_tbb_sanitize_flag}
                                            -fno-omit-frame-pointer)
  target_link_options(CoopSync_TBB INTERFACE -fsanitize=${_coopsync_tbb_sanitize_flag})

  if(COOPSYNC_TBB_SANITIZER STREQUAL "undefined")
    target_compile_options(CoopSync_TBB INTERFACE -fno-sanitize-recover=all)
  endif()

  unset(_coopsync_tbb_sanitize_flag)
endif()

target_sources(
  CoopSync_TBB
  INTERFACE FILE_SET
            HEADERS
            BASE_DIRS
            include/
            FILES
            include/coopsync_tbb/coopsync_tbb.hpp
            include/coopsync_tbb/detail/intrusive_list.hpp
            include/coopsync_tbb/latch.hpp
            include/coopsync_tbb/mutex.hpp
            include/coopsync_tbb/scoped_lock.hpp
            include/coopsync_tbb/cuda/wait_for.hpp
            include/coopsync_tbb/hip/wait_for.hpp)

add_library(CoopSync_TBB::CoopSync_TBB ALIAS CoopSync_TBB)
target_link_libraries(CoopSync_TBB INTERFACE TBB::tbb)

# Tests

if(BUILD_TESTING AND COOPSYNC_TBB_BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()

# Docs

if(COOPSYNC_TBB_BUILD_DOCS)
  add_subdirectory(docs)
endif()

# Install

install(
  TARGETS CoopSync_TBB
  EXPORT CoopSync_TBBTargets
  FILE_SET HEADERS)

install(
  EXPORT CoopSync_TBBTargets
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CoopSync_TBB
  NAMESPACE CoopSync_TBB::)

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/CoopSync_TBBConfigVersion.cmake
  COMPATIBILITY ExactVersion)

install(FILES cmake/CoopSync_TBBConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/CoopSync_TBBConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CoopSync_TBB)
