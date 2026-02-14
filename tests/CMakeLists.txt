include(GoogleTest)

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
