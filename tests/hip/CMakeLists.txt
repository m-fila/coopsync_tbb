message(STATUS "Adding HIP tests")

enable_language(HIP)

add_executable(coopsync_tbb_hip_test wait_for_test.cpp nanospin.hpp
                                      nanospin.hip)

target_link_libraries(coopsync_tbb_hip_test PRIVATE GTest::gtest_main
                                                     CoopSync_TBB hip::host)
gtest_discover_tests(coopsync_tbb_hip_test)
