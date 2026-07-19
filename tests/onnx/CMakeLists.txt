# SPDX-FileCopyrightText: 2026 CERN
#
# SPDX-License-Identifier: Apache-2.0

message(STATUS "Adding ONNX Runtime tests")

file(
  COPY ${CMAKE_CURRENT_SOURCE_DIR}/identity_1x1.onnx
  DESTINATION ${CMAKE_CURRENT_BINARY_DIR}
)

add_executable(coopsync_tbb_onnx_wait_for_test wait_for_test.cpp)

target_link_libraries(
  coopsync_tbb_onnx_wait_for_test
  PRIVATE GTest::gtest_main coopsync_tbb onnxruntime::onnxruntime
)

gtest_discover_tests(coopsync_tbb_onnx_wait_for_test)
