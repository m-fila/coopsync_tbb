include(GoogleTest)

add_executable(
  coopsync_tbb_test
  # details
  intrusive_list_test.cpp
  # main classes
  latch_test.cpp)

target_link_libraries(coopsync_tbb_test PRIVATE GTest::gtest_main CoopSync_TBB)

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
