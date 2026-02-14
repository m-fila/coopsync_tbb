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

add_library(CoopSync_TBB INTERFACE include/coopsync_tbb/cuda/wait_for.hpp)
add_library(CoopSync_TBB::CoopSync_TBB ALIAS CoopSync_TBB)
target_include_directories(
  CoopSync_TBB INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
                         $<INSTALL_INTERFACE:include/>)
target_link_libraries(CoopSync_TBB INTERFACE TBB::tbb)

# Tests

if(BUILD_TESTING AND COOPSYNC_TBB_BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()
