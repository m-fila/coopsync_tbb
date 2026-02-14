include(GoogleTest)

find_package(CUDAToolkit QUIET)
option(COOPSYNC_TBB_BUILD_CUDA_TESTING "Enable CUDA tests" ${CUDAToolkit_FOUND})
if(COOPSYNC_TBB_BUILD_CUDA_TESTING)
  add_subdirectory(cuda)
endif()
