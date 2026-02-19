#pragma once
#include <cuda_runtime_api.h>
#include <oneapi/tbb/task.h>

// clang-format off
#ifndef COOPSYNC_TBB_CUDA_NODISCARD
  #if defined(__has_cpp_attribute)
    #if __has_cpp_attribute(nodiscard)
      #define COOPSYNC_TBB_CUDA_NODISCARD [[nodiscard]]
    #endif
  #elif defined(__cplusplus) && __cplusplus >= 201703L
    #define COOPSYNC_TBB_CUDA_NODISCARD [[nodiscard]]
  #endif
  #ifndef COOPSYNC_TBB_CUDA_NODISCARD
    #define COOPSYNC_TBB_CUDA_NODISCARD
  #endif
#endif
// clang-format on

/// @brief CUDA integration.
namespace coopsync_tbb::cuda {

namespace detail {
/// @brief CUDA callback to resume a suspended TBB task.
/// @param tag Pointer to the suspend point of the task to resume.
///
static inline void resumption_callback(void* tag) {
    tbb::task::resume(*static_cast<tbb::task::suspend_point*>(tag));
}
}  // namespace detail

/// @brief Suspends the current TBB task until all the work in a CUDA stream
/// completes. Internally a CUDA callback is used to resume the task.
/// @param stream CUDA stream.
/// @return The CUDA error code.
/// @note In case of error during callback setup, the task is resumed
/// immediately.
///
COOPSYNC_TBB_CUDA_NODISCARD static inline cudaError_t wait_for(
    cudaStream_t stream) {
    auto suspend_point = tbb::task::suspend_point{};
    auto err = cudaSuccess;
    tbb::task::suspend(
        [stream, &err, &suspend_point](tbb::task::suspend_point tag) {
            suspend_point = tag;
            // Note: cudaStreamAddCallback is pending for deprecation, using
            // cudaLaunchHostFunc instead
            err = cudaLaunchHostFunc(stream, detail::resumption_callback,
                                     &suspend_point);
            if (err != cudaSuccess) {
                detail::resumption_callback(&suspend_point);
            }
        });
    return err;
}

}  // namespace coopsync_tbb::cuda

#undef COOPSYNC_TBB_CUDA_NODISCARD
