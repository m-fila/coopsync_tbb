# SPDX-FileCopyrightText: 2026 CERN
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.25)

project(
  CoopSync_TBB_Downstream
  VERSION 0.1.0
  LANGUAGES CXX
  DESCRIPTION "Downstream example consuming CoopSync_TBB")

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(DOWNSTREAM_USE_MODULES "Use CoopSync_TBB as a C++20 module" OFF)

include(CTest)

find_package(CoopSync_TBB CONFIG REQUIRED)

add_executable(coopsync_tbb_downstream main.cpp)
target_link_libraries(coopsync_tbb_downstream
                      PRIVATE CoopSync_TBB::CoopSync_TBB)

if(DOWNSTREAM_USE_MODULES)
  set(CMAKE_CXX_SCAN_FOR_MODULES ON)
  add_executable(coopsync_tbb_downstream_module main.cpp)
  target_link_libraries(coopsync_tbb_downstream_module
                        PRIVATE CoopSync_TBB::CoopSync_TBB)
  target_compile_definitions(coopsync_tbb_downstream_module
                             PRIVATE USE_COOPSYNC_TBB_MODULE)
endif()

if(BUILD_TESTING)
  enable_testing()
  add_test(NAME downstream_test COMMAND coopsync_tbb_downstream)
  if(DOWNSTREAM_USE_MODULES)
    add_test(NAME downstream_test_module COMMAND coopsync_tbb_downstream_module)
  endif()
endif()
