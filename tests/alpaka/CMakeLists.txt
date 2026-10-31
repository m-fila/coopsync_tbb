# SPDX-FileCopyrightText: 2026 CERN
#
# SPDX-License-Identifier: Apache-2.0

message(STATUS "Adding alpaka tests")

# Setup alpaka
set(alpaka_DEP_OMP OFF CACHE BOOL "Enable the alpaka OpenMP backend") # disable OpenMP backend to await clashes with HIP
set(alpaka_DEP_TBB ON CACHE BOOL "Enable the alpaka TBB backend")
if(${CUDAToolkit_FOUND})
  set(alpaka_DEP_CUDA ON CACHE BOOL "Enable the alpaka CUDA backend")
endif()
if(${HIP_FOUND})
  set(alpaka_DEP_HIP ON CACHE BOOL "Enable the alpaka HIP backend")
endif()
set(
  alpaka_SUPPRESS_TARGET_WARNING
  ON
  CACHE BOOL
  "Suppress warning about target linking"
)

option(
  COOPSYNC_TBB_SETUP_ALPAKA
  "Set up the alpaka target(s) explicitly"
  ${COOPSYNC_TBB_BUILD_ALPAKA_TESTING}
)
if(COOPSYNC_TBB_USE_SYSTEM_LIBS)
  set(COOPSYNC_TBB_USE_SYSTEM_ALPAKA_DEFAULT ON)
else()
  set(COOPSYNC_TBB_USE_SYSTEM_ALPAKA_DEFAULT OFF)
endif()
option(
  COOPSYNC_TBB_USE_SYSTEM_ALPAKA
  "Pick up an existing installation of ALPAKA from the build environment"
  ${COOPSYNC_TBB_USE_SYSTEM_ALPAKA_DEFAULT}
)
unset(COOPSYNC_TBB_USE_SYSTEM_ALPAKA_DEFAULT)
if(COOPSYNC_TBB_SETUP_ALPAKA)
  if(COOPSYNC_TBB_USE_SYSTEM_ALPAKA)
    find_package(GTest REQUIRED)
  else()
    add_subdirectory(
      "${CMAKE_CURRENT_SOURCE_DIR}/../../extern/alpaka"
      "${CMAKE_CURRENT_BINARY_DIR}/../../extern/alpaka"
    )
  endif()
endif()

function(add_coopsync_tbb_alpaka_test target)
  add_executable(${target} wait_for_test.cpp)
  target_link_libraries(
    ${target}
    PRIVATE GTest::gtest_main coopsync_tbb alpaka::alpaka ${ARGN}
  )
  alpaka_finalize(${target})
  gtest_discover_tests(${target})
endfunction()

add_coopsync_tbb_alpaka_test(coopsync_tbb_alpaka_test)

if(alpaka_DEP_CUDA)
  add_coopsync_tbb_alpaka_test(
    coopsync_tbb_alpaka_cuda_test alpaka::cuda
  )
endif()

if(alpaka_DEP_HIP)
  add_coopsync_tbb_alpaka_test(
    coopsync_tbb_alpaka_hip_test alpaka::hip
  )
endif()
