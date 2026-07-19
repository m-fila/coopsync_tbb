# SPDX-FileCopyrightText: 2026 CERN
#
# SPDX-License-Identifier: Apache-2.0

include(GoogleTest)

add_executable(
  coopsync_tbb_test
  # extras
  mutex_traits.hpp
  # details
  intrusive_list_test.cpp
  # main classes
  atomic_condition_test.cpp
  atomic_ref_condition_test.cpp
  atomic_flag_test.cpp
  barrier_test.cpp
  condition_variable_test.cpp
  future_test.cpp
  latch_test.cpp
  mutex_test.cpp
  shared_mutex_test.cpp
  semaphore_test.cpp
)

target_link_libraries(coopsync_tbb_test PRIVATE GTest::gtest_main coopsync_tbb)

gtest_discover_tests(coopsync_tbb_test)

# CUDA

find_package(CUDAToolkit QUIET)
option(COOPSYNC_TBB_BUILD_CUDA_TESTING "Enable CUDA tests" ${CUDAToolkit_FOUND})
if(COOPSYNC_TBB_BUILD_CUDA_TESTING)
  add_subdirectory(cuda)
endif()

# HIP

find_package(hip QUIET)
option(COOPSYNC_TBB_BUILD_HIP_TESTING "Enable HIP tests" ${hip_FOUND})
if(COOPSYNC_TBB_BUILD_HIP_TESTING)
  add_subdirectory(hip)
endif()

# ONNX Runtime

find_package(onnxruntime QUIET)
option(
  COOPSYNC_TBB_BUILD_ONNX_TESTING
  "Enable ONNX Runtime tests"
  ${onnxruntime_FOUND}
)
if(COOPSYNC_TBB_BUILD_ONNX_TESTING)
  add_subdirectory(onnx)
endif()
