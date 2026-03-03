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

include(cmake/Sanitizers.cmake) # COOPSYNC_TBB_SANITIZER option

# Common config

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(CMAKE_CXX_STANDARD LESS 17)
  message(FATAL_ERROR "CoopsSync_TBB requires C++17 or higher.")
endif()
include(GNUInstallDirs)

# Set up TBB.

option(COOPSYNC_TBB_SETUP_TBB "Set up the TBB target(s) explicitly" TRUE)
if(COOPSYNC_TBB_USE_SYSTEM_LIBS)
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
if(COOPSYNC_TBB_USE_SYSTEM_LIBS)
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

add_library(CoopSync_TBB SHARED)

coopsync_tbb_apply_sanitizer(CoopSync_TBB)

target_sources(
  CoopSync_TBB
  PUBLIC FILE_SET
         HEADERS
         BASE_DIRS
         include/
         FILES
         # public API
         include/coopsync_tbb/coopsync_tbb.hpp
         # details
         include/coopsync_tbb/detail/intrusive_list.hpp
         include/coopsync_tbb/detail/macros.hpp
         include/coopsync_tbb/detail/unique_scoped_lock.hpp
         include/coopsync_tbb/detail/wait_queue.hpp
         # primitives
         include/coopsync_tbb/barrier.hpp
         include/coopsync_tbb/condition_variable.hpp
         include/coopsync_tbb/future.hpp
         include/coopsync_tbb/latch.hpp
         include/coopsync_tbb/mutex.hpp
         include/coopsync_tbb/shared_mutex.hpp
         include/coopsync_tbb/semaphore.hpp
         # integrations
         include/coopsync_tbb/cuda/wait_for.hpp
         include/coopsync_tbb/hip/wait_for.hpp
  PRIVATE # public API
          src/coopsync_tbb.cpp
          # details
          src/intrusive_list.cpp
          src/macros.cpp
          src/unique_scoped_lock.cpp
          src/wait_queue.cpp
          # primitives
          src/barrier.cpp
          src/condition_variable.cpp
          src/future.cpp
          src/latch.cpp
          src/mutex.cpp
          src/semaphore.cpp
          src/shared_mutex.cpp)

add_library(CoopSync_TBB::CoopSync_TBB ALIAS CoopSync_TBB)
target_link_libraries(CoopSync_TBB PUBLIC TBB::tbb)

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
