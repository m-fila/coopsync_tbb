message(STATUS "Adding CUDA tests")

enable_language(CUDA)

add_executable(coopsync_tbb_cuda_test wait_for_test.cpp nanospin.hpp
                                      nanospin.cu)

target_link_libraries(coopsync_tbb_cuda_test PRIVATE GTest::gtest_main
                                                     CoopSync_TBB CUDA::cudart)
gtest_discover_tests(coopsync_tbb_cuda_test)
