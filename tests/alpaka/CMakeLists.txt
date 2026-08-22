# SPDX-FileCopyrightText: 2026 CERN
#
# SPDX-License-Identifier: Apache-2.0

message(STATUS "Adding alpaka tests")

# Setup alpaka

# Automatically enable the CUDA and HIP backends if they were already found.
# If neither backend was explicitly selected, prefer CUDA when available,
# otherwise use HIP when available. Explicit user settings are preserved.
if(
  NOT DEFINED alpaka_ACC_GPU_CUDA_ENABLE
  AND NOT DEFINED alpaka_ACC_GPU_HIP_ENABLE
)
  if(${CUDAToolkit_FOUND})
    set(
      alpaka_ACC_GPU_CUDA_ENABLE
      ON
      CACHE BOOL
      "Enable the alpaka CUDA backend"
    )
    set(
      alpaka_ACC_GPU_HIP_ENABLE
      OFF
      CACHE BOOL
      "Enable the alpaka HIP backend"
    )
  elseif(${HIP_FOUND})
    set(
      alpaka_ACC_GPU_CUDA_ENABLE
      OFF
      CACHE BOOL
      "Enable the alpaka CUDA backend"
    )
    set(alpaka_ACC_GPU_HIP_ENABLE ON CACHE BOOL "Enable the alpaka HIP backend")
  endif()
endif()

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

alpaka_add_executable(coopsync_tbb_alpaka_test wait_for_test.cpp)

target_link_libraries(
  coopsync_tbb_alpaka_test
  PRIVATE GTest::gtest_main CoopSync_TBB alpaka::alpaka
)
gtest_discover_tests(coopsync_tbb_alpaka_test)
