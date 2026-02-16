cmake_minimum_required(VERSION 3.25)

project(
  CoopSync_TBB_Downstream
  VERSION 0.1.0
  LANGUAGES CXX
  DESCRIPTION "Downstream example consuming CoopSync_TBB")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

find_package(CoopSync_TBB CONFIG REQUIRED)

add_executable(coopsync_tbb_downstream main.cpp)
target_link_libraries(coopsync_tbb_downstream
                      PRIVATE CoopSync_TBB::CoopSync_TBB)

if(BUILD_TESTING)
  enable_testing()
  add_test(NAME downstream_test COMMAND coopsync_tbb_downstream)
endif()
