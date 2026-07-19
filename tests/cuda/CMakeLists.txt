# SPDX-FileCopyrightText: 2026 CERN
#
# SPDX-License-Identifier: Apache-2.0

message(STATUS "Adding CUDA tests")

enable_language(CUDA)

add_executable(coopsync_tbb_cuda_test wait_for_test.cpp nanospin.hpp
                                      nanospin.cu)

target_link_libraries(coopsync_tbb_cuda_test PRIVATE GTest::gtest_main
                                                     coopsync_tbb CUDA::cudart)
gtest_discover_tests(coopsync_tbb_cuda_test)
